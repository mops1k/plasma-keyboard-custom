/*
    SPDX-FileCopyrightText: 2026 Chechulin Serhii

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>
#include <functional>
#include <QString>
#include <QStringList>

/**
 * Keeps the system keyboard layout (org.kde.keyboard) and the language of the
 * on-screen keyboard in step.
 *
 * For a window without text-input support (e.g. an XWayland application) KWin
 * turns the committed text into key presses of the *current system layout*, so
 * a Ukrainian letter typed while the system layout is "us" produces nothing.
 * The layout matching a locale is found from its territory ("uk_UA" -> "ua",
 * "en_US" -> "us"), falling back to its language code.
 */
class SystemLayoutSync : public QObject
{
    Q_OBJECT

public:
    explicit SystemLayoutSync(QObject *parent = nullptr);

    //! Switches the system layout to the one matching @p locale, if there is
    //! one and it is not active already.
    Q_INVOKABLE void syncToLocale(const QString &locale);

    //! The system layout name @p locale corresponds to ("ua" for "uk_UA").
    Q_INVOKABLE QString layoutForLocale(const QString &locale) const;

Q_SIGNALS:
    //! The system layout was switched (by the user or by syncToLocale()).
    void systemLayoutChanged(const QString &layout);

private Q_SLOTS:
    void onLayoutChanged(uint index);

private:
    void fetchLayouts(std::function<void(const QStringList &)> done);
    void fetchCurrentLayout(std::function<void(int)> done);
};
