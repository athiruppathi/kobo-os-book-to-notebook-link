// Pinned to the inspected ARM firmware. No hooks or raw notebook/database writes.
#include <NickelHook.h>
#include <QApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPointer>
#include <QSaveFile>
#include <QSocketNotifier>
#include <QThread>
#include <QVector>
#include <QUuid>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <stdexcept>

static const char *ROOT = "/mnt/onboard/.adds/kobo-os/";
static const char *FIFO = "/tmp/kobo-os-book-links.fifo";

// find() at 0xa96b42/0xa96ba0 constructs two words in the hidden return buffer.
// Only native code constructs, copies, dereferences, or destroys this storage.
struct VolumeStorage { void *words[2]; };
static_assert(sizeof(VolumeStorage) == 8, "Requires 32-bit ARM");

struct Api {
    void *(*window)();
    QWidget *(*view)(void *);
    QWidget *(*underneath)(void *);
    const VolumeStorage *(*readerVolume)(const void *);
    const QByteArray &(*contentId)(const VolumeStorage *);
    const QString &(*externalId)(const VolumeStorage *);
    const void *(*device)();
    // Explicit ARM hidden return pointer; this is NOT a normal C++ Volume return.
    void (*lookup)(VolumeStorage *, const QString &, const void *);
    void (*destroyVolume)(VolumeStorage *);
    bool (*valid)(const VolumeStorage *);
    bool (*basic)(const VolumeStorage *);
    QObject *(*createReader)(void *, const VolumeStorage *, const void *, void *);
    void (*openNotebook)(QObject *, const VolumeStorage *);

    template<class T> void bind(T &target, const char *name) {
        target = reinterpret_cast<T>(dlsym(RTLD_DEFAULT, name));
        if (!target) throw std::runtime_error("Required native symbol is unavailable.");
    }
    Api() {
        bind(window, "_ZN20MainWindowController14sharedInstanceEv");
        bind(view, "_ZNK20MainWindowController11currentViewEv");
        bind(underneath, "_ZNK20MainWindowController25peekUnderneathCurrentViewEv");
        bind(readerVolume, "_ZNK11ReadingView9getVolumeEv");
        bind(contentId, "_ZNK7Content7idBytesEv");
        bind(externalId, "_ZNK7Content13getExternalIdEv");
        bind(device, "_ZN6Device16getCurrentDeviceEv");
        bind(lookup, "_ZN13VolumeManager21getNotebookByObjectIdERK7QStringRK6Device");
        bind(destroyVolume, "_ZN6VolumeD1Ev");
        bind(valid, "_ZNK7Content7isValidEv");
        bind(basic, "_ZNK6Volume15isBasicNotebookEv");
        bind(createReader, "_ZN16ReadingViewMixin12createReaderERK6VolumeRK7QVectorI10ContentKeyEP25AbstractLibraryControllerIS0_E");
        bind(openNotebook, "_ZN14N3ReaderOpener11openNotePadERK6Volume");
    }
};

static QString path(const char *name) { return QString::fromLatin1(ROOT) + QString::fromLatin1(name); }
static bool uuid(const QString &s) {
    return s.size() == 36 && !QUuid(s).isNull()
        && QUuid(s).toString().mid(1, 36).compare(s, Qt::CaseInsensitive) == 0;
}
static QJsonObject readJson(const QString &name, bool allowMissing = false) {
    QFile file(name);
    if (allowMissing && !file.exists()) {
        QJsonObject empty;
        empty.insert("version", 1);
        empty.insert("links", QJsonObject());
        return empty;
    }
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
        throw std::runtime_error("Cannot read configuration (missing, unreadable, or too large).");
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()
        || doc.object().value("version").toDouble(-1) != 1)
        throw std::runtime_error("Unsupported configuration. Existing file was preserved.");
    return doc.object();
}
static void writeJson(const QString &name, const QJsonObject &object) {
    QSaveFile file(name);
    file.setDirectWriteFallback(false);
    QByteArray bytes = QJsonDocument(object).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        throw std::runtime_error("Cannot save configuration atomically.");
}
static QJsonObject linksState() {
    QJsonObject state = readJson(path("book-links.json"), true);
    if (!state.value("links").isObject()) throw std::runtime_error("Invalid links file.");
    QJsonObject links = state.value("links").toObject();
    for (auto it = links.begin(); it != links.end(); ++it)
        if (it.key().trimmed().isEmpty() || !it.value().isString() || !uuid(it.value().toString()))
            throw std::runtime_error("Invalid book/notebook identity in links file.");
    return state;
}

class Notebook {
    Api &api;
    VolumeStorage storage;
public:
    Notebook(Api &a, const QString &id) : api(a) {
        if (!uuid(id)) throw std::runtime_error("Invalid notebook identity.");
        const void *device = api.device();
        if (!device) throw std::runtime_error("Current device is unavailable.");
        api.lookup(&storage, id, device);
        // Lookup constructs even its no-match result; destroy that result too.
        if (!api.valid(&storage) || !api.basic(&storage) || api.externalId(&storage) != id) {
            api.destroyVolume(&storage);
            throw std::runtime_error("Linked basic notebook is unavailable. Refresh the notebook list over USB.");
        }
    }
    ~Notebook() { api.destroyVolume(&storage); }
    const VolumeStorage *get() const { return &storage; }
    Notebook(const Notebook &) = delete;
    Notebook &operator=(const Notebook &) = delete;
};

class Bridge : public QObject {
    Api api;
    int fd;
    QByteArray buffer;
    bool busy = false;
    bool discardLine = false;
    QPointer<QWidget> reader;
    QPointer<QWidget> notebookView;
    QPointer<QObject> opener;

    QWidget *currentReader() {
        void *window = api.window();
        QWidget *view = window ? api.view(window) : nullptr;
        // Do not cast arbitrary visible widgets or trust only a mutable objectName.
        if (!view || !view->inherits("ReadingView"))
            throw std::runtime_error("Open a book first, then use its reading menu.");
        return view;
    }
    QString bookId(QWidget *view) {
        const VolumeStorage *volume = api.readerVolume(view);
        if (!volume || !api.valid(volume)) throw std::runtime_error("Active book is unavailable.");
        QByteArray bytes = api.contentId(volume);
        QString id = QString::fromUtf8(bytes.constData(), bytes.size());
        if (id.trimmed().isEmpty() || id.toUtf8() != bytes)
            throw std::runtime_error("Book identity is not supported.");
        return id;
    }
    void report(const char *result) {
        QJsonObject object;
        object.insert("version", 1);
        object.insert("result", QString::fromLatin1(result));
        // Never include book IDs, notebook IDs, titles, or paths in reports.
        writeJson(path("status.json"), object);
    }
    void link(QWidget *view, const QString &book) {
        QJsonObject catalog = readJson(path("notebooks.json"));
        if (!catalog.value("notebooks").isArray()) throw std::runtime_error("Invalid notebook list.");
        QJsonArray rows = catalog.value("notebooks").toArray();
        QStringList labels, ids;
        for (const QJsonValue &value : rows) {
            QJsonObject row = value.toObject();
            QString id = row.value("notebookId").toString();
            QString title = row.value("title").toString();
            if (!uuid(id) || title.trimmed().isEmpty() || ids.contains(id))
                throw std::runtime_error("Invalid or duplicate notebook list entry.");
            ids.append(id);
            labels.append(QString::number(ids.size()) + ". " + title);
        }
        if (labels.isEmpty()) throw std::runtime_error("No notebooks available. Refresh the notebook list over USB.");
        QPointer<QWidget> original(view);
        bool ok = false;
        QString choice = QInputDialog::getItem(view, "Link notebook", "Notebook for this book:", labels, 0, false, &ok);
        if (!ok) return;
        if (!original || currentReader() != original || bookId(original) != book)
            throw std::runtime_error("The active book changed. Please try again.");
        int index = labels.indexOf(choice);
        if (index < 0) return;
        Notebook notebook(api, ids.at(index));
        QJsonObject state = linksState();
        QJsonObject links = state.value("links").toObject();
        links.insert(book, ids.at(index));
        state.insert("links", links);
        writeJson(path("book-links.json"), state);
        report("linked-and-resolved");
        QMessageBox::information(view, "Book notebook", "Notebook linked to this book.");
    }
    void run(const QByteArray &command) {
        if (!QFile::exists(path("enabled"))) return;
        if (busy) return;
        busy = true;
        try {
            QWidget *view = currentReader();
            QString book = bookId(view);
            if (command == "link") link(view, book);
            else if (command == "check" || command == "open") {
                QString id = linksState().value("links").toObject().value(book).toString();
                if (id.isEmpty()) throw std::runtime_error("No notebook linked. Choose Link notebook first.");
                Notebook notebook(api, id);
                if (command == "check") {
                    report("reader-and-notebook-validated");
                    QMessageBox::information(view, "Book notebook", "Current book and linked basic notebook validated.");
                } else {
                    if (!QFile::exists(path("navigation.enabled")))
                        throw std::runtime_error("Opening is disabled during the first native lookup trial.");
                    if (opener) {
                        if (reader == view && !notebookView.isNull())
                            throw std::runtime_error("Previous notebook view is still active.");
                        // A resource download may still own callbacks. Do not guess completion.
                        throw std::runtime_error("An opener already exists. Restart before another opening trial.");
                    }
                    // Native createReader ignores its mixin argument on this exact firmware.
                    // Its QVector parameter is read-only. A default empty QVector has the
                    // common Qt shared-null header; no ContentKey element is fabricated.
                    const QVector<quint64> emptyKeys;
                    QObject *object = api.createReader(nullptr, notebook.get(), &emptyKeys, nullptr);
                    if (!object) throw std::runtime_error("Native opener creation failed.");
                    object->setParent(view);
                    reader = view;
                    opener = object;
                    api.openNotebook(object, notebook.get());
                    void *window = api.window();
                    QWidget *after = api.view(window);
                    if (after != view && api.underneath(window) == view) {
                        notebookView = after;
                        report("view-pushed-reader-retained");
                    } else report("open-requested-transition-unconfirmed");
                    // Retain the QObject for asynchronous bundle/font dialogs. Do not
                    // manually pop a controller or bypass native notebook save/close.
                }
            } else throw std::runtime_error("Unknown book-notebook command.");
        } catch (const std::exception &error) {
            nh_log("book-notebook action did not complete");
            QMessageBox::warning(nullptr, "Book notebook", QString::fromUtf8(error.what()));
        } catch (...) {
            nh_log("book-notebook action failed with native exception");
        }
        busy = false;
    }
public:
    explicit Bridge(QObject *parent) : QObject(parent), fd(-1) {
        struct stat st;
        if (lstat(FIFO, &st) == 0) {
            if (!S_ISFIFO(st.st_mode) || st.st_uid != geteuid() || (st.st_mode & 0777) != 0600)
                throw std::runtime_error("Unsafe command FIFO.");
        } else if (errno != ENOENT || mkfifo(FIFO, 0600) != 0)
            throw std::runtime_error("Cannot create command FIFO.");
        fd = ::open(FIFO, O_RDWR | O_NONBLOCK | O_NOFOLLOW);
        if (fd < 0) throw std::runtime_error("Cannot open command FIFO.");
        QSocketNotifier *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        QObject::connect(notifier, &QSocketNotifier::activated, this, [this](int) {
            char bytes[128];
            // Bound each wakeup; no polling timer, shell execution, or unbounded input.
            ssize_t count = ::read(fd, bytes, sizeof(bytes));
            for (ssize_t i = 0; i < count; ++i) {
                if (bytes[i] == '\n') {
                    QByteArray command = buffer;
                    buffer.clear();
                    bool discard = discardLine;
                    discardLine = false;
                    if (!discard && !command.isEmpty()) run(command);
                } else if (!discardLine) {
                    buffer.append(bytes[i]);
                    if (buffer.size() > 16) { buffer.clear(); discardLine = true; }
                }
            }
        });
        try { report("native-bridge-ready"); }
        catch (...) { ::close(fd); fd = -1; throw; }
    }
    ~Bridge() { if (fd >= 0) ::close(fd); }
};

static int init() {
    if (!QFile::exists(path("enabled"))) return 0;
    QApplication *app = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app || QThread::currentThread() != app->thread()) {
        nh_log("No GUI-thread application; bridge disabled");
        return 0;
    }
    QFile library("/usr/local/Kobo/libnickel.so.1.0.0");
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!library.open(QIODevice::ReadOnly) || !hash.addData(&library)
        || hash.result().toHex() != "25ec19024666dca7f134335af311593a21cfbd75a0235f1b77c27d9a28f733dd") {
        nh_log("Native library hash mismatch; bridge disabled");
        return 0;
    }
    try { new Bridge(app); }
    catch (...) { nh_log("Bridge initialization failed; no navigation enabled"); }
    return 0;
}

static struct nh_info info = {
    .name = "KoboBookLinks",
    .desc = "Book links with a firmware-pinned native notebook adapter",
    .uninstall_flag = "/mnt/onboard/.adds/kobo-os-links.uninstall",
};
NickelHook(.init = init, .info = &info)
