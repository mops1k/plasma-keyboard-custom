/*
    SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "clipboardhistory.h"
#include "plasmakeyboardsettings.h"
#include "sessionlockstate.h"

#include <QAbstractItemModelTester>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QDir>
#include <QImage>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUrl>
#include <memory>

class FakeService : public QDBusVirtualObject
{
public:
    bool locked = false;
    int reads = 0;
    int clears = 0;
    int delay = 0;
    QStringList entries;
    QString introspect(const QString &) const override
    {
        return {};
    }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &bus) override
    {
        QVariantList values;
        int replyDelay = 0;
        if (message.member() == QLatin1String("GetActive")) {
            values << locked;
        } else if (message.member() == QLatin1String("getClipboardHistoryItem")) {
            ++reads;
            values << entries.value(message.arguments().first().toInt());
            replyDelay = delay;
        } else if (message.member() == QLatin1String("clearClipboardHistory")) {
            ++clears;
            entries.clear();
        } else {
            return false;
        }
        QTimer::singleShot(replyDelay, this, [message, bus, values] {
            bus.send(message.createReply(values));
        });
        return true;
    }
};

class ClipboardHistoryTest : public QObject
{
    Q_OBJECT
    std::unique_ptr<QDBusConnection> m_serviceBus;
    std::unique_ptr<FakeService> m_service;
    QTemporaryDir m_klipperDir;
    QTemporaryDir m_cacheDir;

    //! Mimics the Klipper 6.8 history database: a text entry and an image one,
    //! with the image file next to it. The database is written before any model
    //! is created and is read-only for the model, exactly like the real one.
    bool writeKlipperHistory()
    {
        const QString directory = m_klipperDir.path();
        QDir().mkpath(directory + QStringLiteral("/data/2222222222222222222222222222222222222222"));

        QImage image(8, 8, QImage::Format_RGB32);
        image.fill(Qt::red);
        if (!image.save(directory + QStringLiteral("/data/2222222222222222222222222222222222222222/2222222222222222222222222222222222222222"), "PNG")) {
            return false;
        }

        {
            auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("clipboard-test-klipper"));
            database.setDatabaseName(directory + QStringLiteral("/history3.sqlite"));
            if (!database.open()) {
                return false;
            }
            QSqlQuery query(database);
            const bool schema = query.exec(QStringLiteral("CREATE TABLE main (uuid char(40) PRIMARY KEY, added_time REAL NOT NULL, last_used_time REAL, "
                                                          "mimetypes TEXT NOT NULL, text NTEXT, starred BOOLEAN)"))
                && query.exec(QStringLiteral("CREATE TABLE aux (uuid char(40) NOT NULL, mimetype TEXT NOT NULL, data_uuid char(40) NOT NULL, "
                                             "PRIMARY KEY (uuid, mimetype))"))
                && query.exec(QStringLiteral("INSERT INTO main VALUES ('1111111111111111111111111111111111111111', 200, 200, "
                                             "'text/plain,text/plain;charset=utf-8', 'fixture-a', 0)"))
                && query.exec(QStringLiteral("INSERT INTO main VALUES ('2222222222222222222222222222222222222222', 100, 100, "
                                             "'image/png', NULL, 0)"))
                && query.exec(QStringLiteral("INSERT INTO aux VALUES ('2222222222222222222222222222222222222222', 'image/png', "
                                             "'2222222222222222222222222222222222222222')"));
            database.close();
            if (!schema) {
                return false;
            }
        }
        QSqlDatabase::removeDatabase(QStringLiteral("clipboard-test-klipper"));
        return true;
    }

    void lock(bool active)
    {
        m_service->locked = active;
        auto signal =
            QDBusMessage::createSignal(QStringLiteral("/ScreenSaver"), QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("ActiveChanged"));
        signal << active;
        QVERIFY(m_serviceBus->send(signal));
    }
    void historySignal()
    {
        QVERIFY(m_serviceBus->send(
            QDBusMessage::createSignal(QStringLiteral("/klipper"), QStringLiteral("org.kde.klipper.klipper"), QStringLiteral("clipboardHistoryUpdated"))));
    }

private Q_SLOTS:
    void initTestCase()
    {
        if (qgetenv("PLASMA_KEYBOARD_TEST_ISOLATED_BUS") != "1") {
            QSKIP("Private dbus-run-session required; NEVER fake Klipper or ScreenSaver on a real desktop bus");
        }
        // The model reads the clipboard history of the desktop it runs on. The
        // test must not peek at the real one, nor leave thumbnails behind.
        QVERIFY(m_klipperDir.isValid());
        QVERIFY(m_cacheDir.isValid());
        qputenv("PLASMA_KEYBOARD_KLIPPER_DATA_DIR", m_klipperDir.path().toLocal8Bit());
        qputenv("XDG_CACHE_HOME", m_cacheDir.path().toLocal8Bit());
        QVERIFY(writeKlipperHistory());
        m_serviceBus = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("clipboard-test-service")));
        QVERIFY(m_serviceBus->isConnected());
        m_service = std::make_unique<FakeService>();
        QVERIFY(m_serviceBus->registerVirtualObject(QStringLiteral("/ScreenSaver"), m_service.get()));
        QVERIFY(m_serviceBus->registerVirtualObject(QStringLiteral("/klipper"), m_service.get()));
        QVERIFY(m_serviceBus->registerService(QStringLiteral("org.freedesktop.ScreenSaver")));
        QVERIFY(m_serviceBus->registerService(QStringLiteral("org.kde.klipper")));
    }

    void init()
    {
        m_service->locked = false;
        m_service->reads = 0;
        m_service->clears = 0;
        m_service->delay = 0;
        m_service->entries = {QStringLiteral("fixture-a"), QStringLiteral("fixture-b")};
        PlasmaKeyboardSettings::self()->setClipboardEnabled(true);
    }

    void startupUnknownAndLockedNeverRead()
    {
        m_service->locked = true;
        SessionLockState state;
        ClipboardHistory model(&state);
        model.setAccessAllowed(true);
        QVERIFY(!model.isAvailable());
        QTRY_VERIFY(state.isKnown());
        QVERIFY(state.isLocked());
        historySignal();
        QVERIFY(QMetaObject::invokeMethod(&model, "refresh", Qt::DirectConnection));
        model.clear();
        QTest::qWait(2100); // A whole polling interval must not access Klipper.
        QCOMPARE(m_service->reads, 0);
        QCOMPARE(m_service->clears, 0);
        QVERIFY(model.textAt(0).isEmpty());
        QCOMPARE(model.count(), 0);
    }

    void lockPurgesLocalCacheWithoutClearingDesktop()
    {
        SessionLockState state;
        ClipboardHistory model(&state);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QTRY_VERIFY(state.allowsPredictions());
        QCOMPARE(m_service->reads, 0); // No field has granted access yet.
        model.setAccessAllowed(true);
        QTRY_COMPARE(model.count(), 2);
        QCOMPARE(model.textAt(0), QStringLiteral("fixture-a"));
        const auto oldIndex = model.index(0);
        lock(true);
        QTRY_VERIFY(state.isLocked());
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(model.textAt(0).isEmpty());
        QVERIFY(!model.data(oldIndex, ClipboardHistory::TextRole).isValid());
        model.clear();
        historySignal();
        const int reads = m_service->reads;
        QTest::qWait(50);
        QCOMPARE(m_service->reads, reads);
        QCOMPARE(m_service->clears, 0);
        QCOMPARE(m_service->entries.size(), 2); // Never erase the user's history.
    }

    void disabledSignalCannotBypassGuard()
    {
        PlasmaKeyboardSettings::self()->setClipboardEnabled(false);
        SessionLockState state;
        ClipboardHistory model(&state);
        QTRY_VERIFY(state.allowsPredictions());
        model.setAccessAllowed(true);
        historySignal();
        QVERIFY(QMetaObject::invokeMethod(&model, "refresh", Qt::DirectConnection));
        model.clear();
        QTest::qWait(50);
        QCOMPARE(m_service->reads, 0);
        QCOMPARE(m_service->clears, 0);
    }

    void pendingReplyAfterLockIsDiscarded()
    {
        m_service->delay = 180;
        SessionLockState state;
        ClipboardHistory model(&state);
        QTRY_VERIFY(state.allowsPredictions());
        model.setAccessAllowed(true);
        QTRY_COMPARE(m_service->reads, 1);
        bool eventLoopResponsive = false;
        QTimer::singleShot(0, this, [&] {
            eventLoopResponsive = true;
        });
        QTRY_VERIFY(eventLoopResponsive);
        lock(true);
        QTRY_VERIFY(state.isLocked());
        QTest::qWait(220);
        QCOMPARE(m_service->reads, 1); // No request for index 1 after the lock.
        QCOMPARE(model.count(), 0);
        QVERIFY(model.textAt(0).isEmpty());
        m_service->delay = 0;
        m_service->entries = {QStringLiteral("new-fixture")};
        lock(false);
        QTRY_COMPARE(model.count(), 1);
        QCOMPARE(model.textAt(0), QStringLiteral("new-fixture"));
    }

    void protectedFieldAndRapidChangesCoalesce()
    {
        m_service->delay = 100;
        SessionLockState state;
        ClipboardHistory model(&state);
        QTRY_VERIFY(state.allowsPredictions());
        model.setAccessAllowed(true);
        QTRY_COMPARE(m_service->reads, 1);
        for (int i = 0; i < 20; ++i) {
            model.setAccessAllowed(false);
            historySignal();
            model.setAccessAllowed(true);
        }
        QCOMPARE(m_service->reads, 1); // Still just one outstanding D-Bus read.
        m_service->delay = 0;
        m_service->entries = {QStringLiteral("latest-fixture")};
        QTRY_COMPARE(model.count(), 1);
        QCOMPARE(model.textAt(0), QStringLiteral("latest-fixture"));
        QCOMPARE(m_service->reads, 3); // Old request + latest index 0 and terminator.
        model.setAccessAllowed(false);
        QVERIFY(!model.isAvailable());
        QCOMPARE(model.count(), 0);
        QVERIFY(model.textAt(0).isEmpty());
    }

    void destroyWithPendingReply()
    {
        m_service->delay = 150;
        SessionLockState state;
        auto model = std::make_unique<ClipboardHistory>(&state);
        QTRY_VERIFY(state.allowsPredictions());
        model->setAccessAllowed(true);
        QTRY_COMPARE(m_service->reads, 1);
        model.reset();
        QTest::qWait(200);
        QCOMPARE(m_service->reads, 1);
    }

    void lockStateLifetimeIsFailClosed()
    {
        auto state = std::make_unique<SessionLockState>();
        ClipboardHistory model(state.get());
        QTRY_VERIFY(state->allowsPredictions());
        model.setAccessAllowed(true);
        QTRY_COMPARE(model.count(), 2);
        state.reset();
        QVERIFY(!model.isAvailable());
        QCOMPARE(model.count(), 0);
        QCOMPARE(model.rowCount(), 0);
        model.clear();
        QVERIFY(model.textAt(0).isEmpty());
        QCOMPARE(m_service->clears, 0);
    }

    void imageEntriesAreRecognisedAndThumbnailed()
    {
        // Klipper offers the entry as the placeholder followed by the size of
        // the image, and the database row at the same index names the file it
        // kept.
        m_service->entries = {QStringLiteral("fixture-a"), QString(QChar(0x25A8)) + QStringLiteral(" 8×8")};
        SessionLockState state;
        ClipboardHistory model(&state);
        QTRY_VERIFY(state.allowsPredictions());
        model.setAccessAllowed(true);
        QTRY_COMPARE(model.count(), 2);

        QVERIFY(!model.isImageAt(0));
        QCOMPARE(model.textAt(0), QStringLiteral("fixture-a"));

        QVERIFY(model.isImageAt(1));
        QVERIFY(model.textAt(1).isEmpty());
        QVERIFY(model.data(model.index(1), ClipboardHistory::ImageRole).toBool());
        QVERIFY(model.data(model.index(1), ClipboardHistory::TextRole).toString().isEmpty());

        const QString imagePath = model.imagePathAt(1);
        QVERIFY(imagePath.endsWith(QStringLiteral("/data/2222222222222222222222222222222222222222/2222222222222222222222222222222222222222")));
        QVERIFY(QFile::exists(imagePath));

        const QString thumbnail = model.thumbnailAt(1);
        QVERIFY(thumbnail.startsWith(QStringLiteral("file://")));
        QVERIFY(QFile::exists(QUrl(thumbnail).toLocalFile()));
        QVERIFY(model.imagePathAt(0).isEmpty());
        QVERIFY(model.thumbnailAt(0).isEmpty());

        // A text entry that happens to sit on the image row of the database is
        // still text: the D-Bus value must be the placeholder as well.
        m_service->entries = {QStringLiteral("fixture-a"), QStringLiteral("fixture-b")};
        historySignal();
        QTRY_COMPARE(model.textAt(1), QStringLiteral("fixture-b"));
        QVERIFY(!model.isImageAt(1));
        QVERIFY(model.imagePathAt(1).isEmpty());
    }

    void cleanupTestCase()
    {
        m_serviceBus.reset();
        QDBusConnection::disconnectFromBus(QStringLiteral("clipboard-test-service"));
        qunsetenv("PLASMA_KEYBOARD_KLIPPER_DATA_DIR");
        qunsetenv("XDG_CACHE_HOME");
    }
};

QTEST_GUILESS_MAIN(ClipboardHistoryTest)
#include "clipboardhistorytest.moc"
