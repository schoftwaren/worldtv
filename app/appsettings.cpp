#include "appsettings.h"

#include <QHash>
#include <QKeySequence>
#include <QSettings>

using namespace Qt::Literals::StringLiterals;

namespace {
struct ActionDefault {
    const char *action;
    Qt::Key key;
};

// name -> default key. Shipped couch layout (see README): the settings
// overlay rebinds these; gamepad buttons stay fixed.
const QVector<ActionDefault> kDefaults = {
    {"play", Qt::Key_Return},     // activate tile / confirm
    {"back", Qt::Key_Escape},     // back / close overlay
    {"favorite", Qt::Key_F},      // toggle fav on focused tile
    {"search", Qt::Key_Slash},    // focus search field
    {"pause", Qt::Key_Space},     // player: pause
    {"mute", Qt::Key_M},          // player: mute
    {"reload", Qt::Key_R},        // player: retry stream 0
    {"nextFeed", Qt::Key_N},      // player: next mirror
    {"prevChannel", Qt::Key_PageUp},
    {"nextChannel", Qt::Key_PageDown},
};

const QHash<QString, int> defaultTable()
{
    QHash<QString, int> t;
    for (const auto &d : kDefaults)
        t.insert(QLatin1String(d.action), int(d.key));
    return t;
}
} // namespace

AppSettings &AppSettings::instance()
{
    static AppSettings s;
    return s;
}

QVariantList AppSettings::actionNames() const
{
    QVariantList out;
    out.reserve(kDefaults.size());
    for (const auto &d : kDefaults)
        out.append(QLatin1String(d.action));
    return out;
}

AppSettings::AppSettings(QObject *parent) : QObject(parent)
{
    loadKeys();
}

void AppSettings::loadKeys()
{
    const QHash<QString, int> defaults = defaultTable();
    QSettings s;
    s.beginGroup(QStringLiteral("keys"));
    for (auto it = defaults.constBegin(); it != defaults.constEnd(); ++it) {
        const QVariant v = s.value(it.key(), it.value());
        bool ok = false;
        const int k = v.toInt(&ok);
        m_keys.insert(it.key(), ok ? k : it.value());
    }
    s.endGroup();
}

int AppSettings::key(const QString &action) const
{
    return m_keys.value(action, Qt::Key_unknown);
}

QString AppSettings::keyLabelOf(int qtKey)
{
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
        return QString(QChar(u'A' + (qtKey - Qt::Key_A)));
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F35)
        return QStringLiteral("F%1").arg(qtKey - Qt::Key_F1 + 1);
    switch (qtKey) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return QStringLiteral("Enter");
    case Qt::Key_Escape:
        return QStringLiteral("Esc");
    case Qt::Key_Space:
        return QStringLiteral("Space");
    case Qt::Key_Slash:
        return QStringLiteral("/");
    case Qt::Key_PageUp:
        return QStringLiteral("PgUp");
    case Qt::Key_PageDown:
        return QStringLiteral("PgDn");
    case Qt::Key_Tab:
        return QStringLiteral("Tab");
    case Qt::Key_Backspace:
        return QStringLiteral("Bksp");
    case Qt::Key_Insert:
        return QStringLiteral("Ins");
    case Qt::Key_Delete:
        return QStringLiteral("Del");
    case Qt::Key_Home:
        return QStringLiteral("Home");
    case Qt::Key_End:
        return QStringLiteral("End");
    case Qt::Key_Minus:
        return QStringLiteral("-");
    case Qt::Key_Plus:
        return QStringLiteral("+");
    case Qt::Key_BracketLeft:
        return QStringLiteral("[");
    case Qt::Key_Comma:
        return QStringLiteral(",");
    case Qt::Key_Period:
        return QStringLiteral(".");
    case Qt::Key_Left:
        return QStringLiteral("Left");
    case Qt::Key_Right:
        return QStringLiteral("Right");
    case Qt::Key_Up:
        return QStringLiteral("Up");
    case Qt::Key_Down:
        return QStringLiteral("Down");
    default:
        break;
    }
    const QString s = QKeySequence(qtKey).toString(QKeySequence::NativeText);
    return s.isEmpty() ? QStringLiteral("?") : s;
}

QString AppSettings::keyLabel(const QString &action) const
{
    return keyLabelOf(key(action));
}

void AppSettings::setKey(const QString &action, int qtKey)
{
    if (!m_keys.contains(action) || m_keys.value(action) == qtKey)
        return;
    // Unbind a duplicate binding of the same key elsewhere: one key, one
    // action (the settings row shows "—" so the user sees the conflict
    // resolution instead of two actions fighting over one key).
    for (auto it = m_keys.begin(); it != m_keys.end(); ++it) {
        if (it.key() != action && it.value() == qtKey) {
            m_keys.insert(it.key(), Qt::Key_unknown);
            saveKey(it.key());
        }
    }
    m_keys.insert(action, qtKey);
    saveKey(action);
    emit keysChanged();
}

void AppSettings::resetKeys()
{
    const QHash<QString, int> defaults = defaultTable();
    QSettings s;
    s.beginGroup(QStringLiteral("keys"));
    s.remove(QString());
    s.endGroup();
    m_keys = defaults;
    emit keysChanged();
}

void AppSettings::saveKey(const QString &action)
{
    QSettings s;
    s.beginGroup(QStringLiteral("keys"));
    s.setValue(action, m_keys.value(action));
    s.endGroup();
}

bool AppSettings::gamepadConnected() const
{
    return m_gpConnected;
}

QString AppSettings::gamepadName() const
{
    return m_gpName;
}

void AppSettings::setGamepadInfo(bool connected, const QString &name)
{
    m_gpConnected = connected;
    m_gpName = name;
    emit gamepadConnectedChanged();
}

void AppSettings::beginCapture(const QString &action)
{
    m_captureAction = action;
    m_capturing = true;
    emit captureChanged();
}

void AppSettings::cancelCapture()
{
    m_captureAction.clear();
    m_capturing = false;
    emit captureChanged();
}

bool AppSettings::captureKey(int qtKey)
{
    if (!m_capturing)
        return false;
    // Esc cancels the capture (never becomes a binding) — rebind "back"
    // with Enter/PgDn/... instead.
    if (qtKey == Qt::Key_Escape) {
        cancelCapture();
        return true;
    }
    setKey(m_captureAction, qtKey);
    m_captureAction.clear();
    m_capturing = false;
    emit captureChanged();
    return true;
}
