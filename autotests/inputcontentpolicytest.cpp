/*
    SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "inputcontentpolicy.h"
#include <QTest>

class InputContentPolicyTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void predictionPrivacy_data()
    {
        QTest::addColumn<bool>("hasContext");
        QTest::addColumn<bool>("typeKnown");
        QTest::addColumn<int>("hint");
        QTest::addColumn<int>("purpose");
        QTest::addColumn<bool>("allowed");
        QTest::newRow("no-context") << false << true << 7 << 0 << false;
        QTest::newRow("activation-before-content-type") << true << false << 0 << 0 << false;
        QTest::newRow("ordinary-text") << true << true << 7 << 0 << true;
        QTest::newRow("ordinary-no-hints") << true << true << 0 << 0 << true;
        QTest::newRow("hidden-only") << true << true << 0x40 << 0 << false;
        QTest::newRow("sensitive-only") << true << true << 0x80 << 0 << false;
        QTest::newRow("hidden-and-sensitive") << true << true << 0xc0 << 0 << false;
        QTest::newRow("password-purpose-only") << true << true << 0 << 8 << false;
        QTest::newRow("password-overrides-completion") << true << true << 7 << 8 << false;
        QTest::newRow("sensitive-overrides-completion") << true << true << 0x87 << 0 << false;
        QTest::newRow("hidden-pin") << true << true << 0x40 << 2 << false;
    }

    void predictionPrivacy()
    {
        QFETCH(bool, hasContext);
        QFETCH(bool, typeKnown);
        QFETCH(int, hint);
        QFETCH(int, purpose);
        QFETCH(bool, allowed);
        QCOMPARE(InputContentPolicy::predictionsAllowed(hasContext, typeKnown, InputPlugin::ContentHint(hint), InputPlugin::ContentPurpose(purpose)), allowed);
    }

    void passwordQtHints()
    {
        const auto password = InputContentPolicy::privacyHints(InputPlugin::content_hint_default, InputPlugin::content_purpose_password);
        QVERIFY(password.testFlag(Qt::ImhSensitiveData));
        QVERIFY(password.testFlag(Qt::ImhHiddenText));
        QVERIFY(password.testFlag(Qt::ImhNoPredictiveText));
        QVERIFY(password.testFlag(Qt::ImhNoAutoUppercase));
        const auto hidden = InputContentPolicy::privacyHints(InputPlugin::content_hint_hidden_text, InputPlugin::content_purpose_normal);
        QVERIFY(hidden.testFlag(Qt::ImhHiddenText));
        QVERIFY(hidden.testFlag(Qt::ImhSensitiveData));
        const auto sensitive = InputContentPolicy::privacyHints(InputPlugin::content_hint_sensitive_data, InputPlugin::content_purpose_normal);
        QVERIFY(sensitive.testFlag(Qt::ImhSensitiveData));
        QVERIFY(sensitive.testFlag(Qt::ImhNoPredictiveText));
        QVERIFY(!sensitive.testFlag(Qt::ImhHiddenText));
        QCOMPARE(InputContentPolicy::privacyHints(InputPlugin::content_hint_default, InputPlugin::content_purpose_normal), Qt::InputMethodHints{});
    }

    void transitions()
    {
        // The same context may change type; every check uses current metadata.
        QVERIFY(InputContentPolicy::predictionsAllowed(true, true, InputPlugin::content_hint_default, InputPlugin::content_purpose_normal));
        QVERIFY(!InputContentPolicy::predictionsAllowed(true, true, InputPlugin::content_hint_default, InputPlugin::content_purpose_password));
        QVERIFY(!InputContentPolicy::predictionsAllowed(true, false, InputPlugin::content_hint_none, InputPlugin::content_purpose_normal));
        QVERIFY(InputContentPolicy::predictionsAllowed(true, true, InputPlugin::content_hint_default, InputPlugin::content_purpose_normal));
    }
};

QTEST_GUILESS_MAIN(InputContentPolicyTest)
#include "inputcontentpolicytest.moc"
