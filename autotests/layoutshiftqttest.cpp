/*
    SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <memory>
#include <utility>

class LayoutShiftQtTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void actualKeysAndInputEngine_data();
    void actualKeysAndInputEngine();
};

void LayoutShiftQtTest::actualKeysAndInputEngine_data()
{
    QTest::addColumn<QString>("locale");
    QTest::newRow("english") << QStringLiteral("fallback");
    QTest::newRow("ukrainian") << QStringLiteral("uk_UA");
}

void LayoutShiftQtTest::actualKeysAndInputEngine()
{
    QFETCH(QString, locale);
    QFile file(QStringLiteral(LAYOUT_SHIFT_SOURCE_DIR "/src/layouts/") + locale + QStringLiteral("/main.qml"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString source = QString::fromUtf8(file.readAll());
    const QRegularExpression level(QStringLiteral("readonly property bool symbolsShifted:\\s*([^\\n]+)"));
    const auto levelMatch = level.match(source);
    QVERIFY(levelMatch.hasMatch());
    // Load the real changed Key blocks, not a rewritten key implementation.
    const QRegularExpression blocks(QStringLiteral(R"REGEX(\bKey\s*\{(?:[^"{}]|"(?:\\.|[^"\\])*")*\})REGEX"));
    QString keys;
    int count = 0;
    auto matches = blocks.globalMatch(source);
    while (matches.hasNext()) {
        QString block = matches.next().captured();
        if (!block.contains(QStringLiteral("text: pcLayout.symbolsShifted")) && !block.contains(QStringLiteral("text: \"ґ\""))) {
            continue;
        }
        block.insert(block.indexOf(QLatin1Char('{')) + 1, QStringLiteral("\nobjectName: \"fixture%1\"\n").arg(count++));
        keys += block + QLatin1Char('\n');
    }
    QCOMPARE(count, locale == QLatin1String("fallback") ? 21 : 15);
    const QString qml = QStringLiteral(R"QML(
import QtQuick
import QtQuick.Layouts
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Components
Item {
    id: keyboard
    width: 900; height: 300
    property var style: null
    property var soundEffect: QtObject { function register(url) {} }
    property bool navigationModeActive: false
    property int inputItemEchoMode: TextInput.Normal
    readonly property real normalKeyWidth: 40
    property var plain: PlainInputMethod {}
    Item {
        id: pcLayout
        readonly property bool symbolsShifted: %1
        property real keyWeight: 160
        property bool smallTextVisible: true
        KeyboardRow { %2 }
    }
    TextInput {
        id: target
        objectName: "syntheticTarget"
        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
        focus: true
    }
    function prepare() {
        target.forceActiveFocus()
        InputContext.inputEngine.inputMethod = plain
        InputContext.inputEngine.inputMode = InputEngine.InputMode.Latin
    }
    function setState(shift, caps) {
        InputContext.priv.shiftHandler.capsLockActive = caps
        InputContext.priv.shiftHandler.shiftActive = shift
    }
    function deliver(key) {
        var text = key.uppercased ? key.text.toUpperCase() : key.text
        var modifiers = key.uppercased ? Qt.ShiftModifier : 0
        var symbol = key.key
        InputContext.inputEngine.virtualKeyPress(symbol, text, modifiers, false)
        InputContext.inputEngine.virtualKeyRelease(symbol, text, modifiers)
    }
}
)QML")
                            .arg(levelMatch.captured(1), keys);
    QQuickWindow window;
    window.resize(900, 300);
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(qml.toUtf8(), QUrl(QStringLiteral("qrc:/synthetic-layout-shift.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QQuickItem> root(qobject_cast<QQuickItem *>(component.create()));
    QVERIFY2(root, qPrintable(component.errorString()));
    root->setParentItem(window.contentItem());
    window.show();
    window.requestActivate();
    QVERIFY(QMetaObject::invokeMethod(root.get(), "prepare"));
    auto *target = root->findChild<QQuickItem *>(QStringLiteral("syntheticTarget"));
    QVERIFY(target);
    QTRY_VERIFY(target->hasActiveFocus());
    for (int i = 0; i < count; ++i) {
        auto *key = root->findChild<QObject *>(QStringLiteral("fixture%1").arg(i));
        QVERIFY(key);
        for (const auto [shift, caps] : {std::pair{false, false}, {true, false}, {false, false}, {false, true}, {true, true}}) {
            target->setProperty("text", QString());
            QCoreApplication::processEvents();
            QVERIFY(QMetaObject::invokeMethod(root.get(), "setState", Q_ARG(QVariant, shift), Q_ARG(QVariant, caps)));
            const bool uppercase = key->property("uppercased").toBool();
            const QString text = uppercase ? key->property("text").toString().toUpper() : key->property("text").toString();
            const QString caption = uppercase ? key->property("displayText").toString().toUpper() : key->property("displayText").toString();
            QVERIFY(!text.isEmpty());
            QCOMPARE(caption, text);
            QVERIFY(QMetaObject::invokeMethod(root.get(), "deliver", Q_ARG(QVariant, QVariant::fromValue(key))));
            // Only this synthetic offscreen TextInput is observed. No seat,
            // desktop field, clipboard, or production daemon is connected.
            QTRY_COMPARE(target->property("text").toString(), text);
        }
    }
}

int main(int argc, char **argv)
{
    QTemporaryDir sandbox;
    if (!sandbox.isValid()) {
        return 1;
    }
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_IM_MODULE", "qtvirtualkeyboard");
    qputenv("QT_VIRTUALKEYBOARD_DESKTOP_DISABLE", "1");
    qputenv("QT_VIRTUALKEYBOARD_STYLE", "default");
    qputenv("XDG_CONFIG_HOME", sandbox.path().toUtf8());
    qputenv("XDG_CACHE_HOME", sandbox.path().toUtf8());
    qputenv("XDG_DATA_HOME", sandbox.path().toUtf8());
    qputenv("XDG_RUNTIME_DIR", sandbox.path().toUtf8());
    QStandardPaths::setTestModeEnabled(true);
    QGuiApplication app(argc, argv);
    LayoutShiftQtTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "layoutshiftqttest.moc"
