/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <QElapsedTimer>
#include <QFile>
#include <QTest>

#include <cmath>
#include <string>
#include <vector>

#include "swipekb/language_engine.hpp"

namespace
{
//! The key grid of one language, laid out like the on-screen keyboard rows.
std::vector<swipekb::KeyRect> grid(const std::vector<std::string> &rows, float keySize = 100.f)
{
    std::vector<swipekb::KeyRect> keys;
    for (size_t row = 0; row < rows.size(); ++row) {
        const auto letters = QString::fromStdString(rows[row]).toUcs4();
        const float x0 = (1280.f - keySize * float(letters.size())) / 2.f;
        for (qsizetype i = 0; i < letters.size(); ++i) {
            keys.push_back({char32_t(letters[i]), x0 + keySize * float(i), keySize * float(row), keySize, keySize});
        }
    }
    return keys;
}

//! A finger path through the centres of the letters of @p word.
std::vector<swipekb::TracePoint> swipeThrough(const std::vector<swipekb::KeyRect> &keys, const std::string &word)
{
    std::vector<swipekb::TracePoint> trace;
    float time = 0.f;
    const auto letters = QString::fromStdString(word).toUcs4();
    for (const char32_t letter : letters) {
        const auto key = std::find_if(keys.begin(), keys.end(), [letter](const swipekb::KeyRect &k) {
            return k.letter == letter;
        });
        if (key == keys.end()) {
            return {};
        }
        const float cx = key->x + key->w / 2.f;
        const float cy = key->y + key->h / 2.f;
        if (trace.empty()) {
            trace.push_back({cx, cy, 0.f});
            continue;
        }
        const float px = trace.back().x;
        const float py = trace.back().y;
        for (int step = 1; step <= 8; ++step) {
            const float t = float(step) / 8.f;
            time += 60.f;
            trace.push_back({px + t * (cx - px), py + t * (cy - py), time});
        }
    }
    return trace;
}

QString dictionaryDir()
{
    return QStringLiteral(SWIPE_DICTIONARY_DIR);
}
} // namespace

class SwipeEngineTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void englishSwipe();
    void russianSwipe();
    void russianDictionaryIsSwipeable();
    void learnedWordWins();
    void decodeIsFast();
};

void SwipeEngineTest::englishSwipe()
{
    const QString dictionary = dictionaryDir() + QStringLiteral("/en.combined");
    if (!QFile::exists(dictionary)) {
        QSKIP("the English swipe dictionary was not generated");
    }

    swipekb::LanguageEngine engine;
    swipekb::LanguageEngineSetup setup;
    setup.language = "en";
    setup.dictionary = dictionary.toStdString();
    QVERIFY(engine.init(setup));

    const auto keys = grid({"qwertyuiop", "asdfghjkl", "zxcvbnm"});
    const auto trace = swipeThrough(keys, "hello");
    QVERIFY(!trace.empty());

    const auto words = engine.recognize(trace, keys, {}, 5);
    QVERIFY(!words.empty());
    QCOMPARE(QString::fromStdString(words.front().word), QStringLiteral("hello"));
}

void SwipeEngineTest::russianSwipe()
{
    const QString dictionary = dictionaryDir() + QStringLiteral("/ru.combined");
    if (!QFile::exists(dictionary)) {
        QSKIP("the Russian swipe dictionary was not generated");
    }

    swipekb::LanguageEngine engine;
    swipekb::LanguageEngineSetup setup;
    setup.language = "ru";
    setup.dictionary = dictionary.toStdString();
    QVERIFY(engine.init(setup));

    const auto keys = grid({"йцукенгшщзхъ", "фывапролджэ", "ячсмитьбю"});
    const auto trace = swipeThrough(keys, "привет");
    QVERIFY(!trace.empty());

    const auto words = engine.recognize(trace, keys, {}, 5);
    QVERIFY(!words.empty());
    QCOMPARE(QString::fromStdString(words.front().word), QStringLiteral("привет"));

    // The dictionary must be usable for a swipe: every letter has a key.
    QVERIFY(engine.hasWord("привет"));
}

void SwipeEngineTest::russianDictionaryIsSwipeable()
{
    const QString dictionary = dictionaryDir() + QStringLiteral("/ru.combined");
    if (!QFile::exists(dictionary)) {
        QSKIP("the Russian swipe dictionary was not generated");
    }

    QFile file(dictionary);
    QVERIFY(file.open(QIODevice::ReadOnly));
    int words = 0;
    while (!file.atEnd()) {
        const QByteArray line = file.readLine();
        if (!line.startsWith(" word=")) {
            continue;
        }
        const int comma = line.indexOf(',');
        QVERIFY(comma > 6);
        const QString word = QString::fromUtf8(line.mid(6, comma - 6));
        // «ё» is typed with the «е» key, so it never reaches the dictionary.
        QVERIFY2(!word.contains(QChar(u'ё')), qPrintable(word));
        for (const QChar character : word) {
            QVERIFY2(character.isLetter(), qPrintable(word));
        }
        const int frequencyAt = line.indexOf("f=");
        QVERIFY(frequencyAt > 0);
        const int frequencyEnd = line.indexOf(',', frequencyAt);
        const int frequency = line.mid(frequencyAt + 2, frequencyEnd - frequencyAt - 2).toInt();
        QVERIFY2(frequency >= 1 && frequency <= 255, qPrintable(word));
        ++words;
    }
    // A handful of words would not be a swipe dictionary.
    QVERIFY(words > 10000);
}

void SwipeEngineTest::learnedWordWins()
{
    const QString dictionary = dictionaryDir() + QStringLiteral("/ru.combined");
    if (!QFile::exists(dictionary)) {
        QSKIP("the Russian swipe dictionary was not generated");
    }

    swipekb::LanguageEngine engine;
    swipekb::LanguageEngineSetup setup;
    setup.language = "ru";
    setup.dictionary = dictionary.toStdString();
    QVERIFY(engine.init(setup));

    // A word that was typed many times is offered like any other word.
    engine.setLearnedWords({{"приветик", 255}});
    QVERIFY(engine.hasWord("приветик"));

    // The path of the learned word itself: without the learned list the word
    // may not be in the dictionary at all, with it the word must be found.
    const auto keys = grid({"йцукенгшщзхъ", "фывапролджэ", "ячсмитьбю"});
    const auto trace = swipeThrough(keys, "приветик");
    const auto words = engine.recognize(trace, keys, {}, 5);
    const auto learned = std::find_if(words.begin(), words.end(), [](const swipekb::Candidate &candidate) {
        return candidate.word == "приветик";
    });
    QVERIFY2(learned != words.end(), "the learned word is not among the guesses");
}

void SwipeEngineTest::decodeIsFast()
{
    const QString dictionary = dictionaryDir() + QStringLiteral("/en.combined");
    if (!QFile::exists(dictionary)) {
        QSKIP("the English swipe dictionary was not generated");
    }

    swipekb::LanguageEngine engine;
    swipekb::LanguageEngineSetup setup;
    setup.language = "en";
    setup.dictionary = dictionary.toStdString();
    QVERIFY(engine.init(setup));

    const auto keys = grid({"qwertyuiop", "asdfghjkl", "zxcvbnm"});
    const auto trace = swipeThrough(keys, "hello");

    // One warm-up decode builds the lexicon; the measured ones use it.
    engine.recognize(trace, keys, {}, 5);

    QElapsedTimer timer;
    timer.start();
    const int runs = 20;
    for (int i = 0; i < runs; ++i) {
        const auto words = engine.recognize(trace, keys, {}, 5);
        QVERIFY(!words.empty());
    }
    const qint64 average = timer.elapsed() / runs;
    qInfo() << "average decode time:" << average << "ms";
    QVERIFY2(average < 30, qPrintable(QStringLiteral("decoding takes %1 ms").arg(average)));
}

QTEST_MAIN(SwipeEngineTest)
#include "swipeenginetest.moc"
