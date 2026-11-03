/*
    SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "sessionlockstate.h"

#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <memory>

class FakeScreenSaver : public QDBusVirtualObject
{
public:
    bool active = false;
    int delay = 0;
    int requests = 0;
    QString introspect(const QString &) const override
    {
        return {};
    }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        if (message.member() != QLatin1String("GetActive")) {
            return false;
        }
        ++requests;
        const bool snapshot = active;
        QTimer::singleShot(delay, this, [message, connection, snapshot] {
            connection.send(message.createReply(QVariantList{snapshot}));
        });
        return true;
    }
};

class SessionLockStateTest : public QObject
{
    Q_OBJECT
    std::unique_ptr<QDBusConnection> m_bus;
    std::unique_ptr<FakeScreenSaver> m_service;

    void change(bool active)
    {
        m_service->active = active;
        auto signal =
            QDBusMessage::createSignal(QStringLiteral("/ScreenSaver"), QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("ActiveChanged"));
        signal << active;
        QVERIFY(m_bus->send(signal));
    }

private Q_SLOTS:
    void initTestCase()
    {
        if (qgetenv("PLASMA_KEYBOARD_TEST_ISOLATED_BUS") != "1") {
            QSKIP("Requires a private dbus-run-session and PLASMA_KEYBOARD_TEST_ISOLATED_BUS=1; never fake ScreenSaver on the desktop bus");
        }
        m_bus = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("lock-state-test-service")));
        QVERIFY(m_bus->isConnected());
        m_service = std::make_unique<FakeScreenSaver>();
        QVERIFY(m_bus->registerVirtualObject(QStringLiteral("/ScreenSaver"), m_service.get()));
        QVERIFY(m_bus->registerService(QStringLiteral("org.freedesktop.ScreenSaver")));
    }

    void init()
    {
        m_service->active = false;
        m_service->delay = 0;
        m_service->requests = 0;
    }

    void unknownAndStartupUnlocked()
    {
        SessionLockState state;
        QVERIFY(!state.isKnown());
        QVERIFY(!state.allowsPredictions());
        QSignalSpy unlocked(&state, &SessionLockState::unlocked);
        QTRY_VERIFY(state.isKnown());
        QVERIFY(!state.isLocked());
        QVERIFY(state.allowsPredictions());
        QCOMPARE(unlocked.count(), 0); // Don't hide an ordinary startup panel.
    }

    void lockUnlockOrderAndNoDuplicates()
    {
        m_service->active = true;
        SessionLockState state;
        QTRY_VERIFY(state.isKnown());
        QVERIFY(state.isLocked());
        QVERIFY(!state.allowsPredictions());
        QStringList signals;
        connect(&state, &SessionLockState::unlocked, this, [&] {
            signals << QStringLiteral("hide");
        });
        connect(&state, &SessionLockState::stateChanged, this, [&] {
            signals << QStringLiteral("refresh");
        });
        change(false);
        QTRY_VERIFY(!state.isLocked());
        QCOMPARE(signals, QStringList({QStringLiteral("hide"), QStringLiteral("refresh")}));
        QVERIFY(state.allowsPredictions());
        change(false);
        QTest::qWait(30);
        QCOMPARE(signals.size(), 2);
    }

    void delayedInitialReplyCannotOverwriteSignal()
    {
        m_service->delay = 180;
        SessionLockState state;
        QTRY_VERIFY(m_service->requests > 0);
        change(true);
        QTRY_VERIFY(state.isLocked());
        QTest::qWait(220);
        QVERIFY(state.isLocked());
        QVERIFY(!state.allowsPredictions());
    }

    void preparationInvalidatesDesktopSnapshot()
    {
        m_service->delay = 180;
        SessionLockState state;
        QTRY_VERIFY(m_service->requests > 0);
        auto signal = QDBusMessage::createSignal(QStringLiteral("/ScreenSaver"), QStringLiteral("org.kde.screensaver"), QStringLiteral("AboutToLock"));
        QVERIFY(m_bus->send(signal));
        QTRY_VERIFY(state.isPreparingLock());
        QVERIFY(state.isLocked());
        QVERIFY(!state.allowsPredictions());
        QTest::qWait(220);
        QVERIFY(state.isPreparingLock());
        QVERIFY(!state.allowsPredictions());
        QSignalSpy changed(&state, &SessionLockState::stateChanged);
        change(true);
        QTRY_VERIFY(!state.isPreparingLock());
        QVERIFY(state.isLocked());
        QCOMPARE(changed.count(), 1); // Ready is distinct from preparing.
        // A late/duplicate AboutToLock must not regress an already locked state.
        QVERIFY(m_bus->send(signal));
        QTest::qWait(30);
        QVERIFY(!state.isPreparingLock());
        QCOMPARE(changed.count(), 1);
    }

    void explicitCancellationEndsPreparation()
    {
        SessionLockState state;
        QTRY_VERIFY(state.allowsPredictions());
        auto signal = QDBusMessage::createSignal(QStringLiteral("/ScreenSaver"), QStringLiteral("org.kde.screensaver"), QStringLiteral("AboutToLock"));
        QVERIFY(m_bus->send(signal));
        QTRY_VERIFY(state.isPreparingLock());
        QSignalSpy unlocked(&state, &SessionLockState::unlocked);
        change(false);
        QTRY_VERIFY(state.allowsPredictions());
        QVERIFY(!state.isPreparingLock());
        QCOMPARE(unlocked.count(), 1);
    }

    void destroyDoesNotWaitForDbus()
    {
        m_service->delay = 200;
        auto state = std::make_unique<SessionLockState>();
        QTRY_VERIFY(m_service->requests > 0);
        QElapsedTimer elapsed;
        elapsed.start();
        state.reset();
        QVERIFY(elapsed.elapsed() < 100);
        QTest::qWait(230);
    }

    void serviceLossIsFailClosed()
    {
        SessionLockState state;
        QTRY_VERIFY(state.allowsPredictions());
        QVERIFY(m_bus->unregisterService(QStringLiteral("org.freedesktop.ScreenSaver")));
        QTRY_VERIFY(!state.isKnown());
        QVERIFY(!state.allowsPredictions());
        m_service->active = true;
        QVERIFY(m_bus->registerService(QStringLiteral("org.freedesktop.ScreenSaver")));
        QTRY_VERIFY(state.isKnown());
        QVERIFY(state.isLocked());
    }

    void cleanupTestCase()
    {
        m_bus.reset();
        QDBusConnection::disconnectFromBus(QStringLiteral("lock-state-test-service"));
    }
};

QTEST_GUILESS_MAIN(SessionLockStateTest)
#include "sessionlockstatetest.moc"
