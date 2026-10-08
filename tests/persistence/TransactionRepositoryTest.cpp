#include "persistence/Database.h"
#include "persistence/TransactionRepository.h"

#include <QTemporaryDir>
#include <QtTest/QTest>

using namespace tweakopedia::persistence;
using namespace Qt::StringLiterals;

class TransactionRepositoryTest final : public QObject
{
    Q_OBJECT

private slots:
    void migratesEmptyDatabaseAndEnablesForeignKeys()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Database database;

        QVERIFY2(database.open(directory.filePath(u"tweakopedia.db"_s)), qPrintable(database.lastError()));
        QCOMPARE(database.schemaVersion(), 1);
        QVERIFY(database.foreignKeysEnabled());
    }

    void preservesRussianPackageName()
    {
        QTemporaryDir directory;
        Database database;
        QVERIFY(database.open(directory.filePath(u"tweakopedia.db"_s)));
        TransactionRepository repository(database);
        const auto transaction = TransactionRecord::pending(
            QUuid::createUuid(),
            u"Мой набор параметров"_s,
            directory.filePath(u"transaction"_s));

        QVERIFY2(repository.insert(transaction), qPrintable(repository.lastError()));
        const auto loaded = repository.find(transaction.id);

        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->packageName, u"Мой набор параметров"_s);
        QCOMPARE(loaded->status, TransactionStatus::Pending);
    }

    void roundTripsEveryTransactionStatus_data()
    {
        QTest::addColumn<int>("status");
        QTest::newRow("pending") << static_cast<int>(TransactionStatus::Pending);
        QTest::newRow("running") << static_cast<int>(TransactionStatus::Running);
        QTest::newRow("succeeded") << static_cast<int>(TransactionStatus::Succeeded);
        QTest::newRow("failed") << static_cast<int>(TransactionStatus::Failed);
        QTest::newRow("rolled-back") << static_cast<int>(TransactionStatus::RolledBack);
        QTest::newRow("interrupted") << static_cast<int>(TransactionStatus::Interrupted);
    }

    void roundTripsEveryTransactionStatus()
    {
        QFETCH(int, status);
        QTemporaryDir directory;
        Database database;
        QVERIFY(database.open(directory.filePath(u"tweakopedia.db"_s)));
        TransactionRepository repository(database);
        auto transaction = TransactionRecord::pending(
            QUuid::createUuid(),
            u"Проверка статуса"_s,
            directory.filePath(u"transaction"_s));
        transaction.status = static_cast<TransactionStatus>(status);

        QVERIFY2(repository.insert(transaction), qPrintable(repository.lastError()));
        const auto loaded = repository.find(transaction.id);

        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->status, transaction.status);
    }

    void updatesExistingStatus()
    {
        QTemporaryDir directory;
        Database database;
        QVERIFY(database.open(directory.filePath(u"tweakopedia.db"_s)));
        TransactionRepository repository(database);
        const auto transaction = TransactionRecord::pending(
            QUuid::createUuid(),
            u"Пакет"_s,
            directory.filePath(u"transaction"_s));
        QVERIFY2(repository.insert(transaction), qPrintable(repository.lastError()));

        QVERIFY(repository.updateStatus(transaction.id, TransactionStatus::Running));
        QVERIFY(repository.updateStatus(transaction.id, TransactionStatus::Succeeded));

        QCOMPARE(repository.find(transaction.id)->status, TransactionStatus::Succeeded);
    }
};

QTEST_GUILESS_MAIN(TransactionRepositoryTest)

#include "TransactionRepositoryTest.moc"
