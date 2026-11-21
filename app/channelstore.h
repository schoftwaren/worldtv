#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

class QNetworkAccessManager;
class QTimer;
class ChannelModel;

// One channel catalog for the whole app: parses the (embedded or refreshed)
// iptv-org-derived catalog.json, exposes countries/categories and the
// filtered channel slice to QML.
//
// Data flow: tools/fetch_data.sh -> data/*.json -> tools/prepdata.py ->
// app/catalog.json -> qt_add_resources (qrc:/worldtv/catalog.json).
// The C++ refresh path (refresh()) re-fetches the raw api dumps, runs the
// same merge in-process and persists the result under AppDataLocation;
// on the next start that file wins over the embedded baseline.
//
// Perf: rebuildFiltered() produces an INDEX SLICE (not QVariantMap rows);
// ChannelModel exposes that slice lazily to QML (data() only for tiles the
// GridView actually instantiates) so selecting a 1600-channel country
// builds 60 tiles, not 1600 maps. Search re-filters on an idle timer.
class ChannelStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(int channelCount READ channelCount NOTIFY loadedChanged)
    Q_PROPERTY(int countryCount READ countryCount NOTIFY loadedChanged)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
    Q_PROPERTY(ChannelModel *channels READ channels CONSTANT)
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

    ChannelModel *channels() const { return m_model; }
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

    // Apply a pending debounced search NOW (tests, --smoke, and callers
    // that read the slice synchronously after setSearch).
    Q_INVOKABLE void flushSearch();

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
    void refreshFavMask(); // recompute m_favMask from Favorites
    QVariantMap chanRow(const Chan &c) const;
    QVariantMap channelAt(int sliceRow) const;
    static int qualityRank(const QString &q);
    void clearFilter();

    QVector<Url> m_urls;
    QVector<Chan> m_chans;
    QHash<QString, int> m_idxById;
    QVector<Ctry> m_countries;
    QHash<QString, QPair<QString, QString>> m_countryMeta; // cc -> (name, flag)

    QVector<int> m_filtered;  // slice of indexes into m_chans (lazy model)
    QVector<bool> m_favMask;  // parallel to m_filtered, Favorites check
    QVariantList m_countryList;
    QVariantList m_catList;

    QString m_fCountry, m_fCategory, m_search;
    bool m_fFav = false;
    bool m_loaded = false;
    bool m_refreshing = false;

    QNetworkAccessManager *m_nam = nullptr;
    qint64 m_built = 0; // epoch seconds stamped by the builder
    QTimer *m_searchTimer = nullptr; // debounce search re-filter (120ms)
    ChannelModel *m_model = nullptr; // exposed to QML as Store.channels
    friend class ChannelModel;
};

// Lazy QML model over the store's filtered index slice. Roles mirror the
// old QVariantMap rows (id/name/cc/logo/cats/nUrls/quality/fav/countryName/
// flag) so tiles and the player overlay read identically. Row maps are
// built on demand in data() — ONLY for tiles the GridView instantiates.
class ChannelModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        CcRole,
        LogoRole,
        CatsRole,
        NUrlsRole,
        QualityRole,
        FavRole,
        CountryNameRole,
        FlagRole,
    };

    explicit ChannelModel(ChannelStore *store, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
                  int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_store->m_filtered.size()); }
    Q_INVOKABLE QVariantMap get(int row) const; // nav/test convenience
    Q_INVOKABLE void refreshFavorites();

signals:
    void countChanged();

private:
    void replaceSlice(QVector<int> slice); // store calls on re-filter
    ChannelStore *m_store;
    friend class ChannelStore;
};
