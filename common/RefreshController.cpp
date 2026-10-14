#include "RefreshController.h"

#include <QDateTime>
#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QUuid>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

RefreshController::RefreshController(QString frontend, QObject *parent)
    : QAbstractListModel(parent), m_frontend(std::move(frontend))
{
    // Formatting happens once, outside the measured run. Adjacent batches differ.
    m_pool.reserve(10000);
    for (int i = 0; i < 10000; ++i)
        m_pool.append(QString::number(i).rightJustified(8, QLatin1Char('0')));
    m_samples.reserve(SampleLimit);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &RefreshController::tick);
    m_statusTimer.setInterval(500);
    connect(&m_statusTimer, &QTimer::timeout, this, &RefreshController::publishStatus);
    configure(300, 10, 20, 100);
}

int RefreshController::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant RefreshController::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= count()) return {};
    return role == TextRole || role == Qt::DisplayRole ? QVariant(textAt(index.row())) : QVariant();
}

QHash<int, QByteArray> RefreshController::roleNames() const
{
    return {{TextRole, "labelText"}};
}

QString RefreshController::textAt(int row) const
{
    return row >= 0 && row < count() ? m_pool.at(m_values[size_t(row)]) : QString();
}

QString RefreshController::configurationSummary() const
{
    return QString("已应用：共 %1 个控件，每行 %2 个；每 %3 毫秒更新前 %4%（%5 个）控件")
        .arg(count()).arg(m_columns).arg(m_intervalMs).arg(m_percent)
        .arg(std::max(1, (count() * m_percent + 99) / 100));
}

bool RefreshController::configure(int count, int columns, int intervalMs, int percent)
{
    if (m_running) return false;
    if (count < 1 || count > 10000 || columns < 1 || columns > 50 ||
        intervalMs < 10 || intervalMs > 80 || percent < 1 || percent > 100) {
        setStatus("Invalid configuration: count 1-10000, columns 1-50, interval 10-80, change 1-100%.");
        return false;
    }
    beginResetModel();
    m_columns = columns;
    m_intervalMs = intervalMs;
    m_percent = percent;
    m_values.resize(size_t(count));
    for (int i = 0; i < count; ++i) m_values[size_t(i)] = i % m_pool.size();
    m_samples.clear();
    m_lastBatch = m_lastNs = m_skipped = m_durationNs = 0;
    endResetModel();
    emit configurationChanged();
    setStatus("Ready. Start after the initial layout is visible. Apply / Start clears previous samples.");
    return true;
}

void RefreshController::start()
{
    if (m_running) return;
    m_samples.clear();
    m_lastBatch = m_lastNs = m_skipped = m_durationNs = 0;
    for (int i = 0; i < count(); ++i) m_values[size_t(i)] = i % m_pool.size();
    emit dataChanged(index(0), index(count() - 1), {TextRole});
    m_clock.start();
    m_running = true;
    emit runningChanged();
    m_timer.start(m_intervalMs);
    m_statusTimer.start();
    publishStatus();
}

void RefreshController::tick()
{
    if (!m_running) return;
    if (m_samples.size() >= SampleLimit - 1) {
        stop();
        setStatus(m_status + " | Sample limit reached; stopped automatically.");
        return;
    }
    submit(m_clock.nsecsElapsed(), false);
}

void RefreshController::submit(qint64 nowNs, bool finalFlush)
{
    const qint64 periodNs = qint64(m_intervalMs) * 1000000;
    const qint64 batch = nowNs / periodNs;
    const qint64 skipped = std::max<qint64>(0, batch - m_lastBatch - 1);
    const bool updated = batch > m_lastBatch;
    QElapsedTimer cost;
    cost.start();
    if (updated) {
        const int changed = std::max(1, (count() * m_percent + 99) / 100);
        // Fixed prefix: spatially clustered partial updates, identical in both UIs.
        for (int row = 0; row < changed; ++row)
            m_values[size_t(row)] = int((batch + row) % m_pool.size());
        emit dataChanged(index(0), index(changed - 1), {TextRole});
    }
    const qint64 submitNs = cost.nsecsElapsed();
    m_samples.push_back({nowNs, nowNs - m_lastNs, batch, skipped, submitNs, finalFlush, updated});
    m_lastNs = nowNs;
    m_lastBatch = batch;
    m_skipped += skipped;
    m_durationNs = nowNs;
}

void RefreshController::stop()
{
    if (!m_running) return;
    m_timer.stop();
    m_statusTimer.stop();
    // Capture the latest scheduled value at Stop, without replaying old batches.
    submit(m_clock.nsecsElapsed(), true);
    m_running = false;
    emit runningChanged();
    publishStatus();
}

void RefreshController::setStatus(const QString &text)
{
    m_status = text;
    emit statusChanged();
}

void RefreshController::publishStatus()
{
    const qint64 elapsed = m_running ? m_clock.nsecsElapsed() : m_durationNs;
    QString message = QString("%1 | %2 s | batch %3 | first value %4 | samples %5 | skipped source batches %6")
        .arg(m_running ? "Running" : "Stopped")
        .arg(elapsed / 1e9, 0, 'f', 1).arg(m_lastBatch).arg(textAt(0))
        .arg(qulonglong(m_samples.size())).arg(m_skipped);
    // Sorting only after Stop; live reporting stays constant-cost.
    if (!m_running && !m_samples.empty()) {
        std::vector<qint64> submitTimes, gaps;
        for (const auto &sample : m_samples) {
            if (sample.finalFlush) continue;
            if (sample.updated) submitTimes.push_back(sample.submitNs);
            gaps.push_back(sample.gapNs);
        }
        if (!submitTimes.empty()) {
            std::sort(submitTimes.begin(), submitTimes.end());
            std::sort(gaps.begin(), gaps.end());
            const auto percentile = [](const auto &values, double p) {
                return values[size_t(std::ceil(p * values.size())) - 1] / 1e6;
            };
            message += QString("\nSubmit ms P50/P95/P99: %1 / %2 / %3 | callback gap ms P99/max: %4 / %5")
                .arg(percentile(submitTimes, .5), 0, 'f', 3)
                .arg(percentile(submitTimes, .95), 0, 'f', 3)
                .arg(percentile(submitTimes, .99), 0, 'f', 3)
                .arg(percentile(gaps, .99), 0, 'f', 3).arg(gaps.back() / 1e6, 0, 'f', 3);
        }
    }
    setStatus(message);
}

QString RefreshController::exportCsv()
{
    if (m_running || m_samples.empty()) {
        const QString error = "Stop a run before exporting samples.";
        setStatus(error);
        return error;
    }
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + "/RefreshComparison";
    if (!QDir().mkpath(folder)) {
        setStatus("Cannot create export directory: " + folder);
        return m_status;
    }
    const QString path = folder + "/" + m_frontend + "-"
        + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz") + "-"
        + QUuid::createUuid().toString(QUuid::Id128).left(8) + ".csv";
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setStatus("Export failed: " + file.errorString());
        return m_status;
    }
    QTextStream out(&file);
    out << "frontend,qt_version,count,columns,interval_ms,change_percent,elapsed_ns,callback_gap_ns,source_batch,skipped_batches,submit_ns,final_flush,updated\n";
    for (const auto &s : m_samples)
        out << m_frontend << ',' << qVersion() << ',' << count() << ',' << m_columns << ','
            << m_intervalMs << ',' << m_percent << ',' << s.elapsedNs << ',' << s.gapNs << ','
            << s.batch << ',' << s.skipped << ',' << s.submitNs << ',' << int(s.finalFlush)
            << ',' << int(s.updated) << '\n';
    out.flush();
    if (out.status() != QTextStream::Ok || !file.commit()) {
        setStatus("Export failed: " + file.errorString());
        return m_status;
    }
    setStatus("CSV saved: " + QDir::toNativeSeparators(path));
    return path;
}
