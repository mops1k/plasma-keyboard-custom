/*
    SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <functional>
#include <optional>
#include <qqmlintegration.h>

class QThread;
class PredictionWorker;

/** Main-thread facade. No dictionary access or waiting is allowed here.
 * At most one request runs and one (replaceable) request waits. Each change
 * invalidates the displayed result, including locale, settings and focus.
 */
class AsyncPredictor : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList predictions READ predictions NOTIFY predictionsChanged)

public:
    struct Request {
        QString prefix;
        QString context;
        QString locale;
        int limit = 0;
        int minPrefixLength = 2;
        bool correction = false;
        bool nextWord = false;
    };
    struct Result {
        QStringList words;
        QString kind;
    };
    // Injectable backend for deterministic slow-work regression tests. It is
    // called and destroyed exclusively on the worker thread.
    using Backend = std::function<Result(const Request &)>;

    explicit AsyncPredictor(QObject *parent = nullptr);
    explicit AsyncPredictor(Backend backend, QObject *parent = nullptr);
    ~AsyncPredictor() override;

    QVariantList predictions() const;
    Q_INVOKABLE void
    request(const QString &prefix, const QString &context, const QString &locale, int limit, int minPrefixLength, bool correction, bool nextWord);
    Q_INVOKABLE void cancel();

Q_SIGNALS:
    void predictionsChanged();

private:
    void dispatch(const Request &request);
    void finished(quint64 generation, const QStringList &words, const QString &kind);
    void clear();

    QThread *m_thread;
    PredictionWorker *m_worker;
    QVariantList m_predictions;
    std::optional<Request> m_pending;
    quint64 m_generation = 0;
    int m_typedLength = 0;
    bool m_busy = false;
};
