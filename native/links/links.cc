// Pinned to the inspected ARM firmware. Swipe call sites patched only in memory.
#include <NickelHook.h>
#include <QApplication>
#include "native_ui.h"
#include "live_catalog.h"
#include "page_swipes.h"
#include <QMap>
#include <QSet>
#include <algorithm>
#include <QMenu>
#include <QWidgetAction>
#include <QTimer>
#include <QEvent>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
    const QString &(*dbName)(const void *);
    void (*lookupBook)(VolumeStorage *, const QString &, const QString &);
    QString (*title)(const VolumeStorage *);
    bool (*isNotebook)(const VolumeStorage *);
    void (*bookmarkCtor)(VolumeStorage *);
    void (*bookmarkDtor)(VolumeStorage *);
    void (*openBook)(QObject *, const VolumeStorage *);

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
        bind(dbName, "_ZNK6Device9getDbNameEv");
        bind(lookupBook, "_ZN13VolumeManager7getByIdERK7QStringS2_");
        bind(title, "_ZNK7Content8getTitleEv");
        bind(isNotebook, "_ZNK7Content10isNotebookEv");
        bind(bookmarkCtor, "_ZN8BookmarkC1Ev");
        bind(bookmarkDtor, "_ZN8BookmarkD1Ev");
        bind(openBook, "_ZN14N3ReaderOpener10openReaderERK8Bookmark");
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
    NativeUi ui;
    int fd;
    QByteArray buffer;
    bool busy = false;
    bool discardLine = false;
    QPointer<QWidget> reader;
    QPointer<QWidget> notebookView;
    QPointer<QObject> opener;
    void *noteLibrary = nullptr;
    void (*noteVolume)(VolumeStorage *, const QWidget *) = nullptr;
    void (*noteQuit)(QWidget *) = nullptr;
    bool reverseQueued = false;
    QPointer<QTimer> lastPageTimer;
    int (*noteMaximum)(const QWidget *) = nullptr;
    int (*notePosition)(const QWidget *) = nullptr;
    void (*noteScroll)(QWidget *, int) = nullptr;

    void bindNotebookApi() {
        if (noteVolume && noteQuit) return;
        QFile library("/usr/local/Kobo/libiinknote.so");
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!library.open(QIODevice::ReadOnly) || !hash.addData(&library)
            || hash.result().toHex() != "2fa70e17773c645d659ff10eefc3692ffe474feb53a9b4b43d964917653c6b7c")
            throw std::runtime_error("Unsupported notebook library.");
        // Acquire only an already loaded module; do not load notebooks at startup.
        if (!noteLibrary) noteLibrary = dlopen("/usr/local/Kobo/libiinknote.so", RTLD_LAZY | RTLD_NOLOAD);
        if (!noteLibrary) throw std::runtime_error("Notebook library is not loaded.");
        noteVolume = reinterpret_cast<decltype(noteVolume)>(dlsym(noteLibrary, "_ZNK11IInkNotePad9getVolumeEv"));
        noteQuit = reinterpret_cast<decltype(noteQuit)>(dlsym(noteLibrary, "_ZN11IInkNotePad4quitEv"));
        if (!noteVolume || !noteQuit) throw std::runtime_error("Notebook navigation API unavailable.");
    }
    struct NativeValue {
        VolumeStorage data;
        void (*destroy)(VolumeStorage *);
        bool constructed = false;
        explicit NativeValue(void (*d)(VolumeStorage *)) : destroy(d) {}
        ~NativeValue() { if (constructed) destroy(&data); }
        NativeValue(const NativeValue &) = delete;
        NativeValue &operator=(const NativeValue &) = delete;
    };
    void openAtLastPage(QWidget *note, const QString &id) {
        bindNotebookApi();
        noteMaximum = reinterpret_cast<decltype(noteMaximum)>(dlsym(noteLibrary, "_ZNK17IInkNotePadWidget13scrollMaximumEv"));
        notePosition = reinterpret_cast<decltype(notePosition)>(dlsym(noteLibrary, "_ZNK17IInkNotePadWidget11scrollValueEv"));
        noteScroll = reinterpret_cast<decltype(noteScroll)>(dlsym(noteLibrary, "_ZN17IInkNotePadWidget8scrollToEi"));
        if (!noteMaximum || !notePosition || !noteScroll)
            throw std::runtime_error("Last-page navigation is unavailable.");
        if (lastPageTimer) { lastPageTimer->stop(); lastPageTimer->deleteLater(); }
        QPointer<QWidget> guard(note);
        QTimer *timer = new QTimer(this);
        lastPageTimer = timer;
        int ticks = 0, previous = -1, stable = 0;
        // Allow native renderVolume/setupVolume and its queued position restore
        // to finish. Stop on user input; never drag a user back after they write.
        QObject::connect(timer, &QTimer::timeout, this,
            [this, guard, id, timer, ticks, previous, stable]() mutable {
                auto stop = [timer]() { timer->stop(); timer->deleteLater(); };
                try {
                    if (!guard || api.view(api.window()) != guard
                        || !QFile::exists(path("enabled")) || !QFile::exists(path("navigation.enabled"))) {
                        stop(); return;
                    }
                    NativeValue volume(api.destroyVolume);
                    noteVolume(&volume.data, guard);
                    volume.constructed = true;
                    if (!api.valid(&volume.data) || !api.basic(&volume.data)
                        || api.externalId(&volume.data).compare(id, Qt::CaseInsensitive) != 0) {
                        stop(); return;
                    }
                    QWidget *canvas = nullptr;
                    for (QWidget *child : guard->findChildren<QWidget *>())
                        if (child->inherits("IInkNotePadWidget")) { canvas = child; break; }
                    ++ticks;
                    if (canvas && canvas->isVisible()) {
                        const int maximum = noteMaximum(canvas);
                        stable = maximum == previous ? stable + 1 : 0;
                        previous = maximum;
                        // For basic notebooks maximum is the final zero-based
                        // package-part index, not an arbitrary scroll distance.
                        if (maximum >= 0 && stable >= 2 && ticks >= 5
                            && (maximum > 0 || ticks >= 20)) {
                            noteScroll(canvas, maximum);
                            report(notePosition(canvas) == maximum
                                ? "linked-notebook-last-page" : "last-page-transition-unconfirmed");
                            stop(); return;
                        }
                    }
                    if (ticks >= 50) { report("last-page-not-ready"); stop(); }
                } catch (...) { nh_log("Last-page navigation failed"); stop(); }
            });
        timer->start(100);
    }
    void getBook(NativeValue &value, const QString &id) {
        const void *device = api.device();
        if (!device) throw std::runtime_error("Current device unavailable.");
        api.lookupBook(&value.data, id, api.dbName(device));
        value.constructed = true;
        if (!api.valid(&value.data) || api.isNotebook(&value.data)
            || QString::fromUtf8(api.contentId(&value.data)) != id)
            throw std::runtime_error("Linked book is unavailable.");
    }
    void openLinkedBook() {
        if (!QFile::exists(path("navigation.enabled")))
            throw std::runtime_error("Notebook navigation is disabled.");
        void *window = api.window();
        QWidget *current = window ? api.view(window) : nullptr;
        if (!current || !current->inherits("IInkNotePad"))
            throw std::runtime_error("Open a basic notebook first.");
        bindNotebookApi();
        NativeValue volume(api.destroyVolume);
        noteVolume(&volume.data, current); // native getter returns by value via r0
        volume.constructed = true;
        if (!api.valid(&volume.data) || !api.basic(&volume.data))
            throw std::runtime_error("Open a basic notebook first.");
        const QString notebookId = api.externalId(&volume.data);
        QJsonObject links = linksState().value("links").toObject();
        QStringList books;
        for (auto it = links.begin(); it != links.end(); ++it)
            if (it.value().toString().compare(notebookId, Qt::CaseInsensitive) == 0) books.append(it.key());
        if (books.isEmpty()) throw std::runtime_error("No linked book");

        QPointer<QWidget> note(current);
        QPointer<QWidget> underneath(api.underneath(api.window()));
        QString selected;
        // Prefer the exact reader from our forward transition, if it still exists
        // immediately underneath this notebook and is still linked to it.
        if (reader && notebookView == note && underneath == reader) {
            QString origin = bookId(reader);
            if (books.contains(origin)) selected = origin;
        }
        if (selected.isEmpty() && books.size() == 1) selected = books.first();
        if (selected.isEmpty()) {
            QStringList titles;
            for (int i = 0; i < books.size(); ++i) {
                QString title;
                try { NativeValue book(api.destroyVolume); getBook(book, books.at(i)); title = api.title(&book.data); }
                catch (const std::exception &) { title = "Unavailable book"; }
                if (title.isEmpty()) title = "Untitled book";
                titles.append(QString::number(i + 1) + ". " + title);
            }
            int page = 0;
            while (selected.isEmpty()) {
                if (!note) return;
                QStringList labels = titles.mid(page * 4, 4);
                const int count = labels.size();
                if (page > 0) labels.append("< Previous page");
                if ((page + 1) * 4 < titles.size()) labels.append("Next page >");
                int choice = ui.choose(note, "Open linked book", labels);
                if (choice < 0) return;
                if (choice < count) selected = books.at(page * 4 + choice);
                else if (page > 0 && choice == count) --page;
                else ++page;
            }
        }
        NativeValue book(api.destroyVolume);
        getBook(book, selected); // Resolve before leaving; failures keep notebook open.
        if (!note || api.view(api.window()) != note)
            throw std::runtime_error("The active notebook changed. Please try again.");
        const bool resume = underneath && underneath->inherits("ReadingView") && bookId(underneath) == selected;
        noteQuit(note); // Exact native Back handler. viewWillDisappear saves handwriting.
        QWidget *after = api.view(api.window());
        if (after == note) throw std::runtime_error("Notebook did not close. Please use its Back button.");
        if (resume && underneath && after == underneath) {
            report("returned-to-linked-reader");
            return;
        }
        NativeValue bookmark(api.bookmarkDtor);
        api.bookmarkCtor(&bookmark.data);
        bookmark.constructed = true;
        const QVector<quint64> emptyKeys;
        QPointer<QObject> bookOpener(api.createReader(nullptr, &book.data, &emptyKeys, nullptr));
        if (!bookOpener) throw std::runtime_error("Cannot create book opener.");
        bookOpener->setParent(this);
        api.openBook(bookOpener, &bookmark.data); // Native default bookmark/resume behavior.
        if (bookOpener) bookOpener->deleteLater();
        QWidget *opened = api.view(api.window());
        if (opened && opened->inherits("ReadingView") && bookId(opened) == selected)
            report("linked-book-opened");
        else report("book-open-requested-transition-unconfirmed");
    }

    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::Gesture) {
            updatePageSwipes(QFile::exists(path("enabled"))
                && !QFile::exists(path("allow-page-swipes")));
        }
        if (lastPageTimer && lastPageTimer->isActive()
            && (event->type() == QEvent::TouchBegin || event->type() == QEvent::MouseButtonPress
                || event->type() == QEvent::TabletPress || event->type() == QEvent::KeyPress)) {
            lastPageTimer->stop();
            lastPageTimer->deleteLater();
        }
        if (event->type() != QEvent::Show || !QFile::exists(path("enabled"))) return false;
        QMenu *menu = qobject_cast<QMenu *>(watched);
        if (!menu || menu->property("koboOsLinkedBook").toBool()) return false;
        void *window = api.window();
        QWidget *current = window ? api.view(window) : nullptr;
        if (!current || !current->inherits("IInkNotePad")) return false;
        bool notebookMenu = false;
        for (QWidget *child : menu->findChildren<QWidget *>())
            if (child->inherits("IInkMenuItem")) { notebookMenu = true; break; }
        if (!notebookMenu) return false;
        try {
            bindNotebookApi();
            QWidgetAction *action = new QWidgetAction(menu);
            QWidget *item = ui.tapItem(menu, "Open linked book");
            action->setDefaultWidget(item);
            if (!QObject::connect(item, SIGNAL(tapped(bool)), action, SLOT(trigger()))) {
                delete action;
                return false;
            }
            QPointer<QMenu> menuGuard(menu);
            QObject::connect(action, &QAction::triggered, this, [this, menuGuard](bool) {
                if (reverseQueued) return;
                reverseQueued = true;
                if (menuGuard) menuGuard->hide();
                QTimer *timer = new QTimer(this);
                timer->setSingleShot(true);
                QObject::connect(timer, &QTimer::timeout, this, [this, timer]() {
                    reverseQueued = false;
                    timer->deleteLater();
                    run("book");
                });
                timer->start(0); // Leave the tap/menu-close callback before navigation.
            });
            menu->insertAction(menu->actions().isEmpty() ? nullptr : menu->actions().first(), action);
            menu->setProperty("koboOsLinkedBook", true);
        } catch (...) { nh_log("Could not add notebook-to-book menu action"); }
        return false;
    }

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
        QJsonObject catalog = liveNotebookCatalog();
        if (!catalog.value("notebooks").isArray()) throw std::runtime_error("Invalid notebook list.");
        QJsonArray rows = catalog.value("notebooks").toArray();
        struct Entry { QString path, id; bool folder; };
        QVector<Entry> entries;
        QSet<QString> seenPaths, seenIds;
        auto safePath = [](const QString &name) {
            if (name.isEmpty() || name.startsWith('/') || name.contains('\\') || name.contains(':') || name.contains(QChar(0))) return false;
            for (const QString &part : name.split('/'))
                if (part.isEmpty() || part == "." || part == "..") return false;
            return true;
        };
        if (!catalog.value("folders").isArray())
            throw std::runtime_error("Refresh the notebook folder catalog over USB.");
        for (const QJsonValue &value : catalog.value("folders").toArray()) {
            QString name = value.toString();
            if (!safePath(name) || seenPaths.contains(name)) throw std::runtime_error("Invalid folder entry.");
            seenPaths.insert(name);
            entries.append(Entry{name, QString(), true});
        }
        for (const QJsonValue &value : rows) {
            QJsonObject row = value.toObject();
            QString id = row.value("notebookId").toString();
            QString name = row.value("relativePath").toString();
            if (!uuid(id) || !safePath(name) || !name.endsWith(".nebo", Qt::CaseInsensitive)
                || seenPaths.contains(name) || seenIds.contains(id.toLower()))
                throw std::runtime_error("Invalid notebook list entry.");
            seenPaths.insert(name);
            seenIds.insert(id.toLower());
            entries.append(Entry{name, id, false});
        }
        auto parentPath = [](const QString &name) {
            int slash = name.lastIndexOf('/');
            return slash < 0 ? QString() : name.left(slash);
        };
        QSet<QString> folderPaths;
        folderPaths.insert(QString());
        for (const Entry &entry : entries) if (entry.folder) folderPaths.insert(entry.path);
        for (const Entry &entry : entries)
            if (!folderPaths.contains(parentPath(entry.path))) throw std::runtime_error("Missing parent folder in catalog.");
        std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
            if (a.folder != b.folder) return a.folder;
            return QString::compare(a.path, b.path, Qt::CaseInsensitive) < 0;
        });
        QPointer<QWidget> original(view);
        QString folder, selectedId;
        int page = 0;
        while (selectedId.isEmpty()) {
            if (!original) return;
            QVector<int> children;
            for (int i = 0; i < entries.size(); ++i)
                if (parentPath(entries.at(i).path) == folder) children.append(i);
            QStringList labels;
            QVector<int> actions;
            const int pageSize = 3;
            const int begin = page * pageSize;
            for (int i = begin; i < qMin(begin + pageSize, children.size()); ++i) {
                const Entry &entry = entries.at(children.at(i));
                QString name = entry.path.section('/', -1);
                if (!entry.folder) name.chop(5);
                labels.append(entry.folder ? "[Folder] " + name + " >" : name);
                actions.append(children.at(i));
            }
            const int contentCount = labels.size();
            if (!folder.isEmpty()) { labels.append("Up"); actions.append(-1); }
            if (page > 0) { labels.append("Previous"); actions.append(-2); }
            if (begin + pageSize < children.size()) { labels.append("Next"); actions.append(-3); }
            QString heading = folder.isEmpty() ? "My Notebooks" : folder.section('/', -1);
            if (children.isEmpty()) heading += " (empty)";
            else heading += QString(" (%1/%2)").arg(page + 1).arg((children.size() + pageSize - 1) / pageSize);
            int choice = ui.choose(original, heading, labels, contentCount);
            if (choice < 0) return;
            int action = actions.at(choice);
            if (action == -1) { folder = parentPath(folder); page = 0; }
            else if (action == -2) --page;
            else if (action == -3) ++page;
            else {
                const Entry &entry = entries.at(action);
                if (entry.folder) { folder = entry.path; page = 0; }
                else if (original && ui.confirm(original, entry.path.left(entry.path.size() - 5))) {
                    selectedId = entry.id;
                }
            }
        }
        if (!original || currentReader() != original || bookId(original) != book)
            throw std::runtime_error("The active book changed. Please try again.");
        Notebook notebook(api, selectedId);
        QJsonObject state = linksState();
        QJsonObject links = state.value("links").toObject();
        links.insert(book, selectedId);
        state.insert("links", links);
        writeJson(path("book-links.json"), state);
        report("linked-and-resolved");
        ui.showOK("Book notebook", "Notebook linked to this book.");
    }
    void run(const QByteArray &command) {
        if (!QFile::exists(path("enabled"))) return;
        if (busy) return;
        busy = true;
        try {
            if (command == "book") { openLinkedBook(); busy = false; return; }
            QWidget *view = currentReader();
            QString book = bookId(view);
            if (command == "link") link(view, book);
            else if (command == "check" || command == "open") {
                QString id = linksState().value("links").toObject().value(book).toString();
                if (id.isEmpty()) throw std::runtime_error("No linked notebook");
                Notebook notebook(api, id);
                if (command == "check") {
                    report("reader-and-notebook-validated");
                    ui.showOK("Book notebook", "Current book and linked basic notebook validated.");
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
                    if (after && after->inherits("IInkNotePad") && api.underneath(window) == view) {
                        notebookView = after;
                        // The successful native push has finished using this opener.
                        // Retain it through the notebook session, then release it after
                        // Nickel destroys that view on Back. This permits a later open
                        // without retaining a stale notebook or requiring a restart.
                        QPointer<QObject> completedOpener(object);
                        QObject::connect(after, &QObject::destroyed, this,
                            [this, completedOpener]() {
                                if (completedOpener) completedOpener->deleteLater();
                                if (opener == completedOpener) {
                                    opener.clear();
                                    notebookView.clear();
                                    reader.clear();
                                }
                            });
                        report("view-pushed-reader-retained");
                        openAtLastPage(after, id);
                    } else report("open-requested-transition-unconfirmed");
                    // Retain the QObject for asynchronous bundle/font dialogs. Do not
                    // manually pop a controller or bypass native notebook save/close.
                }
            } else throw std::runtime_error("Unknown book-notebook command.");
        } catch (const std::exception &error) {
            nh_log("book-notebook action did not complete");
            ui.showOK("Book notebook", QString::fromUtf8(error.what()).toHtmlEscaped());
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
        qApp->installEventFilter(this);
    }
    ~Bridge() {
        if (qApp) qApp->removeEventFilter(this);
        if (fd >= 0) ::close(fd);
        if (noteLibrary) dlclose(noteLibrary);
    }
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
