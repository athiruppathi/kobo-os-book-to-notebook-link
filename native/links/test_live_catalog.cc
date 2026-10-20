#include "live_catalog.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <iostream>

static void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
static void touch(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), "Cannot create fixture");
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir temporary;
        require(temporary.isValid(), "Temporary directory failed");
        const QString mount = temporary.path();
        QDir().mkpath(mount + "/.kobo");
        QDir().mkpath(mount + "/My Notebooks/Folder/Nested");
        QDir().mkpath(mount + "/My Notebooks/Empty");
        QSqlDatabase writer = QSqlDatabase::addDatabase("QSQLITE", "fixture");
        writer.setDatabaseName(mount + "/.kobo/KoboReader.sqlite");
        require(writer.open(), "Cannot create database fixture");
        QSqlQuery query(writer);
        require(query.exec("CREATE TABLE content (ExternalId TEXT, ContentID TEXT, MimeType TEXT)"), "Create failed");
        auto add = [&](const QString &relative, bool present) {
            query.prepare("INSERT INTO content VALUES (?, ?, ?)");
            query.addBindValue(QUuid::createUuid().toString().mid(1, 36));
            query.addBindValue(QString::fromUtf8(QUrl::fromLocalFile("/mnt/onboard/My Notebooks/" + relative).toEncoded()));
            query.addBindValue("application/vnd.myscript.nebo+raw");
            require(query.exec(), "Insert failed");
            if (present) touch(mount + "/My Notebooks/" + relative);
        };
        add("Same.nebo", true);
        add("Folder/Nested/Same.nebo", true);
        add("Deleted.nebo", false);
        const auto before = QSqlDatabase::connectionNames();
        QJsonObject catalog = liveNotebookCatalog(mount);
        require(catalog.value("notebooks").toArray().size() == 2, "Nested/duplicate titles missing or deleted row included");
        require(catalog.value("folders").toArray().size() == 3, "Empty/nested folder missing");
        require(QSqlDatabase::connectionNames() == before, "Connection leaked");
        QDir().mkpath(mount + "/My Notebooks/New folder");
        add("New folder/Added after first scan.nebo", true);
        catalog = liveNotebookCatalog(mount);
        require(catalog.value("notebooks").toArray().size() == 3, "Live additions not refreshed");
        require(catalog.value("folders").toArray().size() == 4, "Live folders not refreshed");
        {
            CatalogConnection readonly;
            readonly.db.setDatabaseName(writer.databaseName());
            readonly.db.setConnectOptions("QSQLITE_OPEN_READONLY");
            require(readonly.db.open(), "Read-only open failed");
            QSqlQuery attempt(readonly.db);
            require(!attempt.exec("DELETE FROM content"), "Read-only connection allowed a write");
        }
        touch(mount + "/outside.nebo");
        require(QFile::link(mount + "/outside.nebo", mount + "/My Notebooks/Escape.nebo"), "Symlink fixture failed");
        add("Escape.nebo", false);
        bool rejected = false;
        try { liveNotebookCatalog(mount); } catch (const std::exception &) { rejected = true; }
        require(rejected, "Escaped symlink accepted");
        require(QSqlDatabase::connectionNames() == before, "Error path connection leaked");
        std::cout << "Live catalog checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
