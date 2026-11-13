#pragma once

#include <QObject>

struct _SDL_GameController;
typedef struct _SDL_GameController SDL_GameController;

// SDL2 gamepad -> Qt key events, so the whole UI is driveable from the couch
// with no mouse/keyboard. Steam Deck's embedded controller exposes itself as
// a standard SDL GameController (xone/xbox360 driver in SteamOS; plain SDL2
// sees it as "Steam Deck Controller" via the Steam input layer or directly).
//
// Mapping (10-foot TV conventions):
//   D-pad / left stick  -> arrows
//   A (bottom)          -> Return (activate)
//   B (right)           -> Escape (back)
//   X (left)            -> F (favorite toggle on focused tile)
//   Y (top)             -> / (search focus)
//   Left stick press    -> R (reload channel)
//   Start (+)           -> Space (play/pause)
//   Select (-)          -> M (mute)
//   Right stick         -> nothing (mpv takes arrow-scale keys already)
//   Shoulder L/R        -> PageUp/PageDown (rail page)
//
// Keyboard stays fully functional; SDL events are additive. When no
// controller exists the app still works (Deck users: Steam Input may also
// inject synthetic key events — harmless duplicates).
class Gamepad : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString name READ name NOTIFY connectedChanged)

public:
    static Gamepad &instance();
    ~Gamepad() override;

    bool start(); // init SDL; safe to call twice
    bool connected() const { return m_controller != nullptr; }
    QString name() const { return m_name; }

    // Runs the SDL event pump; called from a timer in main.cpp. Emits
    // keyEvent(Qt::Key, pressed) per translated button/stick edge.
    void pump();

signals:
    void connectedChanged();
    void keyEvent(int key, bool pressed);

private:
    explicit Gamepad(QObject *parent = nullptr);
    void openController(int deviceIndex);
    void closeController();
    void emitKey(Qt::Key key, bool pressed);

    // left-stick digital thresholds with hysteresis
    bool m_leftX = false; // stick currently acting as "left"
    bool m_rightX = false;
    bool m_upY = false;
    bool m_downY = false;

    SDL_GameController *m_controller = nullptr;
    QString m_name;
    bool m_sdlReady = false;
};
