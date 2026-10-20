#pragma once
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QUrl>
#include <QUuid>
#include <stdexcept>

// A separate, read-only connection; never reuse Nickel's default connection.
struct CatalogConnection {
    QString name = "kobo-os-catalog-" + QUuid::createUuid().toString();
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
    ~CatalogConnection() {
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
    }
};

static QJsonObject liveNotebookCatalog(const QString &mount = "/mnt/onboard") {
    const QString root = QDir(mount + "/My Notebooks").canonicalPath();
    if (root.isEmpty()) throw std::runtime_error("My Notebooks is unavailable.");
    CatalogConnection connection;
    connection.db.setDatabaseName(mount + "/.kobo/KoboReader.sqlite");
    connection.db.setConnectOptions("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=1000");
    if (!connection.db.open()) throw std::runtime_error("Cannot read the current notebook catalog. Please try again.");
    QSqlQuery query(connection.db);
    if (!query.exec("PRAGMA query_only=ON") || !query.exec(
        "SELECT ExternalId, ContentID FROM content WHERE MimeType = 'application/vnd.myscript.nebo+raw'"))
        throw std::runtime_error("Cannot read the current notebook list.");
    QJsonArray notebooks, folders;
    QSet<QString> identities, paths;
    while (query.next()) {
        const QString id = query.value(0).toString();
        // Nickel stores literal spaces, including in "My Notebooks". Accept
        // that observed form without relaxing strict parsing of other syntax.
        QString contentId = query.value(1).toString();
        contentId.replace(QLatin1Char(' '), QStringLiteral("%20"));
        const QUrl url(contentId, QUrl::StrictMode);
        if (!url.isValid() || !url.isLocalFile() || !url.host().isEmpty()
            || url.hasQuery() || url.hasFragment())
            throw std::runtime_error("Invalid notebook path in the catalog.");
        const QString prefix = "/mnt/onboard/My Notebooks/";
        const QString local = url.toLocalFile();
        if (!local.startsWith(prefix)) throw std::runtime_error("Notebook is outside My Notebooks.");
        const QString relative = local.mid(prefix.size());
        for (const QString &part : relative.split('/'))
            if (part.isEmpty() || part == "." || part == ".." || part.contains('\\')
                || part.contains(':') || part.contains(QChar(0)))
                throw std::runtime_error("Invalid notebook path in the catalog.");
        const QFileInfo file(root + '/' + relative);
        // Deleted/stale database rows are not selectable. Never follow escaped links.
        if (!file.exists()) continue;
        if (!file.isFile() || !file.canonicalFilePath().startsWith(root + '/'))
            throw std::runtime_error("Notebook path escapes My Notebooks.");
        if (id.size() != 36 || QUuid(id).isNull()
            || QUuid(id).toString().mid(1, 36).compare(id, Qt::CaseInsensitive) != 0
            || !relative.endsWith(".nebo", Qt::CaseInsensitive)
            || identities.contains(id.toLower()) || paths.contains(relative))
            throw std::runtime_error("Ambiguous notebook identity in the catalog.");
        identities.insert(id.toLower());
        paths.insert(relative);
        QJsonObject row;
        row.insert("notebookId", id);
        row.insert("relativePath", relative);
        notebooks.append(row);
    }
    if (query.lastError().isValid()) throw std::runtime_error("Notebook catalog read was interrupted. Please try again.");
    query.finish();
    QDirIterator it(root, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo directory = it.fileInfo();
        if (directory.isSymLink() || !directory.canonicalFilePath().startsWith(root + '/'))
            throw std::runtime_error("Unsupported link in My Notebooks.");
        folders.append(QDir(root).relativeFilePath(directory.filePath()));
    }
    QJsonObject catalog;
    catalog.insert("version", 1);
    catalog.insert("notebooks", notebooks);
    catalog.insert("folders", folders);
    return catalog;
}
