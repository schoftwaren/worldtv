#include "recents.h"

#include "channelstore.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kMax = 24;
}

Recents &Recents::instance()
{
    static Recents s;
    return s;
}

Recents::Recents(QObject *parent) : QObject(parent)
{
    load();
}

QString Recents::filePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
           QStringLiteral("/recents.txt");
}

void Recents::load()
{
    m_ids.clear();
    QFile f(filePath());
    if (f.open(QIODevice::ReadOnly)) {
        while (!f.atEnd()) {
            const QString line = QString::fromUtf8(f.readLine()).trimmed();
            if (!line.isEmpty())
                m_ids.append(line);
        }
    }
}

void Recents::save() const
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    QFile f(dir + QStringLiteral("/recents.txt"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        for (const QString &id : m_ids)
            f.write(id.toUtf8() + '\n');
    }
}

QVariantList Recents::list() const
{
    QVariantList out;
    for (const QString &id : m_ids) {
        QVariantMap row = ChannelStore::instance().channelById(id);
        if (!row.isEmpty())
            out.append(row);
    }
    return out;
}

void Recents::add(const QString &channelId)
{
    if (channelId.isEmpty())
        return;
    m_ids.removeAll(channelId);
    m_ids.prepend(channelId);
    while (m_ids.size() > kMax)
        m_ids.removeLast();
    save();
    emit changed();
}

void Recents::clear()
{
    if (m_ids.isEmpty())
        return;
    m_ids.clear();
    save();
    emit changed();
}
