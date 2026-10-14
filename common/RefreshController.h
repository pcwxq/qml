#pragma once

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QTimer>
#include <QStringList>
#include <vector>

class RefreshController final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int count READ count NOTIFY configurationChanged)
    Q_PROPERTY(int columns READ columns NOTIFY configurationChanged)
    Q_PROPERTY(QString configurationSummary READ configurationSummary NOTIFY configurationChanged)
public:
    enum { TextRole = Qt::UserRole + 1 };
    explicit RefreshController(QString frontend, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool running() const { return m_running; }
    QString status() const { return m_status; }
    int count() const { return int(m_values.size()); }
    int columns() const { return m_columns; }
    QString configurationSummary() const;
    QString textAt(int row) const;
    Q_INVOKABLE bool configure(int count, int columns, int intervalMs, int percent);
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE QString exportCsv();
signals:
    void runningChanged();
    void configurationChanged();
    void statusChanged();
private:
    struct Sample {
        qint64 elapsedNs, gapNs, batch, skipped, submitNs;
        bool finalFlush, updated;
    };
    void tick();
    void submit(qint64 nowNs, bool finalFlush);
    void publishStatus();
    void setStatus(const QString &text);
    QString m_frontend;
    QString m_status;
    QStringList m_pool;
    std::vector<int> m_values;
    std::vector<Sample> m_samples;
    QTimer m_timer, m_statusTimer;
    QElapsedTimer m_clock;
    int m_columns = 10, m_intervalMs = 20, m_percent = 100;
    bool m_running = false;
    qint64 m_lastBatch = 0, m_lastNs = 0, m_skipped = 0, m_durationNs = 0;
    static constexpr size_t SampleLimit = 200000;
};
