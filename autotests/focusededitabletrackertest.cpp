// SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
#include "focusededitabletracker.h"
#include "logging.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QScreen>
#include <QTest>
#include <QTimer>

Q_LOGGING_CATEGORY(PlasmaKeyboard, "focusededitabletrackertest")

struct Bounds { int x = 0, y = 0, width = 100, height = 100; };
Q_DECLARE_METATYPE(Bounds)
QDBusArgument &operator<<(QDBusArgument &arg, const Bounds &bounds)
{
    arg.beginStructure();
    arg << bounds.x << bounds.y << bounds.width << bounds.height;
    arg.endStructure();
    return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg, Bounds &bounds)
{
    arg.beginStructure();
    arg >> bounds.x >> bounds.y >> bounds.width >> bounds.height;
    arg.endStructure();
    return arg;
}

class AccessibilityService : public QDBusVirtualObject
{
public:
    QHash<QString, int> stateDelay, extentsDelay;
    QSet<QString> nonEditable, errors;
    QStringList requests;
    int registrations = 0;
    int addressDelay = 0;
    int registrationDelay = 0;

    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        const QString method = message.member();
        const QString path = message.path();
        requests.append(path + QLatin1Char(':') + method);
        QVariantList values;
        int delay = 0;
        if (method == QLatin1String("GetAddress")) {
            values << QString::fromLocal8Bit(qgetenv("DBUS_SESSION_BUS_ADDRESS"));
            delay = addressDelay;
        } else if (method == QLatin1String("RegisterEvent")) {
            ++registrations;
            delay = registrationDelay;
        } else if (method == QLatin1String("DeregisterEvent")) {
        } else if (method == QLatin1String("GetState")) {
            values << QVariant::fromValue(QList<uint>{nonEditable.contains(path) ? 0u : 128u, 0u});
            delay = stateDelay.value(path);
        } else if (method == QLatin1String("GetExtents")) {
            values << QVariant::fromValue(path == QLatin1String("/new") ? Bounds{400, 0, 100, 100} : Bounds{});
            delay = extentsDelay.value(path);
        } else {
            return false;
        }
        const bool fail = errors.contains(path);
        // Always reply later, from another bus connection: no local-call shortcut.
        QTimer::singleShot(delay, this, [message, connection, values, fail] {
            connection.send(fail ? message.createErrorReply(QStringLiteral("org.example.Unavailable"), QStringLiteral("unavailable"))
                                 : message.createReply(values));
        });
        return true;
    }
};

class FocusedEditableTrackerTest : public QObject
{
    Q_OBJECT
    std::unique_ptr<QDBusConnection> m_serviceBus;
    std::unique_ptr<AccessibilityService> m_service;

    void focus(const QString &path, bool enabled = true)
    {
        auto event = QDBusMessage::createSignal(path, QStringLiteral("org.a11y.atspi.Event.Object"), QStringLiteral("StateChanged"));
        event << QStringLiteral("focused") << int(enabled) << 0 << QVariant::fromValue(QDBusVariant(0)) << QVariantMap();
        QVERIFY(m_serviceBus->send(event));
    }
    static QPointF touch(int x, int y)
    {
        const auto geometry = QGuiApplication::primaryScreen()->geometry();
        return QPointF(qreal(x - geometry.x()) / geometry.width(), qreal(y - geometry.y()) / geometry.height());
    }
private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(QGuiApplication::primaryScreen());
        // This mock owns real AT-SPI names. Never run it on the user's desktop bus.
        if (qgetenv("PLASMA_KEYBOARD_TEST_ISOLATED_BUS") != "1" || !QDBusConnection::sessionBus().isConnected()) {
            QSKIP("Run on a private D-Bus session with PLASMA_KEYBOARD_TEST_ISOLATED_BUS=1");
        }
        qDBusRegisterMetaType<Bounds>();
        qDBusRegisterMetaType<QList<uint>>();
        m_serviceBus = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(
            QString::fromLocal8Bit(qgetenv("DBUS_SESSION_BUS_ADDRESS")), QStringLiteral("tracker-test-service")));
        QVERIFY(m_serviceBus->isConnected());
        QVERIFY(m_serviceBus->registerService(QStringLiteral("org.a11y.Bus")));
        QVERIFY(m_serviceBus->registerService(QStringLiteral("org.a11y.atspi.Registry")));
        m_service = std::make_unique<AccessibilityService>();
        QVERIFY(m_serviceBus->registerVirtualObject(QStringLiteral("/"), m_service.get(), QDBusConnection::SubPath));
    }
    void init()
    {
        m_service->stateDelay.clear();
        m_service->extentsDelay.clear();
        m_service->nonEditable.clear();
        m_service->errors.clear();
        m_service->requests.clear();
        m_service->registrations = 0;
        m_service->addressDelay = 0;
        m_service->registrationDelay = 0;
    }
    void slowDiscoveryDoesNotBlock()
    {
        m_service->addressDelay = 300;
        m_service->registrationDelay = 300;
        QElapsedTimer elapsed;
        elapsed.start();
        FocusedEditableTracker tracker;
        QVERIFY(elapsed.elapsed() < 200);
        bool heartbeat = false;
        QTimer::singleShot(20, &tracker, [&] { heartbeat = true; });
        QTRY_VERIFY(heartbeat);
        QCOMPARE(m_service->registrations, 0);
        QTRY_COMPARE(m_service->registrations, 1);
        QTest::qWait(350);
        focus(QStringLiteral("/slow"));
        QTRY_VERIFY(tracker.containsNormalizedTouch(touch(50, 50)));
    }
    void slowStateAndExtentsKeepEventLoopResponsive()
    {
        m_service->stateDelay[QStringLiteral("/slow")] = 300;
        m_service->extentsDelay[QStringLiteral("/slow")] = 300;
        FocusedEditableTracker tracker(QDBusConnection::sessionBus());
        QTRY_COMPARE(m_service->registrations, 1);
        QElapsedTimer responsiveness;
        responsiveness.start();
        focus(QStringLiteral("/slow"));
        QTRY_VERIFY(m_service->requests.contains(QStringLiteral("/slow:GetState")));
        QVERIFY(responsiveness.elapsed() < 250);
        int heartbeats = 0;
        QTimer heartbeat;
        connect(&heartbeat, &QTimer::timeout, this, [&] { ++heartbeats; });
        heartbeat.start(10);
        QTest::qWait(100);
        QVERIFY(heartbeats >= 3);
        QVERIFY(!tracker.containsNormalizedTouch(touch(50, 50)));
        QTRY_VERIFY(m_service->requests.contains(QStringLiteral("/slow:GetExtents")));
        const int before = heartbeats;
        QTest::qWait(100);
        QVERIFY(heartbeats > before);
        QTRY_VERIFY(tracker.containsNormalizedTouch(touch(50, 50)));
    }
    void staleReplies_data()
    {
        QTest::addColumn<bool>("delayState");
        QTest::newRow("late-state") << true;
        QTest::newRow("late-extents") << false;
    }
    void staleReplies()
    {
        QFETCH(bool, delayState);
        if (delayState) m_service->stateDelay[QStringLiteral("/old")] = 300;
        else m_service->extentsDelay[QStringLiteral("/old")] = 300;
        FocusedEditableTracker tracker(QDBusConnection::sessionBus());
        QTRY_COMPARE(m_service->registrations, 1);
        focus(QStringLiteral("/old"));
        QTRY_VERIFY(m_service->requests.contains(delayState ? QStringLiteral("/old:GetState") : QStringLiteral("/old:GetExtents")));
        focus(QStringLiteral("/new"));
        QTRY_VERIFY(tracker.containsNormalizedTouch(touch(450, 50)));
        focus(QStringLiteral("/old"), false); // late blur must not clear new focus
        QTest::qWait(400);
        QVERIFY(tracker.containsNormalizedTouch(touch(450, 50)));
        QVERIFY(!tracker.containsNormalizedTouch(touch(50, 50)));
        if (delayState) QVERIFY(!m_service->requests.contains(QStringLiteral("/old:GetExtents")));
    }
    void blurInvalidatesPendingReply()
    {
        m_service->extentsDelay[QStringLiteral("/old")] = 250;
        FocusedEditableTracker tracker(QDBusConnection::sessionBus());
        QTRY_COMPARE(m_service->registrations, 1);
        focus(QStringLiteral("/old"));
        QTRY_VERIFY(m_service->requests.contains(QStringLiteral("/old:GetExtents")));
        focus(QStringLiteral("/old"), false);
        QTest::qWait(350);
        QVERIFY(!tracker.containsNormalizedTouch(touch(50, 50)));
    }
    void newNonEditableOrFailedTargetClearsOldBounds_data()
    {
        QTest::addColumn<bool>("fail");
        QTest::newRow("non-editable") << false;
        QTest::newRow("error") << true;
    }
    void newNonEditableOrFailedTargetClearsOldBounds()
    {
        QFETCH(bool, fail);
        FocusedEditableTracker tracker(QDBusConnection::sessionBus());
        QTRY_COMPARE(m_service->registrations, 1);
        focus(QStringLiteral("/old"));
        QTRY_VERIFY(tracker.containsNormalizedTouch(touch(50, 50)));
        if (fail) m_service->errors.insert(QStringLiteral("/new"));
        else m_service->nonEditable.insert(QStringLiteral("/new"));
        focus(QStringLiteral("/new"));
        QTRY_VERIFY(m_service->requests.contains(QStringLiteral("/new:GetState")));
        QVERIFY(!tracker.containsNormalizedTouch(touch(50, 50)));
        QTest::qWait(50);
        QVERIFY(!tracker.containsNormalizedTouch(touch(450, 50)));
    }
    void timedOutReplyCannotRestoreBounds()
    {
        m_service->stateDelay[QStringLiteral("/old")] = 1500;
        FocusedEditableTracker tracker(QDBusConnection::sessionBus());
        QTRY_COMPARE(m_service->registrations, 1);
        focus(QStringLiteral("/old"));
        QTRY_VERIFY(m_service->requests.contains(QStringLiteral("/old:GetState")));
        QTest::qWait(1700);
        QVERIFY(!tracker.containsNormalizedTouch(touch(50, 50)));
        QVERIFY(!m_service->requests.contains(QStringLiteral("/old:GetExtents")));
    }
    void failedRegistrationDisablesTracking()
    {
        m_service->errors.insert(QStringLiteral("/org/a11y/atspi/registry"));
        FocusedEditableTracker tracker(QDBusConnection::sessionBus());
        QTRY_COMPARE(m_service->registrations, 1);
        QTest::qWait(50);
        focus(QStringLiteral("/old"));
        QTest::qWait(100);
        QVERIFY(!m_service->requests.contains(QStringLiteral("/old:GetState")));
        QVERIFY(!tracker.containsNormalizedTouch(touch(50, 50)));
    }
    void destroyWithPendingReply()
    {
        m_service->stateDelay[QStringLiteral("/old")] = 200;
        auto tracker = std::make_unique<FocusedEditableTracker>(QDBusConnection::sessionBus());
        QTRY_COMPARE(m_service->registrations, 1);
        focus(QStringLiteral("/old"));
        QTRY_VERIFY(m_service->requests.contains(QStringLiteral("/old:GetState")));
        QElapsedTimer elapsed;
        elapsed.start();
        tracker.reset();
        QVERIFY(elapsed.elapsed() < 200);
        QTest::qWait(300); // no callback may access the destroyed tracker
    }
    void cleanup()
    {
        QTest::qWait(20); // drain deregistration before resetting mock counters
    }
    void cleanupTestCase()
    {
        m_service.reset();
        m_serviceBus.reset();
        QDBusConnection::disconnectFromBus(QStringLiteral("tracker-test-service"));
    }
};

QTEST_MAIN(FocusedEditableTrackerTest)
#include "focusededitabletrackertest.moc"
