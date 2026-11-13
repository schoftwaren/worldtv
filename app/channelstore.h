#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

class QNetworkAccessManager;

// One channel catalog for the whole app: parses the (embedded or refreshed)
// iptv-org-derived catalog.json, exposes countries/categories and the
// filtered channel slice to QML.
//
// Data flow: tools/fetch_data.sh -> data/*.json -> tools/prepdata.py ->
// app/catalog.json -> qt_add_resources (qrc:/worldtv/catalog.json).
// The C++ refresh path (refresh()) re-fetches the raw api dumps, runs the
// same merge in-process and persists the result under AppDataLocation;
// on the next start that file wins over the embedded baseline.
class ChannelStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(int channelCount READ channelCount NOTIFY loadedChanged)
    Q_PROPERTY(int countryCount READ countryCount NOTIFY loadedChanged)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
    Q_PROPERTY(QVariantList channels READ channels NOTIFY channelsChanged)
    Q_PROPERTY(QVariantList countries READ countries CONSTANT)
    Q_PROPERTY(QVariantList categories READ categories CONSTANT)
    Q_PROPERTY(QString filterCountry READ filterCountry WRITE setFilterCountry
                   NOTIFY filterChanged)
    Q_PROPERTY(QString filterCategory READ filterCategory WRITE
                   setFilterCategory NOTIFY filterChanged)
    Q_PROPERTY(bool filterFav READ filterFav WRITE setFilterFav NOTIFY
                   filterChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY filterChanged)

public:
    static ChannelStore &instance();

    // Parse the catalog (embedded qrc first, AppDataLocation override if
    // newer, source-dir fallback for test builds). Synchronous; ~100ms.
    void load();

    bool loaded() const { return m_loaded; }
    int channelCount() const { return int(m_chans.size()); }
    int countryCount() const { return m_countries.size(); }
    bool refreshing() const { return m_refreshing; }

    const QVariantList &channels() const { return m_filtered; }
    const QVariantList &countries() const { return m_countryList; }
    const QVariantList &categories() const { return m_catList; }

    QString filterCountry() const { return m_fCountry; }
    QString filterCategory() const { return m_fCategory; }
    bool filterFav() const { return m_fFav; }
    QString search() const { return m_search; }

    void setFilterCountry(const QString &cc);
    void setFilterCategory(const QString &cat);
    void setFilterFav(bool fav);
    void setSearch(const QString &s);

    // Ordered stream candidate list for one channel:
    // [{u, q, ua, ref}, ...] best-quality first.
    Q_INVOKABLE QVariantList streamUrls(const QString &id) const;

    // Light row for the player header / recents checks.
    Q_INVOKABLE QVariantMap channelById(const QString &id) const;

    // Re-fetch the iptv-org api dumps and rebuild the catalog in place.
    // Persists under AppDataLocation/catalog.json on success.
    Q_INVOKABLE void refresh();

    // Test/CI hook: parse any catalog file directly (bypasses qrc lookup).
    bool loadFromFile(const QString &path);

    // Test hook: quality rank used by the stream-url sort.
    static int qualityRankForTest(const QString &q)
    {
        return qualityRank(q);
    }

signals:
    void loadedChanged();
    void refreshingChanged();
    void channelsChanged();
    void filterChanged();
    void refreshDone(bool ok, const QString &message);

private:
    explicit ChannelStore(QObject *parent = nullptr);

    struct Url {
        QString u, ua, ref, q;
    };
    struct Chan {
        QString id, name, cc, logo;
        QStringList cats;
        int first = 0, nUrls = 0;
    };
    struct Ctry {
        QString cc, name, flag;
        int n = 0;
    };

    void rebuildCountryAndCatLists();
    void rebuildFiltered();
    QVariantMap chanRow(const Chan &c) const;
    static int qualityRank(const QString &q);
    void clearFilter();

    QVector<Url> m_urls;
    QVector<Chan> m_chans;
    QHash<QString, int> m_idxById;
    QVector<Ctry> m_countries;
    QHash<QString, QPair<QString, QString>> m_countryMeta; // cc -> (name, flag)

    QVariantList m_filtered;
    QVariantList m_countryList;
    QVariantList m_catList;

    QString m_fCountry, m_fCategory, m_search;
    bool m_fFav = false;
    bool m_loaded = false;
    bool m_refreshing = false;

    QNetworkAccessManager *m_nam = nullptr;
    qint64 m_built = 0; // epoch seconds stamped by the builder
};
