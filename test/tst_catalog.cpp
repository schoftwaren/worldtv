// Catalog parser test: real embedded catalog.json, real filtering logic.
// Ad-hoc protocol: run via ctest; prints CATALOG_TEST verdict lines.
#include "channelstore.h"
#include "favorites.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QTest>
#include <QVariantMap>

using namespace Qt::Literals::StringLiterals;

class CatalogTest : public QObject {
    Q_OBJECT

private slots:
    void loadEmbeddedCatalog();
    void countryAndCategoryFilters();
    void searchFilter();
    void streamUrlFallbacks();
    void favoritesPersist();

private:
    static ChannelStore &store()
    {
        auto &s = ChannelStore::instance();
        if (!s.loaded())
            s.load();
        return s;
    }
};

void CatalogTest::loadEmbeddedCatalog()
{
    auto &s = store();
    QVERIFY2(s.loaded(), "catalog failed to load");
    // The embedded build ships ~10k channels; the guard is deliberately
    // loose so a smaller honest dataset still passes.
    QVERIFY2(s.channelCount() > 1000,
             qPrintable(QStringLiteral("channelCount=%1")
                            .arg(s.channelCount())));
    QVERIFY2(s.countryCount() > 50,
             qPrintable(QStringLiteral("countryCount=%1")
                            .arg(s.countryCount())));
    // every channel carries at least one stream url
    const QVariantList all = s.channels();
    for (const QVariant &v : all) {
        const QVariantMap row = v.toMap();
        if (row.value("nUrls"_L1).toInt() < 1) {
            QFAIL(qPrintable(QStringLiteral("channel without urls: %1")
                                 .arg(row.value("name"_L1).toString())));
            return;
        }
    }
}

void CatalogTest::countryAndCategoryFilters()
{
    auto &s = store();
    s.setFilterCountry(QStringLiteral("DE"));
    const QVariantList de = s.channels();
    QVERIFY(!de.isEmpty());
    for (const QVariant &v : de)
        QCOMPARE(v.toMap().value("cc"_L1).toString(), QStringLiteral("DE"));
    // countries list is sorted by count desc
    const QVariantList cs = s.countries();
    QVERIFY(cs.size() > 2);
    QVERIFY(cs.first().toMap().value("n"_L1).toInt() >=
            cs.last().toMap().value("n"_L1).toInt());

    // category filter: pick the first known category and verify membership
    const QVariantList cats = s.categories();
    QVERIFY(!cats.isEmpty());
    const QString cat = cats.first().toMap().value("id"_L1).toString();
    s.setFilterCountry(QString());
    s.setFilterCategory(cat);
    const QVariantList slice = s.channels();
    QVERIFY(!slice.isEmpty());
    for (const QVariant &v : slice)
        QVERIFY(v.toMap().value("cats"_L1).toStringList().contains(cat));
    s.setFilterCategory(QString());
}

void CatalogTest::searchFilter()
{
    auto &s = store();
    s.setSearch(QStringLiteral("bbc"));
    const QVariantList hits = s.channels();
    QVERIFY(!hits.isEmpty());
    for (const QVariant &v : hits)
        QVERIFY2(v.toMap().value("name"_L1)
                     .toString()
                     .contains("bbc", Qt::CaseInsensitive),
                 qPrintable(v.toMap().value("name"_L1).toString()));
    s.setSearch(QStringLiteral("zzzzqqqq_no_match"));
    QCOMPARE(s.channels().size(), 0);
    s.setSearch(QString());
}

void CatalogTest::streamUrlFallbacks()
{
    auto &s = store();
    // find a channel with >1 url and verify order + shape
    QString multi;
    const QVariantList all = s.channels();
    for (const QVariant &v : all) {
        if (v.toMap().value("nUrls"_L1).toInt() > 1) {
            multi = v.toMap().value("id"_L1).toString();
            break;
        }
    }
    QVERIFY(!multi.isEmpty());
    const QVariantList urls = s.streamUrls(multi);
    QVERIFY(urls.size() > 1);
    for (const QVariant &v : urls) {
        const QVariantMap u = v.toMap();
        QVERIFY(u.value("u"_L1).toString().startsWith("http"_L1));
    }
    // quality is descending (unknown-quality entries rank 0)
    int prev = 99;
    for (const QVariant &v : urls) {
        const int r = ChannelStore::qualityRankForTest(
            v.toMap().value("q"_L1).toString());
        QVERIFY2(r <= prev, "urls not quality-sorted");
        prev = r;
    }
    QCOMPARE(s.streamUrls(QStringLiteral("does-not-exist")).size(), 0);
}

void CatalogTest::favoritesPersist()
{
    Favorites &f = Favorites::instance();
    const QString id = QStringLiteral("tst-fav-channel");
    const bool had = f.contains(id);
    f.toggle(id);
    if (had)
        QVERIFY(!f.contains(id));
    else {
        QVERIFY(f.contains(id));
        f.toggle(id); // restore
        QVERIFY(!f.contains(id));
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    // catalog lookup priority includes APP_SOURCE_DIR — defined by CMake.
    CatalogTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_catalog.moc"
