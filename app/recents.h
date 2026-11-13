#pragma once

#include <QObject>
#include <QVariantList>

// Recent channels (session + persisted history, newest first). Powers the
// "Recent" rail so the couch user gets back to what was last watched.
class Recents : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList list READ list NOTIFY changed)

public:
    static Recents &instance();

    QVariantList list() const;
    Q_INVOKABLE void add(const QString &channelId);
    Q_INVOKABLE void clear();

    void load();
    void save() const;

signals:
    void changed();

private:
    explicit Recents(QObject *parent = nullptr);
    QString filePath() const;

    QStringList m_ids;
};
