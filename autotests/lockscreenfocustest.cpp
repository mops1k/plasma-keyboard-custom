/*
    SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <memory>

class TouchCounter : public QObject
{
public:
    int begins = 0;
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::TouchBegin) {
            ++begins;
        }
        return false;
    }
};

class LockscreenFocusTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void localTouchesNotGlobalVisibility()
    {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        // Real TextField child consumes pointer events. The same passive
        // handler attaches to that control, preserving the existing TOUCHFIX.
        component.setData(R"(
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import "../tools/lockscreen" as Local
Window {
    id: view
    width: 320; height: 240; visible: true
    property bool globalKeyboardVisible: false
    property bool globalTouchMode: false
    property int activationCount: 0
    MouseArea {
        id: screenItem
        drag.filterChildren: true
        hoverEnabled: true
        objectName: "screenItem"
        anchors.fill: parent
        property bool uiVisible: false
        readonly property bool blockUI: view.globalKeyboardVisible && view.globalTouchMode
        onBlockUIChanged: {
            if (blockUI && view.active) uiVisible = true;
        }
        Local.LocalTouchFocus {
            screenRoot: screenItem
            targetItem: passwordField
            viewWindow: view
            onActivationRequested: ++view.activationCount
        }
        TextField {
            id: passwordField
            objectName: "password"
            width: 220; height: 60; anchors.centerIn: parent
            echoMode: TextInput.Password
        }
    }
}
)",
                          QUrl::fromLocalFile(QStringLiteral(LOCKSCREEN_TEST_SOURCE_DIR "/focus-harness.qml")));
        QVERIFY2(!component.isError(), qPrintable(component.errorString()));
        std::unique_ptr<QQuickWindow> a(qobject_cast<QQuickWindow *>(component.create()));
        QVERIFY2(a, qPrintable(component.errorString()));
        std::unique_ptr<QQuickWindow> b(qobject_cast<QQuickWindow *>(component.create()));
        QVERIFY2(b, qPrintable(component.errorString()));
        a->setPosition(0, 0);
        b->setPosition(400, 0);
        QVERIFY(QTest::qWaitForWindowExposed(a.get()));
        QVERIFY(QTest::qWaitForWindowExposed(b.get()));
        QTest::qWait(80); // Let both scene graphs complete their initial layout.
        for (auto *view : {a.get(), b.get()}) {
            auto *handler = view->findChild<QObject *>(QStringLiteral("localTouchFocus"));
            auto *parentItem = view->findChild<QQuickItem *>(QStringLiteral("password"));
            QVERIFY(handler);
            QVERIFY(parentItem);
            QCOMPARE(handler->property("parent").value<QQuickItem *>(), parentItem);
            QVERIFY(parentItem->width() > 0 && parentItem->height() > 0);
        }
        for (auto *view : {a.get(), b.get()}) {
            view->setProperty("globalTouchMode", true);
            view->setProperty("globalKeyboardVisible", true);
        }
        QCoreApplication::processEvents();
        QCOMPARE(a->property("activationCount").toInt(), 0);
        QCOMPARE(b->property("activationCount").toInt(), 0);

        TouchCounter counterA, counterB;
        a->installEventFilter(&counterA);
        b->installEventFilter(&counterB);
        std::unique_ptr<QPointingDevice> device(QTest::createTouchDevice(QInputDevice::DeviceType::TouchScreen));
        const auto tap = [&](QQuickWindow *view) {
            const QPoint position(view->width() / 2, view->height() / 2);
            auto &counter = view == a.get() ? counterA : counterB;
            const int before = counter.begins;
            QTest::touchEvent(view, device.get()).press(0, position, view).commit();
            QCoreApplication::processEvents();
            const bool delivered = counter.begins > before;
            auto *handler = view->findChild<QObject *>(QStringLiteral("localTouchFocus"));
            const bool handlerActive = handler && handler->property("active").toBool();
            // Always finish the synthetic sequence, even if an assertion fails.
            QTest::touchEvent(view, device.get()).release(0, position, view).commit();
            QCoreApplication::processEvents();
            QVERIFY2(delivered, "No synthetic TouchBegin reached the target QQuickWindow");
            QVERIFY(handlerActive);
        };
        tap(a.get());
        QTRY_COMPARE(a->property("activationCount").toInt(), 1);
        QCOMPARE(b->property("activationCount").toInt(), 0);
        tap(b.get());
        QTRY_COMPARE(b->property("activationCount").toInt(), 1);
        QCOMPARE(a->property("activationCount").toInt(), 1);
        tap(a.get()); // Both UIs already visible: still activate the touched view.
        QTRY_COMPARE(a->property("activationCount").toInt(), 2);
        QCOMPARE(b->property("activationCount").toInt(), 1);
        auto *field = a->findChild<QQuickItem *>(QStringLiteral("password"));
        QVERIFY(field);
        QTRY_VERIFY(field->hasActiveFocus()); // Passive handler didn't eat child input.
        QTest::mouseClick(a.get(), Qt::LeftButton, Qt::NoModifier, QPoint(160, 120));
        QCOMPARE(a->property("activationCount").toInt(), 2); // Touch-only policy.
    }
};
QTEST_MAIN(LockscreenFocusTest)
#include "lockscreenfocustest.moc"
