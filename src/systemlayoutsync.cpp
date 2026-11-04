/*
    SPDX-FileCopyrightText: 2026 Chechulin Serhii

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "systemlayoutsync.h"
#include "logging.h"

#include <KConfig>
#include <KConfigGroup>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QStandardPaths>

namespace
{
constexpr auto s_service = "org.kde.keyboard";
constexpr auto s_path = "/Layouts";
constexpr auto s_iface = "org.kde.KeyboardLayouts";

//! The Plasma OSD shows a popup on every layout change; the keyboard switches
//! the layout itself, so the popup has to be kept out of the way while it is
//! on screen. Key and group of that setting in plasmarc.
constexpr auto s_plasmaConfig = "plasmarc";
constexpr auto s_osdGroup = "OSD";
constexpr auto s_osdKey = "kbdLayoutChangedEnabled";

QDBusMessage layoutsCall(const QString &method)
{
    return QDBusMessage::createMethodCall(QLatin1String(s_service), QLatin1String(s_path), QLatin1String(s_iface), method);
}
}

SystemLayoutSync::SystemLayoutSync(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::sessionBus()
        .connect(QLatin1String(s_service), QLatin1String(s_path), QLatin1String(s_iface), QStringLiteral("layoutChanged"), this, SLOT(onLayoutChanged(uint)));

    // A previous process may have died while the OSD was suppressed: the
    // marker keeps the value the user had chosen, so put it back.
    QFile marker(osdMarkerPath());
    if (marker.exists() && marker.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString saved = QString::fromUtf8(marker.readAll()).trimmed();
        marker.close();
        m_osdOriginal = saved != QLatin1String("false");
        writeOsdEnabled(m_osdOriginal);
        QFile::remove(osdMarkerPath());
        qCWarning(PlasmaKeyboard) << "Restored the layout OSD setting after an unclean shutdown";
    }
}

SystemLayoutSync::~SystemLayoutSync()
{
    restoreLayoutOsd();
}

bool SystemLayoutSync::readOsdEnabled() const
{
    KConfig config(QString::fromLatin1(s_plasmaConfig), KConfig::NoGlobals);
    return KConfigGroup(&config, QString::fromLatin1(s_osdGroup)).readEntry(QString::fromLatin1(s_osdKey), true);
}

void SystemLayoutSync::writeOsdEnabled(bool enabled)
{
    KConfig config(QString::fromLatin1(s_plasmaConfig), KConfig::NoGlobals);
    KConfigGroup group(&config, QString::fromLatin1(s_osdGroup));
    // KConfig::Notify makes Plasma re-read the value at once instead of on the
    // next restart, so the OSD is already gone when the layout is switched.
    group.writeEntry(QString::fromLatin1(s_osdKey), enabled, KConfig::Notify);
    config.sync();
}

QString SystemLayoutSync::osdMarkerPath()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (directory.isEmpty()) {
        directory = QDir::tempPath();
    }
    return directory + QStringLiteral("/plasma-keyboard-custom/layout-osd-suppressed");
}

void SystemLayoutSync::suppressLayoutOsd()
{
    if (m_osdSuppressed) {
        return;
    }
    m_osdOriginal = readOsdEnabled();
    QDir().mkpath(QFileInfo(osdMarkerPath()).absolutePath());
    QFile marker(osdMarkerPath());
    if (marker.open(QIODevice::WriteOnly | QIODevice::Text)) {
        marker.write(m_osdOriginal ? "true" : "false");
        marker.close();
    }
    writeOsdEnabled(false);
    m_osdSuppressed = true;
}

void SystemLayoutSync::restoreLayoutOsd()
{
    if (!m_osdSuppressed) {
        return;
    }
    writeOsdEnabled(m_osdOriginal);
    QFile::remove(osdMarkerPath());
    m_osdSuppressed = false;
}

void SystemLayoutSync::setKeyboardVisible(bool visible)
{
    if (m_keyboardVisible == visible) {
        return;
    }
    m_keyboardVisible = visible;
    if (visible) {
        suppressLayoutOsd();
    } else {
        restoreLayoutOsd();
    }
}

namespace
{
QStringList parseLayouts(const QDBusMessage &reply)
{
    QStringList names;
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return names;
    }
    // a(sss): short name, variant, display name.
    const QDBusArgument arg = reply.arguments().first().value<QDBusArgument>();
    arg.beginArray();
    while (!arg.atEnd()) {
        QString shortName;
        QString variant;
        QString displayName;
        arg.beginStructure();
        arg >> shortName >> variant >> displayName;
        arg.endStructure();
        names.append(shortName);
    }
    arg.endArray();
    return names;
}
}

// Never block the GUI thread on the layout daemon: every call is asynchronous
// and the continuation runs from the event loop.
void SystemLayoutSync::fetchLayouts(std::function<void(const QStringList &)> done)
{
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(layoutsCall(QStringLiteral("getLayoutsList"))), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher, done = std::move(done)] {
        done(parseLayouts(QDBusMessage(watcher->reply())));
        watcher->deleteLater();
    });
}

void SystemLayoutSync::fetchCurrentLayout(std::function<void(int)> done)
{
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(layoutsCall(QStringLiteral("getLayout"))), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher, done = std::move(done)] {
        const QDBusMessage reply = watcher->reply();
        done(reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty() ? static_cast<int>(reply.arguments().first().toUInt()) : -1);
        watcher->deleteLater();
    });
}

QString SystemLayoutSync::layoutForLocale(const QString &locale) const
{
    const QLocale qlocale(locale);
    if (qlocale.language() == QLocale::C) {
        return {};
    }
    if (qlocale.territory() != QLocale::AnyTerritory && locale.contains(QLatin1Char('_'))) {
        return QLocale::territoryToCode(qlocale.territory()).toLower();
    }
    if (qlocale.language() == QLocale::English) {
        return QStringLiteral("us");
    }
    return QLocale::languageToCode(qlocale.language()).toLower();
}

void SystemLayoutSync::syncToLocale(const QString &locale)
{
    const QString wanted = layoutForLocale(locale);
    if (wanted.isEmpty()) {
        return;
    }
    fetchLayouts([this, wanted, locale](const QStringList &names) {
        const int index = names.indexOf(wanted);
        if (index < 0) {
            return;
        }
        fetchCurrentLayout([this, wanted, locale, index](int current) {
            if (index == current) {
                return;
            }
            qCDebug(PlasmaKeyboard) << "switching the system layout to" << wanted << "for" << locale;
            QDBusMessage msg = layoutsCall(QStringLiteral("setLayout"));
            msg << static_cast<uint>(index);
            QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
        });
    });
}

void SystemLayoutSync::onLayoutChanged(uint index)
{
    fetchLayouts([this, index](const QStringList &names) {
        if (index < static_cast<uint>(names.size())) {
            Q_EMIT systemLayoutChanged(names.at(index));
        }
    });
}
