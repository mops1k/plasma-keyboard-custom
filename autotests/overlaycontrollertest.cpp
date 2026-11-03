// SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
#include "inputplugin.h"
#include "logging.h"
#include "overlaycontroller.h"
#include <QTest>

Q_LOGGING_CATEGORY(PlasmaKeyboard, "overlaycontrollertest")

class HoldTrigger : public OverlayTrigger
{
public:
    QString triggerId() const override { return QStringLiteral("hold"); }
    QString displayName() const override { return triggerId(); }
    bool isEnabled() const override { return true; }
    void reset() override {}
    QStringList candidates(const QString &) const override { return {QStringLiteral("à")}; }
    OverlayTriggerResult processEvent(OverlayInputEvent type, const QKeyEvent *key, const QString &, OverlayController *) override
    {
        OverlayTriggerResult result;
        if (type == OverlayInputEvent::KeyPress && key && !key->text().isEmpty()) {
            result.action = OverlayAction::StartTimer;
            result.consumeEvent = true;
            result.pendingText = key->text();
            result.pendingNativeScanCode = key->nativeScanCode();
            result.timerDurationMs = 60000;
        } else if (type == OverlayInputEvent::TimerExpired) {
            result.action = OverlayAction::OpenOverlay;
        }
        return result;
    }
};

class OverlayControllerTest : public QObject
{
    Q_OBJECT
    static void startRepeat(OverlayController &controller)
    {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, 30, 'a', 0, QStringLiteral("a"));
        QVERIFY(controller.processKeyPress(&press));
        QVERIFY(QMetaObject::invokeMethod(&controller, "handleTimerExpired", Qt::DirectConnection));
        QVERIFY(QMetaObject::invokeMethod(&controller, "handleOverlayGraceTimer", Qt::DirectConnection));
    }
    static QKeyEvent release()
    {
        return QKeyEvent(QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier, 30, 'a', 0, QStringLiteral("a"));
    }
private Q_SLOTS:
    void cancelBalancesRepeat()
    {
        InputPlugin input;
        OverlayController controller(&input);
        controller.registerTrigger(new HoldTrigger);
        startRepeat(controller);
        QCOMPARE(input.events.size(), 3);
        controller.cancelOverlay();
        QCOMPARE(input.events.size(), 4);
        QCOMPARE(input.events.last(), qMakePair(quint32(30), InputPlugin::Released));
        controller.cancelOverlay();
        QCOMPARE(input.events.size(), 4); // cleanup is idempotent
        auto up = release();
        QVERIFY(controller.processKeyRelease(&up)); // already released synthetically
    }
    void physicalReleaseIsNotDuplicated()
    {
        InputPlugin input;
        OverlayController controller(&input);
        controller.registerTrigger(new HoldTrigger);
        startRepeat(controller);
        auto up = release();
        QVERIFY(!controller.processKeyRelease(&up)); // caller forwards it
        QCOMPARE(input.events.size(), 3);
        controller.cancelOverlay();
        QCOMPARE(input.events.size(), 3);
    }
    void rolloverBalancesRepeat()
    {
        InputPlugin input;
        OverlayController controller(&input);
        controller.registerTrigger(new HoldTrigger);
        startRepeat(controller);
        QKeyEvent next(QEvent::KeyPress, Qt::Key_E, Qt::NoModifier, 18, 'e', 0, QStringLiteral("e"));
        QVERIFY(controller.processKeyPress(&next));
        QCOMPARE(input.events.size(), 6); // old key-up + new click
        QCOMPARE(input.events.at(3), qMakePair(quint32(30), InputPlugin::Released));
        auto up = release();
        QVERIFY(controller.processKeyRelease(&up));
        controller.cancelOverlay();
        QCOMPARE(input.events.size(), 6);
    }
    void alternateOverlayBalancesRepeat()
    {
        InputPlugin input;
        OverlayController controller(&input);
        controller.registerTrigger(new HoldTrigger);
        startRepeat(controller);
        QVERIFY(controller.openAlternates({QStringLiteral("é")}));
        QCOMPARE(input.events.size(), 4);
        QCOMPARE(input.events.last(), qMakePair(quint32(30), InputPlugin::Released));
        auto up = release();
        QVERIFY(controller.processKeyRelease(&up));
    }
    void contextChangeReleasesAndClearsSuppression()
    {
        InputPlugin input;
        OverlayController controller(&input);
        controller.registerTrigger(new HoldTrigger);
        startRepeat(controller);
        Q_EMIT input.contextAboutToChange();
        QCOMPARE(input.events.size(), 4);
        QCOMPARE(input.events.last(), qMakePair(quint32(30), InputPlugin::Released));
        // A release delivered by the new grab must not inherit suppression.
        auto up = release();
        QVERIFY(!controller.processKeyRelease(&up));
        controller.cancelOverlay();
        QCOMPARE(input.events.size(), 4);
    }
};

QTEST_GUILESS_MAIN(OverlayControllerTest)
#include "overlaycontrollertest.moc"
