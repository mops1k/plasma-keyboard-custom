/*
    SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "asyncpredictor.h"
#include "predictivedictionary.h"
#include "wordpredictor.h"

#include <QThread>
#include <QVariantMap>
#include <algorithm>
#include <memory>
#include <utility>

class PredictionWorker : public QObject
{
    Q_OBJECT
public:
    explicit PredictionWorker(AsyncPredictor::Backend backend)
        : m_backend(std::move(backend))
    {
    }

    void run(quint64 generation, const AsyncPredictor::Request &request)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        const auto result = m_backend ? m_backend(request) : calculate(request);
        Q_EMIT done(generation, result.words, result.kind);
    }

Q_SIGNALS:
    void done(quint64 generation, const QStringList &words, const QString &kind);

private:
    AsyncPredictor::Result calculate(const AsyncPredictor::Request &request)
    {
        // Construct, use and destroy every dictionary (including its Hunspell
        // handles) on this thread. Loading is lazy, never in the GUI constructor.
        if (!m_dictionary) {
            m_dictionary = std::make_unique<PredictiveDictionary>();
            m_nextWord = std::make_unique<WordPredictor>();
        }
        if (request.prefix.size() >= request.minPrefixLength) {
            auto words = m_dictionary->complete(request.prefix, request.limit, request.locale);
            if (!words.isEmpty()) {
                return {words, QStringLiteral("complete")};
            }
            if (request.correction && request.prefix.size() >= 2) {
                return {m_dictionary->correct(request.prefix, request.limit, request.locale), QStringLiteral("correct")};
            }
        } else if (request.prefix.isEmpty() && request.nextWord) {
            return {m_nextWord->predict(request.context, request.limit, request.locale), QStringLiteral("next")};
        }
        return {};
    }

    AsyncPredictor::Backend m_backend;
    std::unique_ptr<PredictiveDictionary> m_dictionary;
    std::unique_ptr<WordPredictor> m_nextWord;
};

AsyncPredictor::AsyncPredictor(QObject *parent)
    : AsyncPredictor(Backend{}, parent)
{
}

AsyncPredictor::AsyncPredictor(Backend backend, QObject *parent)
    : QObject(parent)
    , m_thread(new QThread)
    , m_worker(new PredictionWorker(std::move(backend)))
{
    m_thread->setObjectName(QStringLiteral("keyboard-prediction"));
    m_worker->moveToThread(m_thread);
    connect(m_worker, &PredictionWorker::done, this, &AsyncPredictor::finished, Qt::QueuedConnection);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    m_thread->start();
}

AsyncPredictor::~AsyncPredictor()
{
    // Hunspell calls cannot be interrupted safely. Let the one in-flight call
    // finish and let the thread clean itself up; never wait on the input thread.
    disconnect(m_worker, nullptr, this, nullptr);
    m_thread->quit();
}

QVariantList AsyncPredictor::predictions() const
{
    return m_predictions;
}

void AsyncPredictor::clear()
{
    if (!m_predictions.isEmpty()) {
        m_predictions.clear();
        Q_EMIT predictionsChanged();
    }
}

void AsyncPredictor::cancel()
{
    ++m_generation;
    m_pending.reset();
    clear();
}

void AsyncPredictor::request(const QString &prefix,
                             const QString &context,
                             const QString &locale,
                             int limit,
                             int minPrefixLength,
                             bool correction,
                             bool nextWord)
{
    cancel();
    m_typedLength = prefix.size();
    if (limit <= 0 || (prefix.size() < std::max(1, minPrefixLength) && !(prefix.isEmpty() && nextWord))) {
        return;
    }
    const Request request{prefix, context, locale, std::min(limit, 32), std::max(1, minPrefixLength), correction, nextWord};
    if (m_busy) {
        m_pending = request;
    } else {
        dispatch(request);
    }
}

void AsyncPredictor::dispatch(const Request &request)
{
    m_busy = true;
    const auto generation = m_generation;
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, generation, request] {
            worker->run(generation, request);
        },
        Qt::QueuedConnection);
}

void AsyncPredictor::finished(quint64 generation, const QStringList &words, const QString &kind)
{
    m_busy = false;
    if (generation == m_generation) {
        for (const auto &word : words) {
            m_predictions.append(QVariantMap{
                {QStringLiteral("text"), word},
                {QStringLiteral("kind"), kind},
                {QStringLiteral("completion"), kind == QLatin1String("complete") ? std::max(0, int(word.size()) - m_typedLength) : 0},
            });
        }
        Q_EMIT predictionsChanged();
    }
    if (m_pending) {
        const auto request = std::exchange(m_pending, std::nullopt);
        dispatch(*request);
    }
}

#include "asyncpredictor.moc"
