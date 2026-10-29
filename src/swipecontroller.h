/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantList>

#include <memory>
#include <qqmlintegration.h>

namespace swipekb
{
class LanguageEngine;
}

/**
 * Swipe (glide) typing: turns the finger path and the letter-key rectangles
 * into ranked words with the vendored MIT engine (src/swipekb).
 *
 * One language engine per language, built on first use: English loads the
 * neural swipe model, Russian is decoded by geometry over the Russian word
 * list. The word lists are AOSP .combined files generated at build time from
 * the predictive word lists and installed to
 * <datadir>/plasma-keyboard-custom/swipe/.
 *
 * The engine is not thread-safe; every call happens on the GUI thread and a
 * decode takes a few milliseconds.
 */
class SwipeController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit SwipeController(QObject *parent = nullptr);
    ~SwipeController() override;

    /**
     * Whether a word list for @p locale is installed, so a swipe can be typed
     * in that language.
     */
    Q_INVOKABLE bool supports(const QString &locale) const;

    /**
     * The words the finger path may have spelled, best first.
     *
     * @param locale The input locale ("ru_RU", "en_US", ...); a language
     *               without an installed word list has no swipe typing.
     * @param trace Points of the path: records with x, y (window pixels) and t
     *              (milliseconds since the swipe started).
     * @param keys  The letter keys of the current layout: records with letter,
     *              x, y, w and h in the same pixels.
     * @param context The previous word, when there is one (the engine's
     *              sentence prior is not loaded yet, so it only matters later).
     * @param limit At most this many words.
     * @return Ranked words, or an empty list when nothing could be recognised.
     */
    Q_INVOKABLE QStringList recognize(const QString &locale, const QVariantList &trace, const QVariantList &keys, const QStringList &context, int limit);

    /**
     * Drops the engines of the languages that are no longer needed (the
     * layout changed, the keyboard was hidden).
     */
    Q_INVOKABLE void reset();

private:
    swipekb::LanguageEngine *engineFor(const QString &locale);

    static QString languageOf(const QString &locale);
    static QString dataFile(const QString &name);

    QHash<QString, std::shared_ptr<swipekb::LanguageEngine>> m_engines;
};
