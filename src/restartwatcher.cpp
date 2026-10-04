/*
    SPDX-FileCopyrightText: 2026 Plasma Keyboard contributors

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "restartwatcher.h"
#include "logging.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>

namespace
{
//! How often the installed binary is compared with the one this process runs.
constexpr int s_pollIntervalMs = 30 * 1000;
//! Time KWin gets to tear the old input method down before starting a new one.
constexpr int s_kwinRestartDelayMs = 1000;
//! Longest the restart is postponed while the keyboard is on screen.
constexpr int s_maxDeferMs = 2 * 60 * 1000;

QString kwinInputMethod()
{
    const KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Wayland"));
    return group.readEntry(QStringLiteral("InputMethod"));
}

bool customInputMethodIsConfigured()
{
    const QString desktopFile = QFileInfo(kwinInputMethod()).fileName();
    return desktopFile.startsWith(QStringLiteral("org.kde.plasma.keyboard.custom"));
}

void writeKwinInputMethod(const QString &value)
{
    QProcess::execute(QStringLiteral("kwriteconfig6"),
                      {QStringLiteral("--notify"),
                       QStringLiteral("--file"),
                       QStringLiteral("kwinrc"),
                       QStringLiteral("--group"),
                       QStringLiteral("Wayland"),
                       QStringLiteral("--key"),
                       QStringLiteral("InputMethod"),
                       value});
}

//! Ask KWin, whose state is the authoritative one (the window of this process
//! keeps reporting itself as visible).
bool keyboardVisible()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                      QStringLiteral("/VirtualKeyboard"),
                                                      QStringLiteral("org.freedesktop.DBus.Properties"),
                                                      QStringLiteral("Get"));
    msg << QStringLiteral("org.kde.kwin.VirtualKeyboard") << QStringLiteral("visible");
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
        return reply.arguments().constFirst().value<QDBusVariant>().variant().toBool();
    }
    return false;
}
}

int restartInputMethod()
{
    const QString inputMethod = kwinInputMethod();
    if (inputMethod.isEmpty()) {
        return 0;
    }

    qCInfo(PlasmaKeyboard) << "Restarting the keyboard, the installed binary changed";
    // Emptying the setting kills the process that runs this helper, restoring
    // it makes KWin start a fresh one.
    writeKwinInputMethod(QString());
    QThread::msleep(s_kwinRestartDelayMs);
    writeKwinInputMethod(inputMethod);
    return 0;
}

RestartWatcher::RestartWatcher(QObject *parent)
    : QObject(parent)
{
    // /proc/self/exe follows symlinks, so this is the file a package update
    // replaces.
    m_binaryPath = QFile::symLinkTarget(QStringLiteral("/proc/self/exe"));
    if (m_binaryPath.isEmpty()) {
        m_binaryPath = QCoreApplication::applicationFilePath();
    }

    const QFileInfo info(m_binaryPath);
    m_modified = info.lastModified();
    m_size = info.size();
    m_sinceDetection.start();

    if (!customInputMethodIsConfigured()) {
        // This process is not the configured input method, so KWin will not
        // keep it alive: nothing to restart.
        return;
    }

    m_timer = new QTimer(this);
    m_timer->setInterval(s_pollIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &RestartWatcher::checkForUpdate);
    m_timer->start();
}

void RestartWatcher::checkForUpdate()
{
    const QFileInfo info(m_binaryPath);
    if (!info.exists()) {
        // The package manager is in the middle of replacing the file.
        return;
    }

    const bool changed = info.lastModified() != m_modified || info.size() != m_size;
    if (!changed && !m_restartPending) {
        return;
    }

    if (changed) {
        m_modified = info.lastModified();
        m_size = info.size();
        m_restartPending = true;
        m_sinceDetection.start();
    }

    // Do not pull the keyboard away from under the user's fingers, but do not
    // wait forever either: an update has to reach the running keyboard.
    if (keyboardVisible() && m_sinceDetection.elapsed() < s_maxDeferMs) {
        return;
    }

    m_restartPending = false;
    restart();
}

void RestartWatcher::restart()
{
    qCInfo(PlasmaKeyboard) << "The installed binary changed, restarting the keyboard";
    // The helper survives the toggle below; this process is killed by KWin as
    // soon as the input method is torn down, so quit cleanly (which also
    // releases the single instance lock) and let the helper finish the job.
    if (!QProcess::startDetached(m_binaryPath, {QStringLiteral("--restart-input-method")})) {
        qCWarning(PlasmaKeyboard) << "Could not start the restart helper";
        return;
    }
    QCoreApplication::quit();
}

int runInputMethodWatchdog()
{
    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString instanceLockPath = runtimeDir + QStringLiteral("/plasma-keyboard-custom.lock");
    const QString watchdogLockPath = runtimeDir + QStringLiteral("/plasma-keyboard-custom-watchdog.lock");

    // A single watchdog is enough, however many keyboard instances ask for one.
    QLockFile watchdogLock(watchdogLockPath);
    watchdogLock.setStaleLockTime(0);
    if (!watchdogLock.tryLock(0)) {
        return 0;
    }

    constexpr int s_checkIntervalMs = 5000;
    constexpr int s_startGraceMs = 20000;

    for (;;) {
        if (!customInputMethodIsConfigured()) {
            // Another virtual keyboard is configured (or virtual keyboards are
            // disabled), so it is not ours to keep alive.
            QThread::msleep(s_checkIntervalMs);
            continue;
        }

        QLockFile instanceProbe(instanceLockPath);
        instanceProbe.setStaleLockTime(0);
        if (!instanceProbe.tryLock(0)) {
            // An input method process is running, the global shortcut is
            // registered by it.
            QThread::msleep(s_checkIntervalMs);
            continue;
        }
        instanceProbe.unlock();

        // KWin does not start the input method again after it exits, which
        // would silently take the global shortcut with it. Toggle the setting
        // so KWin starts a fresh process.
        qCInfo(PlasmaKeyboard) << "The keyboard is not running, restarting it";
        restartInputMethod();
        QThread::msleep(s_startGraceMs);
    }
}
