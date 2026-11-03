/*
    SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "asyncpredictor.h"
#include "predictivedictionary.h"
#include "wordpredictor.h"

#include <QElapsedTimer>
#include <QSemaphore>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <memory>

class AsyncPredictorTest : public QObject
{
    Q_OBJECT
    struct WorkState {
        QSemaphore gate;
        std::atomic_int calls = 0;
        std::atomic_bool workerThread = false;
        std::atomic<QThread *> worker = nullptr;
    };
private Q_SLOTS:
    void responsiveAndCoalesced()
    {
        auto state = std::make_shared<WorkState>();
        AsyncPredictor predictor([state, mainThread = thread()](const AsyncPredictor::Request &request) {
            state->workerThread = QThread::currentThread() != mainThread;
            if (++state->calls == 1) {
                state->gate.tryAcquire(1, 3000);
            }
            return AsyncPredictor::Result{{request.prefix + request.locale}, QStringLiteral("complete")};
        });
        predictor.request(QStringLiteral("old"), {}, QStringLiteral("en_US"), 5, 2, true, true);
        QTRY_COMPARE(state->calls.load(), 1);
        QVERIFY(state->workerThread.load());

        int ticks = 0;
        QTimer timer;
        connect(&timer, &QTimer::timeout, this, [&] {
            ++ticks;
        });
        timer.start(1);
        for (int i = 0; i < 100; ++i) {
            predictor.request(QStringLiteral("discard%1").arg(i), {}, QStringLiteral("en_US"), 5, 2, true, true);
        }
        predictor.request(QStringLiteral("latest"), {}, QStringLiteral("uk_UA"), 5, 2, true, true);
        QTest::qWait(30);
        QVERIFY(ticks > 0);
        QCOMPARE(state->calls.load(), 1);
        QVERIFY(predictor.predictions().isEmpty());
        QSignalSpy results(&predictor, &AsyncPredictor::predictionsChanged);
        state->gate.release();
        QTRY_COMPARE(predictor.predictions().size(), 1);
        QCOMPARE(state->calls.load(), 2);
        QCOMPARE(results.count(), 1); // The old locale/prefix was never published.
        const auto item = predictor.predictions().first().toMap();
        QCOMPARE(item.value(QStringLiteral("text")).toString(), QStringLiteral("latestuk_UA"));
        QCOMPARE(item.value(QStringLiteral("completion")).toInt(), 5);
    }

    void cancelDropsPendingAndResult()
    {
        auto state = std::make_shared<WorkState>();
        AsyncPredictor predictor([state](const AsyncPredictor::Request &) {
            ++state->calls;
            state->gate.tryAcquire(1, 3000);
            return AsyncPredictor::Result{{QStringLiteral("obsolete")}, QStringLiteral("correct")};
        });
        predictor.request(QStringLiteral("old"), {}, QStringLiteral("en_US"), 5, 2, true, true);
        QTRY_COMPARE(state->calls.load(), 1);
        predictor.request(QStringLiteral("pending"), {}, QStringLiteral("en_US"), 5, 2, true, true);
        predictor.cancel(); // Hidden panel, disabled setting or lost focus.
        QSignalSpy results(&predictor, &AsyncPredictor::predictionsChanged);
        state->gate.release();
        QTest::qWait(100);
        QCOMPARE(state->calls.load(), 1);
        QCOMPARE(results.count(), 0);
        QVERIFY(predictor.predictions().isEmpty());
        // The facade can be reused after cancellation.
        state->gate.release();
        predictor.request(QStringLiteral("new"), {}, QStringLiteral("uk_UA"), 5, 2, true, true);
        QTRY_COMPARE(predictor.predictions().size(), 1);
    }

    void destructionDoesNotWait()
    {
        auto state = std::make_shared<WorkState>();
        auto predictor = std::make_unique<AsyncPredictor>([state](const AsyncPredictor::Request &) {
            state->worker = QThread::currentThread();
            state->gate.tryAcquire(1, 3000);
            return AsyncPredictor::Result{};
        });
        predictor->request(QStringLiteral("slow"), {}, QStringLiteral("en_US"), 5, 2, true, true);
        QTRY_VERIFY(state->worker.load());
        QSignalSpy finished(state->worker.load(), &QThread::finished);
        QElapsedTimer elapsed;
        elapsed.start();
        predictor.reset();
        QVERIFY(elapsed.elapsed() < 250);
        state->gate.release();
        QTRY_COMPARE(finished.count(), 1);
    }

    void noWorkBelowThreshold()
    {
        auto state = std::make_shared<WorkState>();
        AsyncPredictor predictor([state](const AsyncPredictor::Request &) {
            ++state->calls;
            return AsyncPredictor::Result{};
        });
        predictor.request(QStringLiteral("a"), {}, QStringLiteral("en_US"), 5, 2, true, true);
        predictor.request({}, {}, QStringLiteral("en_US"), 5, 2, true, false);
        predictor.request(QStringLiteral("valid"), {}, QStringLiteral("en_US"), 0, 2, true, true);
        QTest::qWait(30);
        QCOMPARE(state->calls.load(), 0);
    }

    void realBackend()
    {
        // Synthetic test inputs only; no live input, clipboard or desktop bus.
        PredictiveDictionary dictionary;
        WordPredictor nextWord;
        AsyncPredictor predictor;
        QSignalSpy results(&predictor, &AsyncPredictor::predictionsChanged);
        const auto check = [&](const QString &prefix, const QString &context, const QStringList &expected, const QString &kind) {
            predictor.request(prefix, context, QStringLiteral("en_US"), 5, 2, true, true);
            const int before = results.count();
            QTRY_VERIFY(results.count() > before);
            QStringList actual;
            for (const auto &item : predictor.predictions()) {
                actual.append(item.toMap().value(QStringLiteral("text")).toString());
                QCOMPARE(item.toMap().value(QStringLiteral("kind")).toString(), kind);
            }
            QCOMPARE(actual, expected);
        };
        check(QStringLiteral("hel"), {}, dictionary.complete(QStringLiteral("hel"), 5, QStringLiteral("en_US")), QStringLiteral("complete"));
        // A trailing extra character ensures no completion masks correction.
        QVERIFY(dictionary.complete(QStringLiteral("hello!"), 5, QStringLiteral("en_US")).isEmpty());
        const auto corrections = dictionary.correct(QStringLiteral("hello!"), 5, QStringLiteral("en_US"));
        QVERIFY(!corrections.isEmpty());
        check(QStringLiteral("hello!"), {}, corrections, QStringLiteral("correct"));
        const auto next = nextWord.predict(QStringLiteral("thank"), 5, QStringLiteral("en_US"));
        QVERIFY(!next.isEmpty());
        check({}, QStringLiteral("thank"), next, QStringLiteral("next"));
        predictor.cancel();
        QVERIFY(predictor.predictions().isEmpty());
    }
};

QTEST_GUILESS_MAIN(AsyncPredictorTest)
#include "asyncpredictortest.moc"
