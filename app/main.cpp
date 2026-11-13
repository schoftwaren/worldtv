#include "channelstore.h"
#include "favorites.h"
#include "gamepad.h"
#include "recents.h"
#include "wtvmpvitem.h"

#include <QCommandLineParser>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickWindow>
#include <QTimer>
#include <cstdio>

namespace {

// Headless core smoke: load the embedded catalog, print verdicts, exit.
// No QML, no window — the fast CI gate for the data layer.
int runSmoke()
{
    ChannelStore &store = ChannelStore::instance();
    store.load();
    const int n = store.channelCount();
    std::printf("SMOKE channels=%d countries=%d cats=%d\n", n,
                store.countryCount(), store.categories().size());
    if (n < 1000) {
        std::printf("SMOKE RESULT: FAIL (catalog too small)\n");
        return 1;
    }
    // filters must produce sane slices
    store.setFilterCountry(QStringLiteral("DE"));
    std::printf("SMOKE de=%d\n", store.channels().size());
    if (store.channels().isEmpty()) {
        std::printf("SMOKE RESULT: FAIL (DE filter empty)\n");
        return 1;
    }
    const QVariantList urls =
        store.streamUrls(store.channels().first().toMap()
                             .value(QStringLiteral("id"))
                             .toString());
    std::printf("SMOKE first-de-urls=%d\n", urls.size());
    if (urls.isEmpty()) {
        std::printf("SMOKE RESULT: FAIL (no stream urls)\n");
        return 1;
    }
    store.setFilterCountry(QString());
    store.setSearch(QStringLiteral("bbc"));
    std::printf("SMOKE search-bbc=%d\n", store.channels().size());
    store.setSearch(QString());
    std::printf("SMOKE RESULT: OK\n");
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    // ONE message handler, installed first (viddeck §17: a second handler
    // replaces the first and silently ate every Qt warning in packaged
    // builds; filtered default logging on this box hides QML load errors).
    qInstallMessageHandler(
        [](QtMsgType t, const QMessageLogContext &, const QString &m) {
            const char *kind = t == QtFatalMsg     ? "FATAL"
                               : t == QtCriticalMsg ? "CRIT"
                               : t == QtWarningMsg  ? "WARN"
                                                    : "INFO";
            std::fprintf(stderr, "[qt:%s] %s\n", kind, qUtf8Printable(m));
        });

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("worldtv"));
    QGuiApplication::setOrganizationName(QStringLiteral("worldtv"));

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption smokeOpt(QStringLiteral("smoke"),
                                QStringLiteral("headless data-layer smoke test"));
    parser.addOption(smokeOpt);

    QCommandLineOption shotOpt(
        QStringLiteral("screenshot"),
        QStringLiteral("render UI states to <prefix>_*.png and exit"),
        QStringLiteral("prefix"));
    parser.addOption(shotOpt);
    parser.process(app);

    if (parser.isSet(smokeOpt))
        return runSmoke();

    // mpv renders via OpenGL into the QQuickFramebufferObject — must be set
    // before the first window exists (viddeck/mpvqt requirement).
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    qmlRegisterType<WtvMpvItem>("Wtv", 1, 0, "MpvItem");
    qmlRegisterSingletonInstance("Wtv", 1, 0, "Store", &ChannelStore::instance());
    qmlRegisterSingletonInstance("Wtv", 1, 0, "Favs", &Favorites::instance());
    qmlRegisterSingletonInstance("Wtv", 1, 0, "Recent", &Recents::instance());
    qmlRegisterSingletonInstance("Wtv", 1, 0, "Pad", &Gamepad::instance());

    // Parse the embedded/refreshed catalog before the engine loads (the QML
    // side builds its nav model as soon as the store reports loaded).
    ChannelStore::instance().load();

    // Gamepad pump: SDL events -> Qt key events, 60 Hz.
#ifndef WORLDTV_NO_SDL
    Gamepad::instance().start();
    QTimer pumpTimer;
    QObject::connect(&pumpTimer, &QTimer::timeout, &Gamepad::instance(),
                     &Gamepad::pump);
    pumpTimer.start(16);
#endif

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral(APP_SOURCE_DIR));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
#ifdef WORLDTV_QML_EMBEDDED
    // Embedded releases: Main.qml rides in the executable's own qrc.
    // Two prefix candidates (qt_add_qml_module's default prefix moved
    // across Qt versions - viddeck AppImage chain). load() is void;
    // pick the candidate that actually exists in the resource system.
    const QUrl mainUrl =
        QFile::exists(QStringLiteral(":/worldtv/Main.qml"))
            ? QUrl(QStringLiteral("qrc:/worldtv/Main.qml"))
            : QUrl(QStringLiteral("qrc:/qt/qml/worldtv/Main.qml"));
    engine.load(mainUrl);
#else
    engine.load(QUrl::fromLocalFile(QStringLiteral(APP_SOURCE_DIR) +
                                    QStringLiteral("/Main.qml")));
#endif
    std::fprintf(stderr, "[m2] load done, rootObjects=%d\n",
                 int(engine.rootObjects().size()));
    if (engine.rootObjects().isEmpty())
        return 1;
    // Falsifiable smoke marker (grep target for CI / local verification):
    // printed ONLY after Main.qml actually mounted.
    std::fprintf(stderr, "WTV_UI_OK channels=%d\n",
                 ChannelStore::instance().channelCount());

    auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    const QString winSize = qEnvironmentVariable("WORLDTV_WINDOW_SIZE");
    // Override through the QML property system (QQmlProperty::write breaks
    // the QML width/height bindings and sticks — QWindow::resize does not:
    // the QML-side sizing re-asserts itself after show).
    win->show();
    if (!winSize.isEmpty()) {
        const QStringList wh = winSize.split(QLatin1Char('x'));
        if (wh.size() == 2 && wh[0].toInt() > 0 && wh[1].toInt() > 0) {
            QQmlProperty::write(win, QStringLiteral("width"), wh[0].toInt());
            QQmlProperty::write(win, QStringLiteral("height"), wh[1].toInt());
        }
    }
    std::fprintf(stderr, "[shot] afterWrite win=%dx%d\n", win->width(), win->height());

    const QString autoPlay = qEnvironmentVariable("WORLDTV_AUTOPLAY");
    if (parser.isSet(shotOpt)) {
        const QString prefix = parser.value(shotOpt);
        QEventLoop pump;
        auto wait = [&](int ms) {
            QTimer::singleShot(ms, &pump, &QEventLoop::quit);
            pump.exec();
        };
        auto grab = [&](const QString &suffix) {
            // xcb re-sizes the window shortly after show (a phantom
            // 941x1030 without any WM; see the no-SDL probe in git log).
            // Re-assert the requested size immediately before grabbing so
            // kit shots always come out at the requested resolution.
            if (!winSize.isEmpty()) {
                const QStringList wh = winSize.split(QLatin1Char('x'));
                if (wh.size() == 2 && wh[0].toInt() > 0 && wh[1].toInt() > 0) {
                    QQmlProperty::write(win, QStringLiteral("width"),
                                        wh[0].toInt());
                    QQmlProperty::write(win, QStringLiteral("height"),
                                        wh[1].toInt());
                    // Let xcb apply the reconfigure + the FBO reallocate
                    // (first grab raced it and came out at the old size).
                    wait(150);
                }
            }
            const QImage img = win->grabWindow();
            const QString file = prefix + suffix;
            std::printf("%s %s\n", img.save(file) ? "SHOT" : "SHOT_FAIL",
                        qPrintable(file));
        };

        wait(1500); // catalog parse + first layout
        grab(QStringLiteral("_browse.png"));

        if (!autoPlay.isEmpty()) {
            QMetaObject::invokeMethod(win, "demoPlay",
                                      Q_ARG(QVariant, autoPlay));
            const int delay = qEnvironmentVariableIntValue(
                "WORLDTV_SHOT_DELAY", nullptr);
            wait(delay > 0 ? delay : 8000); // stream connect budget
            std::printf("DEMO_STATE %s\n",
                        qPrintable(win->property("debugState").toString()));
            grab(QStringLiteral("_play.png"));
            QMetaObject::invokeMethod(win, "demoBack");
            wait(600);
            grab(QStringLiteral("_back.png"));
        }
        return 0;
    }

    if (!autoPlay.isEmpty()) {
        QTimer::singleShot(1500, win, [win, autoPlay] {
            QMetaObject::invokeMethod(win, "demoPlay",
                                      Q_ARG(QVariant, autoPlay));
        });
    }

    return app.exec();
}
