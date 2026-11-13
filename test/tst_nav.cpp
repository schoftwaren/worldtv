// Nav regression test: selecting a country in the sidebar — with keyboard
// Enter AND with a plain mouse click — must apply the country filter to
// the grid slice.
//
// Regression: the nav delegate's onClicked called applyNav() unqualified.
// A delegate's scope chain skips the enclosing view, so the call threw
// "ReferenceError: applyNav is not defined" (silently — stderr never
// reaches a start-menu launch). Qt 6 ItemDelegate activates the view's
// current item on Return, so the keyboard path died in the same spot,
// and a mouse click only moved the highlight. Fix: navList.applyNav().
// Falsifiability: this suite FAILS against the pre-fix Main.qml.
#include "channelstore.h"
#include "favorites.h"
#include "gamepad.h"
#include "recents.h"
#include "wtvmpvitem.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>
#include <QVariantList>
#include <QtTest/qtest.h>
#include <functional>

using namespace Qt::Literals::StringLiterals;

namespace {
// The sidebar ListView: the only QQuickListView at 320 logical width.
QQuickItem *findSidebarList(QQuickItem *parent)
{
    if (!parent)
        return nullptr;
    if (QLatin1String(parent->metaObject()->className())
            .startsWith(QLatin1String("QQuickListView"))
        && qFuzzyCompare(parent->width(), 320.0))
        return parent;
    const QList<QQuickItem *> kids = parent->childItems();
    for (QQuickItem *k : kids) {
        if (QQuickItem *r = findSidebarList(k))
            return r;
    }
    return nullptr;
}
} // namespace

class NavTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void keyboardEnterAppliesCountryFilter();
    void mouseClickAppliesCountryFilter();
    void cleanupTestCase();

private:
    QQmlApplicationEngine *engine = nullptr;
    QQuickWindow *win = nullptr;
};

void NavTest::initTestCase()
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    qmlRegisterType<WtvMpvItem>("Wtv", 1, 0, "MpvItem");
    qmlRegisterSingletonInstance("Wtv", 1, 0, "Store",
                                 &ChannelStore::instance());
    qmlRegisterSingletonInstance("Wtv", 1, 0, "Favs",
                                 &Favorites::instance());
    qmlRegisterSingletonInstance("Wtv", 1, 0, "Recent",
                                 &Recents::instance());
    qmlRegisterSingletonInstance("Wtv", 1, 0, "Pad", &Gamepad::instance());
    ChannelStore::instance().load();
    auto &store = ChannelStore::instance();
    QVERIFY2(store.loaded(), "catalog failed to load");
    QVERIFY2(store.channelCount() > 1000, "catalog unexpectedly small");

    engine = new QQmlApplicationEngine(this);
    engine->addImportPath(QStringLiteral(APP_SOURCE_DIR));
    engine->load(QUrl::fromLocalFile(QStringLiteral(APP_SOURCE_DIR) +
                                     QStringLiteral("/Main.qml")));
    QVERIFY2(!engine->rootObjects().isEmpty(), "Main.qml failed to mount");
    win = qobject_cast<QQuickWindow *>(engine->rootObjects().first());
    QVERIFY2(win, "root object is not a window");
    win->show();
    QTest::qWait(600); // layout + navModel build + delegate creation
    QVERIFY2(findSidebarList(win->contentItem()), "sidebar list not found");
}

void NavTest::keyboardEnterAppliesCountryFilter()
{
    ChannelStore &store = ChannelStore::instance();
    const int before = store.channels().size();

    // Couch flow, exactly as played: grid focus at launch -> Left into the
    // sidebar -> three Downs (All, Favorites, first country, second) ->
    // Return applies.
    QTest::keyClick(win, Qt::Key_Left);
    QTest::keyClick(win, Qt::Key_Down);
    QTest::keyClick(win, Qt::Key_Down);
    QTest::keyClick(win, Qt::Key_Down);
    QTest::keyClick(win, Qt::Key_Return);

    // Expected country derives from the live countries list (count-desc
    // sorted, nav row 2 = countries[0], row 3 = countries[1]) so the test
    // never couples to a specific dataset.
    const QVariantList cs = store.countries();
    QVERIFY2(cs.size() > 2, "not enough countries for the nav layout");
    const QString expect = cs.at(1).toMap().value(u"cc"_s).toString();
    const QString cc = store.filterCountry();
    QVERIFY2(cc == expect, qUtf8Printable(QStringLiteral(
                               "filterCountry=%1 expected=%2")
                               .arg(cc, expect)));
    QVERIFY2(store.channels().size() < before,
             "country filter did not shrink the grid slice");

    // reset for the next test
    store.setFilterCountry(QString());
    store.setFilterCategory(QString());
    store.setFilterFav(false);
    store.setSearch(QString());
    QCOMPARE(store.channels().size(), before);
}

void NavTest::mouseClickAppliesCountryFilter()
{
    ChannelStore &store = ChannelStore::instance();
    QQuickItem *nav = findSidebarList(win->contentItem());
    QVERIFY(nav);

    // A click IS an activation: row 2 (first country) must filter the grid
    // without any keyboard involvement.
    nav->setProperty("currentIndex", 2);
    const auto cur =
        qvariant_cast<QQuickItem *>(nav->property("currentItem"));
    QVERIFY2(cur, "nav list has no current item");
    const QPointF center =
        cur->mapToScene(QPointF(cur->width() / 2.0, cur->height() / 2.0));
    QTest::mouseClick(win, Qt::LeftButton, Qt::NoModifier, center.toPoint());

    const QVariantList cs = store.countries();
    QVERIFY(!cs.isEmpty());
    const QString expect = cs.at(0).toMap().value(u"cc"_s).toString();
    const QString cc = store.filterCountry();
    QVERIFY2(cc == expect,
             qUtf8Printable(
                 QStringLiteral("filterCountry=%1 expected=%2")
                     .arg(cc, expect)));

    store.setFilterCountry(QString());
    store.setFilterCategory(QString());
    store.setFilterFav(false);
    store.setSearch(QString());
}

void NavTest::cleanupTestCase()
{
    ChannelStore &store = ChannelStore::instance();
    store.setFilterCountry(QString());
    store.setFilterCategory(QString());
    store.setFilterFav(false);
    store.setSearch(QString());
    delete engine;
    engine = nullptr;
    win = nullptr;
}

QTEST_MAIN(NavTest)
#include "tst_nav.moc"
