#include "favorites.h"

#include "channelstore.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

using namespace Qt::Literals::StringLiterals;

Favorites &Favorites::instance()
{
    static Favorites s;
    return s;
}

Favorites::Favorites(QObject *parent) : QObject(parent)
{
    load();
}

QString Favorites::filePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
           QStringLiteral("/favorites.txt");
}

void Favorites::load()
{
    m_ids.clear();
    QFile f(filePath());
    if (f.open(QIODevice::ReadOnly)) {
        while (!f.atEnd()) {
            const QString line =
                QString::fromUtf8(f.readLine()).trimmed();
            if (!line.isEmpty())
                m_ids.append(line);
        }
    }
}

void Favorites::save() const
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    QFile f(dir + QStringLiteral("/favorites.txt"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        for (const QString &id : m_ids)
            f.write(id.toUtf8() + '\n');
    }
}

bool Favorites::contains(const QString &channelId) const
{
    return m_ids.contains(channelId);
}

void Favorites::toggle(const QString &channelId)
{
    if (channelId.isEmpty())
        return;
    if (!m_ids.removeAll(channelId))
        m_ids.prepend(channelId);
    save();
    emit changed();
}

QVariantList Favorites::list() const
{
    QVariantList out;
    for (const QString &id : m_ids) {
        QVariantMap row = ChannelStore::instance().channelById(id);
        if (row.isEmpty()) {
            row.insert("id"_L1, id);
            row.insert("name"_L1, id);
        }
        out.append(row);
    }
    return out;
}
