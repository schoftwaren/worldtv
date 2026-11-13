#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

// Persistent favorites (starred channels). QSettings-free: a plain text file
// under AppDataLocation, one channel id per line — trivially inspectable and
// robust across Qt versions. Ordered newest-first.
class Favorites : public QObject {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(QVariantList list READ list NOTIFY changed)

public:
    static Favorites &instance();

    bool contains(const QString &channelId) const;
    int count() const { return int(m_ids.size()); }
    QVariantList list() const; // [{id, name, cc}] via ChannelStore lookup

    Q_INVOKABLE void toggle(const QString &channelId);
    Q_INVOKABLE bool isFav(const QString &channelId) const { return contains(channelId); }

    void load();
    void save() const;

signals:
    void changed();

private:
    explicit Favorites(QObject *parent = nullptr);
    QString filePath() const;

    QStringList m_ids;
};
