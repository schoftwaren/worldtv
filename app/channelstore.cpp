#include "channelstore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>

#include <algorithm>
#include <set>

#include "favorites.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr auto kApiChannels =
    "https://iptv-org.github.io/api/channels.json"_L1;
constexpr auto kApiStreams = "https://iptv-org.github.io/api/streams.json"_L1;
constexpr auto kCatalogRes = "qrc:/worldtv/catalog.json"_L1;

int qualityRank(const QString &q)
{
    static const QHash<QString, int> rank = {
        {u"2160p"_s, 7}, {u"1080p"_s, 6}, {u"720p"_s, 5}, {u"480p"_s, 4},
        {u"360p"_s, 3},  {u"270p"_s, 2},  {u"180p"_s, 1},
    };
    return rank.value(q, 0);
}

// In-process merge of the two raw api dumps -> the same shape prepdata.py
// writes. Kept in one place so refresh() and the python tool agree.
QJsonDocument buildCatalog(const QJsonDocument &channelsDoc,
                           const QJsonDocument &streamsDoc,
                           const QJsonObject &countryMeta)
{
    const auto channels = channelsDoc.array();
    const auto streams = streamsDoc.array();

    QHash<QString, QJsonObject> byId;
    byId.reserve(channels.size());
    for (const auto &v : channels)
        byId.insert(v.toObject().value("id"_L1).toString(), v.toObject());

    // channel -> best (rank, stream) + merged url list
    struct Entry {
        QJsonObject ch;
        QVector<QJsonObject> urls;
    };
    QHash<QString, Entry> entries;
    entries.reserve(4096);
    for (const auto &v : streams) {
        const QJsonObject s = v.toObject();
        const QString cid = s.value("channel"_L1).toString();
        const auto it = byId.constFind(cid);
        if (it == byId.constEnd())
            continue;
        const QJsonObject ch = it.value();
        if (ch.value("is_nsfw"_L1).toBool())
            continue;
        const auto cats = ch.value("categories"_L1).toArray();
        bool xxx = false;
        for (int i = 0; i < cats.size(); ++i)
            if (cats.at(i).toString() == "xxx"_L1)
                xxx = true;
        if (xxx)
            continue;
        Entry &e = entries[cid];
        e.ch = ch;
        e.urls.append(s);
    }

    QJsonArray outChans;
    QHash<QString, int> counts;
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        const QJsonObject ch = it.value().ch;
        QString name = ch.value("name"_L1).toString().trimmed();
        if (name.isEmpty())
            continue;
        const QString cc = ch.value("country"_L1).toString().toUpper();

        // dedupe urls, quality-descending
        std::sort(it.value().urls.begin(), it.value().urls.end(),
                  [](const QJsonObject &a, const QJsonObject &b) {
                      return qualityRank(a.value("quality"_L1).toString()) >
                             qualityRank(b.value("quality"_L1).toString());
                  });
        QJsonArray urlArr;
        QSet<QString> seen;
        for (const QJsonObject &s : std::as_const(it.value().urls)) {
            const QString u = s.value("url"_L1).toString();
            if (u.isEmpty() || seen.contains(u))
                continue;
            seen.insert(u);
            QJsonObject o;
            o.insert("u"_L1, u);
            o.insert("q"_L1, s.value("quality"_L1).toString());
            o.insert("ua"_L1, s.value("user_agent"_L1).toString());
            o.insert("ref"_L1, s.value("referrer"_L1).toString());
            urlArr.append(o);
        }
        if (urlArr.isEmpty())
            continue;

        QJsonArray catArr;
        const auto cats = ch.value("categories"_L1).toArray();
        for (int i = 0; i < cats.size() && i < 2; ++i)
            catArr.append(cats.at(i));

        QJsonObject o;
        o.insert("id"_L1, ch.value("id"_L1).toString());
        o.insert("name"_L1, name);
        o.insert("cc"_L1, cc);
        o.insert("cats"_L1, catArr);
        o.insert("logo"_L1, ch.value("logo"_L1).toString());
        o.insert("urls"_L1, urlArr);
        outChans.append(o);
        counts[cc]++;
    }

    // sort channels by name (casefold)
    QVariantList varList;
    varList.reserve(outChans.size());
    for (int i = 0; i < outChans.size(); ++i)
        varList.append(outChans.at(i).toObject().toVariantMap());
    std::sort(varList.begin(), varList.end(),
              [](const QVariant &a, const QVariant &b) {
                  return a.toMap().value("name"_L1).toString().compare(
                             b.toMap().value("name"_L1).toString(),
                             Qt::CaseInsensitive) < 0;
              });
    outChans = QJsonArray();
    for (const QVariant &v : varList)
        outChans.append(QJsonObject::fromVariantMap(v.toMap()));

    // countries
    struct Ctry {
        QString cc, name, flag;
        int n;
    };
    QVector<Ctry> ctry;
    ctry.reserve(counts.size());
    auto flagOf = [](const QString &cc) -> QString {
        QString code = cc == "UK"_L1 ? QStringLiteral("GB") : cc;
        if (code.size() != 2)
            return QString(QChar(0x1F310));
        QString out;
        for (const QChar &c : code) {
            if (!c.isLetter() || !c.isUpper())
                return QString(QChar(0x1F310));
            out.append(QChar(0x1F1E6 + c.toLatin1() - 'A'));
        }
        return out;
    };
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        const QString cc = it.key();
        QString cname = countryMeta.value(cc).toString();
        if (cname.isEmpty())
            cname = cc.isEmpty() ? QStringLiteral("International") : cc;
        ctry.append({cc, cname, flagOf(cc), it.value()});
    }
    std::sort(ctry.begin(), ctry.end(), [](const Ctry &a, const Ctry &b) {
        if (a.n != b.n)
            return a.n > b.n;
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    QJsonArray outCtry;
    for (const auto &c : ctry) {
        QJsonObject o;
        o.insert("cc"_L1, c.cc);
        o.insert("name"_L1, c.name);
        o.insert("flag"_L1, c.flag);
        o.insert("n"_L1, c.n);
        outCtry.append(o);
    }

    // categories (used count)
    QHash<QString, int> catCounts;
    for (int i = 0; i < outChans.size(); ++i) {
        const auto cats =
            outChans.at(i).toObject().value("cats"_L1).toArray();
        for (int j = 0; j < cats.size(); ++j)
            catCounts[cats.at(j).toString()]++;
    }
    QVector<QPair<QString, int>> catPairs;
    for (auto it = catCounts.constBegin(); it != catCounts.constEnd(); ++it)
        catPairs.append({it.key(), it.value()});
    std::sort(catPairs.begin(), catPairs.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });
    QJsonObject outCats;
    for (const auto &p : catPairs)
        outCats.insert(p.first, p.second);

    QJsonObject root;
    root.insert("built"_L1, QDateTime::currentSecsSinceEpoch());
    root.insert("countries"_L1, outCtry);
    root.insert("cats"_L1, outCats);
    root.insert("channels"_L1, outChans);
    return QJsonDocument(root);
}
} // namespace

ChannelStore &ChannelStore::instance()
{
    static ChannelStore s;
    return s;
}

ChannelStore::ChannelStore(QObject *parent) : QObject(parent)
{
    // An unfavorite while the "Favorites" filter is active must drop the
    // row from the visible slice — re-filter on favorites changes.
    connect(&Favorites::instance(), &Favorites::changed, this, [this] {
        if (m_fFav)
            rebuildFiltered();
    });
}

bool ChannelStore::loadFromFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    const QJsonObject root = doc.object();
    const auto chans = root.value("channels"_L1).toArray();
    const auto countries = root.value("countries"_L1).toArray();

    m_urls.clear();
    m_chans.clear();
    m_idxById.clear();
    m_countries.clear();
    m_countryMeta.clear();
    m_built = root.value("built"_L1).toInteger(0);

    m_urls.reserve(20000);
    m_chans.reserve(chans.size());
    for (const auto &cv : chans) {
        const QJsonObject c = cv.toObject();
        Chan ch;
        ch.id = c.value("id"_L1).toString();
        ch.name = c.value("name"_L1).toString();
        ch.cc = c.value("cc"_L1).toString();
        ch.logo = c.value("logo"_L1).toString();
        for (const auto &x : c.value("cats"_L1).toArray())
            ch.cats.append(x.toString());
        ch.first = int(m_urls.size());
        for (const auto &uv : c.value("urls"_L1).toArray()) {
            const QJsonObject u = uv.toObject();
            m_urls.push_back({u.value("u"_L1).toString(),
                              u.value("ua"_L1).toString(),
                              u.value("ref"_L1).toString(),
                              u.value("q"_L1).toString()});
            ++ch.nUrls;
        }
        if (ch.nUrls == 0 || ch.id.isEmpty())
            continue;
        m_idxById.insert(ch.id, int(m_chans.size()));
        m_chans.push_back(std::move(ch));
    }
    for (const auto &cv : countries) {
        const QJsonObject c = cv.toObject();
        const QString cc = c.value("cc"_L1).toString();
        m_countries.push_back({cc, c.value("name"_L1).toString(),
                               c.value("flag"_L1).toString(),
                               c.value("n"_L1).toInt()});
        m_countryMeta.insert(
            cc, {c.value("name"_L1).toString(), c.value("flag"_L1).toString()});
    }
    return true;
}

void ChannelStore::load()
{
    // Priority: AppDataLocation/catalog.json (a previous refresh) >
    // embedded qrc baseline > APP_SOURCE_DIR fallback (test builds).
    if (m_chans.isEmpty()) {
        const QString overridePath = QStandardPaths::writableLocation(
                                         QStandardPaths::AppDataLocation) +
                                     QStringLiteral("/catalog.json");
        if (!loadFromFile(overridePath)) {
            if (!loadFromFile(kCatalogRes.toString())) {
                // last resort: source tree (test builds without resources)
                loadFromFile(QStringLiteral(APP_SOURCE_DIR) +
                             QStringLiteral("/catalog.json"));
            }
        }
    }

    rebuildCountryAndCatLists();
    clearFilter();
    rebuildFiltered();
    m_loaded = true;
    emit loadedChanged();
}

void ChannelStore::rebuildCountryAndCatLists()
{
    m_countryList.clear();
    for (const auto &c : m_countries) {
        QVariantMap m;
        m.insert("cc"_L1, c.cc);
        m.insert("name"_L1, c.name);
        m.insert("flag"_L1, c.flag);
        m.insert("n"_L1, c.n);
        m_countryList.append(m);
    }
    QHash<QString, int> catCounts;
    for (const auto &ch : m_chans)
        for (const QString &c : ch.cats)
            catCounts[c]++;
    m_catList.clear();
    for (auto it = catCounts.constBegin(); it != catCounts.constEnd(); ++it) {
        QVariantMap m;
        m.insert("id"_L1, it.key());
        m.insert("n"_L1, it.value());
        m_catList.append(m);
    }
    std::sort(m_catList.begin(), m_catList.end(),
              [](const QVariant &a, const QVariant &b) {
                  return a.toMap().value("id"_L1).toString() <
                         b.toMap().value("id"_L1).toString();
              });
}

int ChannelStore::qualityRank(const QString &q)
{
    return ::qualityRank(q);
}

void ChannelStore::rebuildFiltered()
{
    // Search tokens, case-insensitive substring on the preallocated slice.
    const QString needle = m_search.trimmed();
    const auto tokens = needle.split(QLatin1Char(' '), Qt::SkipEmptyParts);

    m_filtered.clear();
    m_filtered.reserve(m_chans.size());
    for (const auto &ch : m_chans) {
        if (!m_fCountry.isEmpty() && ch.cc != m_fCountry)
            continue;
        if (!m_fCategory.isEmpty() && !ch.cats.contains(m_fCategory))
            continue;
        if (m_fFav && !Favorites::instance().contains(ch.id))
            continue;
        if (!tokens.isEmpty()) {
            bool all = true;
            for (const QString &t : tokens) {
                if (!ch.name.contains(t, Qt::CaseInsensitive)) {
                    all = false;
                    break;
                }
            }
            if (!all)
                continue;
        }
        m_filtered.append(chanRow(ch));
    }
    emit channelsChanged();
}

QVariantMap ChannelStore::chanRow(const Chan &c) const
{
    QVariantMap m;
    m.insert("id"_L1, c.id);
    m.insert("name"_L1, c.name);
    m.insert("cc"_L1, c.cc);
    m.insert("logo"_L1, c.logo);
    m.insert("cats"_L1, c.cats);
    m.insert("nUrls"_L1, c.nUrls);
    const auto meta = m_countryMeta.constFind(c.cc);
    if (meta != m_countryMeta.constEnd()) {
        m.insert("countryName"_L1, meta->first);
        m.insert("flag"_L1, meta->second);
    } else {
        m.insert("countryName"_L1, c.cc);
        m.insert("flag"_L1, QString(QChar(0x1F310)));
    }
    // best quality label for the tile badge
    QString bestQ;
    for (int i = c.first; i < c.first + c.nUrls; ++i) {
        const QString &q = m_urls.at(i).q;
        if (qualityRank(q) > qualityRank(bestQ))
            bestQ = q;
    }
    m.insert("quality"_L1, bestQ);
    m.insert("fav"_L1, Favorites::instance().contains(c.id));
    return m;
}

void ChannelStore::clearFilter()
{
    m_fCountry.clear();
    m_fCategory.clear();
    m_fFav = false;
    m_search.clear();
}

void ChannelStore::setFilterCountry(const QString &cc)
{
    if (m_fCountry == cc)
        return;
    m_fCountry = cc;
    emit filterChanged();
    rebuildFiltered();
}

void ChannelStore::setFilterCategory(const QString &cat)
{
    if (m_fCategory == cat)
        return;
    m_fCategory = cat;
    emit filterChanged();
    rebuildFiltered();
}

void ChannelStore::setFilterFav(bool fav)
{
    if (m_fFav == fav)
        return;
    m_fFav = fav;
    emit filterChanged();
    rebuildFiltered();
}

void ChannelStore::setSearch(const QString &s)
{
    if (m_search == s)
        return;
    m_search = s;
    emit filterChanged();
    rebuildFiltered();
}

QVariantList ChannelStore::streamUrls(const QString &id) const
{
    QVariantList out;
    const auto it = m_idxById.constFind(id);
    if (it == m_idxById.constEnd())
        return out;
    const Chan &ch = m_chans.at(it.value());
    for (int i = ch.first; i < ch.first + ch.nUrls; ++i) {
        const Url &u = m_urls.at(i);
        QVariantMap m;
        m.insert("u"_L1, u.u);
        m.insert("ua"_L1, u.ua);
        m.insert("ref"_L1, u.ref);
        m.insert("q"_L1, u.q);
        out.append(m);
    }
    return out;
}

QVariantMap ChannelStore::channelById(const QString &id) const
{
    const auto it = m_idxById.constFind(id);
    if (it == m_idxById.constEnd())
        return {};
    return chanRow(m_chans.at(it.value()));
}

void ChannelStore::refresh()
{
    if (m_refreshing)
        return;
    if (!m_nam)
        m_nam = new QNetworkAccessManager(this);
    m_refreshing = true;
    emit refreshingChanged();

    // --- straightforward sequential fetch (channels then streams) ---
    QNetworkRequest req1{QUrl(kApiChannels)};
    req1.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                      QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *r1 = m_nam->get(req1);
    connect(r1, &QNetworkReply::finished, this, [this, r1] {
        if (r1->error() != QNetworkReply::NoError) {
            emit refreshDone(false, r1->errorString());
            r1->deleteLater();
            m_refreshing = false;
            emit refreshingChanged();
            return;
        }
        const QJsonDocument channelsDoc =
            QJsonDocument::fromJson(r1->readAll());
        r1->deleteLater();

        QNetworkRequest req2{QUrl(kApiStreams)};
        req2.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                          QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *r2 = m_nam->get(req2);
        connect(r2, &QNetworkReply::finished, this, [this, r2, channelsDoc] {
            if (r2->error() != QNetworkReply::NoError) {
                emit refreshDone(false, r2->errorString());
                r2->deleteLater();
                m_refreshing = false;
                emit refreshingChanged();
                return;
            }
            const QJsonDocument streamsDoc =
                QJsonDocument::fromJson(r2->readAll());
            r2->deleteLater();

            // country names come from the current catalog (already parsed);
            QJsonObject countryMeta;
            for (const auto &v : m_countries)
                countryMeta.insert(v.cc, v.name);

            const QJsonDocument merged =
                buildCatalog(channelsDoc, streamsDoc, countryMeta);
            const QString dir = QStandardPaths::writableLocation(
                QStandardPaths::AppDataLocation);
            QDir().mkpath(dir);
            const QString path = dir + QStringLiteral("/catalog.json");
            QFile out(path);
            if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                emit refreshDone(false, QStringLiteral("cannot write ") +
                                            path);
                m_refreshing = false;
                emit refreshingChanged();
                return;
            }
            out.write(merged.toJson(QJsonDocument::Compact));
            out.close();

            const bool ok = loadFromFile(path);
            rebuildCountryAndCatLists();
            rebuildFiltered();
            m_loaded = true;
            emit loadedChanged();
            emit refreshDone(ok, ok ? QStringLiteral("ok") : path);
            m_refreshing = false;
            emit refreshingChanged();
        });
    });
}
