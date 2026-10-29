/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "swipecontroller.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

#include "swipekb/language_engine.hpp"
#include "swipekb/text.hpp"

Q_LOGGING_CATEGORY(lcSwipe, "org.kde.plasma.keyboard.custom.swipe")

namespace
{
//! The languages with a generated word list. Keep in sync with _swipe_languages
//! in src/swipekb/CMakeLists.txt.
const QStringList supportedLanguages = {QStringLiteral("en"), QStringLiteral("ru")};

QString installedPath(const QString &fileName)
{
    return QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("plasma-keyboard-custom/swipe/") + fileName);
}
} // namespace

SwipeController::SwipeController(QObject *parent)
    : QObject(parent)
    , m_saveTimer(new QTimer(this))
{
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(2000);
    connect(m_saveTimer, &QTimer::timeout, this, [this] {
        for (auto it = m_learned.cbegin(); it != m_learned.cend(); ++it) {
            saveLearned(it.key());
        }
    });
}

SwipeController::~SwipeController() = default;

QString SwipeController::languageOf(const QString &locale)
{
    const int separator = locale.indexOf(QLatin1Char('_'));
    const QString language = (separator > 0 ? locale.left(separator) : locale).toLower();
    return supportedLanguages.contains(language) ? language : QString();
}

QString SwipeController::dataFile(const QString &name)
{
    return installedPath(name);
}

bool SwipeController::supports(const QString &locale) const
{
    const QString language = languageOf(locale);
    if (language.isEmpty()) {
        return false;
    }
    return QFileInfo::exists(installedPath(language + QStringLiteral(".combined")));
}

swipekb::LanguageEngine *SwipeController::engineFor(const QString &locale)
{
    const QString language = languageOf(locale);
    if (language.isEmpty()) {
        return nullptr;
    }

    const auto cached = m_engines.constFind(language);
    if (cached != m_engines.constEnd()) {
        return cached->get();
    }

    const QString dictionary = installedPath(language + QStringLiteral(".combined"));
    if (dictionary.isEmpty()) {
        qCWarning(lcSwipe) << "no swipe dictionary for" << language;
        return nullptr;
    }

    auto engine = std::make_shared<swipekb::LanguageEngine>();
    swipekb::LanguageEngineSetup setup;
    setup.language = language.toStdString();
    setup.dictionary = dictionary.toStdString();
    // The neural model covers English. Russian is decoded by geometry over its
    // word list, which needs no model.
    const QString model = language == QLatin1String("en") ? installedPath(QStringLiteral("swipe-model.bin")) : QString();
    if (!model.isEmpty()) {
        setup.swipeModel = model.toStdString();
    }

    QElapsedTimer timer;
    timer.start();
    if (!engine->init(setup)) {
        qCWarning(lcSwipe) << "cannot initialise the swipe engine for" << language << dictionary;
        return nullptr;
    }
    loadLearned(language);
    engine->setLearnedWords(learnedWordsFor(language));
    qCDebug(lcSwipe) << "swipe engine ready for" << language << "mode" << engine->mode() << "in" << timer.elapsed() << "ms";

    m_engines.insert(language, engine);
    return engine.get();
}

QStringList SwipeController::recognize(const QString &locale, const QVariantList &trace, const QVariantList &keys, const QStringList &context, int limit)
{
    if (trace.size() < 2 || keys.size() < 2 || limit <= 0) {
        return {};
    }

    swipekb::LanguageEngine *engine = engineFor(locale);
    if (!engine) {
        return {};
    }

    std::vector<swipekb::TracePoint> points;
    points.reserve(trace.size());
    const qint64 start = trace.first().toMap().value(QStringLiteral("t")).toLongLong();
    for (const QVariant &item : trace) {
        const QVariantMap point = item.toMap();
        points.push_back({float(point.value(QStringLiteral("x")).toDouble()),
                          float(point.value(QStringLiteral("y")).toDouble()),
                          float(point.value(QStringLiteral("t")).toLongLong() - start)});
    }

    std::vector<swipekb::KeyRect> rects;
    rects.reserve(keys.size());
    for (const QVariant &item : keys) {
        const QVariantMap key = item.toMap();
        const QString letter = key.value(QStringLiteral("letter")).toString();
        if (letter.isEmpty()) {
            continue;
        }
        // QML gives the centre of a key; the engine wants its top-left corner
        // (KeyRect::x/y are the corner, the centre is derived as x + w / 2).
        const double width = key.value(QStringLiteral("w")).toDouble();
        const double height = key.value(QStringLiteral("h")).toDouble();
        rects.push_back({letter.at(0).toLower().unicode(),
                         float(key.value(QStringLiteral("x")).toDouble() - width / 2),
                         float(key.value(QStringLiteral("y")).toDouble() - height / 2),
                         float(width),
                         float(height)});
    }
    if (rects.size() < 2) {
        return {};
    }

    std::vector<std::string> words;
    words.reserve(context.size());
    for (const QString &word : context) {
        if (!word.isEmpty()) {
            words.push_back(word.toStdString());
        }
    }

    QElapsedTimer timer;
    timer.start();
    const std::vector<swipekb::Candidate> candidates = engine->recognize(points, rects, words, limit);
    const qint64 elapsed = timer.elapsed();

    // Debug aid: with the swipe logging category enabled, keep the last gesture
    // so it can be replayed outside the keyboard (see .dsh/tmp/swipeprobe).
    if (lcSwipe().isDebugEnabled()) {
        QJsonObject dump;
        dump.insert(QStringLiteral("locale"), locale);
        QJsonArray traceArray;
        for (const auto &point : points) {
            traceArray.append(QJsonObject{{QStringLiteral("x"), point.x}, {QStringLiteral("y"), point.y}, {QStringLiteral("t"), point.t_ms}});
        }
        dump.insert(QStringLiteral("trace"), traceArray);
        QJsonArray keyArray;
        for (const auto &rect : rects) {
            keyArray.append(QJsonObject{{QStringLiteral("letter"), QString::fromUcs4(&rect.letter, 1)},
                                        {QStringLiteral("x"), rect.x},
                                        {QStringLiteral("y"), rect.y},
                                        {QStringLiteral("w"), rect.w},
                                        {QStringLiteral("h"), rect.h}});
        }
        dump.insert(QStringLiteral("keys"), keyArray);

        QFile file(QStringLiteral("/tmp/plasma-keyboard-swipe-last.json"));
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(dump).toJson(QJsonDocument::Compact));
        }
    }

    QStringList result;
    result.reserve(int(candidates.size()));
    for (const swipekb::Candidate &candidate : candidates) {
        result.append(QString::fromStdString(candidate.word));
    }
    qCDebug(lcSwipe) << "swipe decoded" << result.size() << "words in" << elapsed << "ms";
    return result;
}

void SwipeController::reset()
{
    m_engines.clear();
}

QString SwipeController::learnedPath(const QString &language)
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/plasma-keyboard-custom/swipe");
    return directory + QStringLiteral("/learned-") + language + QStringLiteral(".json");
}

void SwipeController::loadLearned(const QString &language)
{
    if (m_learned.contains(language)) {
        return;
    }

    QHash<QString, int> words;
    QFile file(learnedPath(language));
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonObject stored = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("words")).toObject();
        for (auto it = stored.begin(); it != stored.end(); ++it) {
            const int count = it.value().toInt();
            if (count > 0) {
                words.insert(it.key(), count);
            }
        }
        qCDebug(lcSwipe) << "learned words for" << language << words.size();
    }
    m_learned.insert(language, words);
}

void SwipeController::saveLearned(const QString &language)
{
    const QString path = learnedPath(language);
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject words;
    const auto known = m_learned.constFind(language);
    if (known != m_learned.constEnd()) {
        for (auto it = known->begin(); it != known->end(); ++it) {
            words.insert(it.key(), it.value());
        }
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcSwipe) << "cannot write the learned words" << path;
        return;
    }
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("words"), words}}).toJson(QJsonDocument::Compact));
    if (file.commit()) {
        // The words are personal: keep the file readable only by its owner.
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
}

std::vector<std::pair<std::string, int>> SwipeController::learnedWordsFor(const QString &language) const
{
    std::vector<std::pair<std::string, int>> words;
    const auto known = m_learned.constFind(language);
    if (known == m_learned.constEnd()) {
        return words;
    }
    words.reserve(size_t(known->size()));
    for (auto it = known->begin(); it != known->end(); ++it) {
        // A typed word is worth more than a rare word of the list from the
        // first use, and grows with every use, up to the top of the scale.
        const int frequency = std::clamp(int(80 + 25 * std::log2(it.value() + 1)), 80, 255);
        words.emplace_back(it.key().toStdString(), frequency);
    }
    return words;
}

void SwipeController::learnWord(const QString &word, const QString &locale)
{
    const QString language = languageOf(locale);
    if (language.isEmpty() || word.isEmpty()) {
        return;
    }

    // Only letters: the engine rejects anything else, and punctuation would
    // make one word of «слово,» and «слово».
    QString cleaned;
    cleaned.reserve(word.size());
    for (const QChar character : word.toLower()) {
        if (character.isLetter()) {
            cleaned.append(character);
        }
    }
    if (cleaned.isEmpty()) {
        return;
    }

    loadLearned(language);
    QHash<QString, int> &words = m_learned[language];
    words.insert(cleaned, std::min(words.value(cleaned) + 1, 255));

    if (swipekb::LanguageEngine *engine = engineFor(locale)) {
        engine->setLearnedWords(learnedWordsFor(language));
    }
    // Write once the person stops typing rather than on every word.
    m_saveTimer->start();
}

void SwipeController::clearLearnedWords(const QString &locale)
{
    const QString language = languageOf(locale);
    if (language.isEmpty()) {
        return;
    }
    m_learned.remove(language);
    QFile::remove(learnedPath(language));
    if (swipekb::LanguageEngine *engine = engineFor(locale)) {
        engine->setLearnedWords({});
    }
}
