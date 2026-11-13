#include "gamepad.h"

#ifdef WORLDTV_NO_SDL
// Android: no SDL2 in the NDK; touch drives the UI. The stub keeps
// gamepad.h's ABI so main.cpp compiles unchanged.
Gamepad &Gamepad::instance()
{
    static Gamepad s;
    return s;
}

Gamepad::Gamepad(QObject *parent) : QObject(parent) {}
Gamepad::~Gamepad() = default;

bool Gamepad::start()
{
    m_sdlReady = false;
    return false;
}

void Gamepad::pump() {}
#endif // WORLDTV_NO_SDL

#ifndef WORLDTV_NO_SDL
#include <SDL.h>

#include <QGuiApplication>
#include <QKeyEvent>
#include <QWindow>

Gamepad &Gamepad::instance()
{
    static Gamepad s;
    return s;
}

Gamepad::Gamepad(QObject *parent) : QObject(parent) {}

Gamepad::~Gamepad()
{
    if (m_controller)
        SDL_GameControllerClose(m_controller);
    if (m_sdlReady)
        SDL_Quit();
}

bool Gamepad::start()
{
    if (m_sdlReady)
        return true;
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        // Not fatal: keyboard still drives everything.
        return false;
    }
    m_sdlReady = true;

    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            openController(i);
            break;
        }
    }
    return m_controller != nullptr;
}

void Gamepad::openController(int deviceIndex)
{
    m_controller = SDL_GameControllerOpen(deviceIndex);
    if (!m_controller)
        return;
    const char *n = SDL_GameControllerName(m_controller);
    m_name = n ? QString::fromUtf8(n) : QStringLiteral("gamepad");
    emit connectedChanged();
}

void Gamepad::closeController()
{
    if (m_controller) {
        SDL_GameControllerClose(m_controller);
        m_controller = nullptr;
        m_name.clear();
        emit connectedChanged();
    }
}

void Gamepad::emitKey(Qt::Key key, bool pressed)
{
    QKeyEvent press(pressed ? QEvent::KeyPress : QEvent::KeyRelease, key,
                    Qt::NoModifier);
    if (QGuiApplication::focusObject())
        QGuiApplication::sendEvent(QGuiApplication::focusObject(), &press);
    else if (QGuiApplication::focusWindow())
        QGuiApplication::sendEvent(QGuiApplication::focusWindow(), &press);
}

void Gamepad::pump()
{
    if (!m_sdlReady)
        return;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_CONTROLLERDEVICEADDED:
            if (!m_controller)
                openController(ev.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            closeController();
            break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP: {
            const bool down = ev.type == SDL_CONTROLLERBUTTONDOWN;
            using CK = SDL_GameControllerButton;
            switch (CK(ev.cbutton.button)) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                emitKey(Qt::Key_Up, down);
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                emitKey(Qt::Key_Down, down);
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                emitKey(Qt::Key_Left, down);
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                emitKey(Qt::Key_Right, down);
                break;
            case SDL_CONTROLLER_BUTTON_A:
                emitKey(Qt::Key_Return, down);
                break;
            case SDL_CONTROLLER_BUTTON_B:
                emitKey(Qt::Key_Escape, down);
                break;
            case SDL_CONTROLLER_BUTTON_X:
                emitKey(Qt::Key_F, down);
                break;
            case SDL_CONTROLLER_BUTTON_Y:
                emitKey(Qt::Key_Slash, down);
                break;
            case SDL_CONTROLLER_BUTTON_START:
                emitKey(Qt::Key_Space, down);
                break;
            case SDL_CONTROLLER_BUTTON_BACK:
                emitKey(Qt::Key_M, down);
                break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                emitKey(Qt::Key_PageUp, down);
                break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                emitKey(Qt::Key_PageDown, down);
                break;
            case SDL_CONTROLLER_BUTTON_LEFTSTICK:
                emitKey(Qt::Key_R, down);
                break;
            default:
                break;
            }
            break;
        }
        case SDL_CONTROLLERAXISMOTION: {
            if (ev.caxis.axis != SDL_CONTROLLER_AXIS_LEFTX &&
                ev.caxis.axis != SDL_CONTROLLER_AXIS_LEFTY)
                break;
            const qint16 v = ev.caxis.value;
            const qint16 ON = 12000, OFF = 8000; // hysteresis
            if (ev.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
                if (v < -ON && !m_leftX) {
                    m_leftX = true;
                    emitKey(Qt::Key_Left, true);
                } else if (v > -OFF && m_leftX) {
                    m_leftX = false;
                    emitKey(Qt::Key_Left, false);
                }
                if (v > ON && !m_rightX) {
                    m_rightX = true;
                    emitKey(Qt::Key_Right, true);
                } else if (v < OFF && m_rightX) {
                    m_rightX = false;
                    emitKey(Qt::Key_Right, false);
                }
            } else {
                if (v < -ON && !m_upY) {
                    m_upY = true;
                    emitKey(Qt::Key_Up, true);
                } else if (v > -OFF && m_upY) {
                    m_upY = false;
                    emitKey(Qt::Key_Up, false);
                }
                if (v > ON && !m_downY) {
                    m_downY = true;
                    emitKey(Qt::Key_Down, true);
                } else if (v < OFF && m_downY) {
                    m_downY = false;
                    emitKey(Qt::Key_Down, false);
                }
            }
            break;
        }
        default:
            break;
        }
    }
}
#endif // WORLDTV_NO_SDL
