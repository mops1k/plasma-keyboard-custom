/*
    SPDX-FileCopyrightText: 2026 Chechulin Serhii

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "systemlayoutsync.h"
#include "logging.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QLocale>

namespace
{
constexpr auto s_service = "org.kde.keyboard";
constexpr auto s_path = "/Layouts";
constexpr auto s_iface = "org.kde.KeyboardLayouts";

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
}

QStringList SystemLayoutSync::layouts() const
{
    const QDBusMessage reply = QDBusConnection::sessionBus().call(layoutsCall(QStringLiteral("getLayoutsList")));
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

int SystemLayoutSync::currentLayout() const
{
    const QDBusMessage reply = QDBusConnection::sessionBus().call(layoutsCall(QStringLiteral("getLayout")));
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return -1;
    }
    return static_cast<int>(reply.arguments().first().toUInt());
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
    const QStringList names = layouts();
    const int index = names.indexOf(wanted);
    if (index < 0 || index == currentLayout()) {
        return;
    }
    qCDebug(PlasmaKeyboard) << "switching the system layout to" << wanted << "for" << locale;
    QDBusMessage msg = layoutsCall(QStringLiteral("setLayout"));
    msg << static_cast<uint>(index);
    QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
}

void SystemLayoutSync::onLayoutChanged(uint index)
{
    const QStringList names = layouts();
    if (index < static_cast<uint>(names.size())) {
        Q_EMIT systemLayoutChanged(names.at(index));
    }
}
