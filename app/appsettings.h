#pragma once

#include <QHash>
#include <QObject>
#include <QString>

// App settings + user-rebindable keys, persisted via QSettings
// (AppDataLocation/worldtv.conf). The UI (Main.qml settings overlay) binds
// actions by NAME; AppSettings maps name -> Qt::Key so both the keyboard
// paths in QML and the SDL gamepad pump go through one table.
//
// Defaults follow the shipped couch layout. Custom bindings survive
// restarts; "reset" restores the defaults below.
class AppSettings : public QObject {
    Q_OBJECT
    // Exposed for the settings overlay (shows the gamepad legend). Not
    // rebindable — buttons are fixed, only keyboard bindings are.
    Q_PROPERTY(bool gamepadConnected READ gamepadConnected NOTIFY
                   gamepadConnectedChanged)
    Q_PROPERTY(QString gamepadName READ gamepadName NOTIFY
                   gamepadConnectedChanged)

public:
    static AppSettings &instance();

    // The action names QML and gamepad agree on. QVariantList for QML
    // marshalling (a static const QStringList& does not survive the
    // meta-object round-trip: the settings list came up empty).
    Q_INVOKABLE QVariantList actionNames() const;

    int key(const QString &action) const;      // Qt::Key
    QString keyLabel(const QString &action) const; // "F", "Space", "N" ...
    Q_INVOKABLE void setKey(const QString &action, int qtKey);
    Q_INVOKABLE void resetKeys();
    Q_INVOKABLE QString label(int qtKey) const { return keyLabelOf(qtKey); }

    // Hook the SDL pump up to this table (gamepad.cpp calls these).
    bool gamepadConnected() const;
    QString gamepadName() const;
    void setGamepadInfo(bool connected, const QString &name);

    // "New key for <action>: press any key" capture helper. Next key event
    // delivered to captureKey() is stored as the binding for `action`.
    Q_INVOKABLE void beginCapture(const QString &action);
    Q_INVOKABLE void cancelCapture();
    Q_INVOKABLE bool capturing() const { return m_capturing; }
    Q_INVOKABLE QString capturingAction() const { return m_captureAction; }
    // Feed a raw Qt::Key in (QML key handlers forward here while capturing).
    // Returns true when the capture consumed the key.
    Q_INVOKABLE bool captureKey(int qtKey);

signals:
    void keysChanged();
    void gamepadConnectedChanged();
    void captureChanged();

private:
    explicit AppSettings(QObject *parent = nullptr);
    void loadKeys();
    void saveKey(const QString &action);
    static QString keyLabelOf(int qtKey);

    QHash<QString, int> m_keys;
    QString m_captureAction;
    bool m_capturing = false;
    bool m_gpConnected = false;
    QString m_gpName;
};
