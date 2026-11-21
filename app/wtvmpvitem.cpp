#include "wtvmpvitem.h"

#include <QJSValue>
#include <QJSValueIterator>
#include <QVariantList>
#include <QVariantMap>

WtvMpvItem::WtvMpvItem(QQuickItem *parent)
    : MpvAbstractItem(parent)
{
    if (auto *c = controller()) {
        connect(c, &MpvController::propertyChanged, this,
                &WtvMpvItem::propChanged, Qt::QueuedConnection);
        connect(c, &MpvController::endFile, this, &WtvMpvItem::fileEnded,
                Qt::QueuedConnection);
        connect(c, &MpvController::fileLoaded, this, &WtvMpvItem::playbackLive,
                Qt::QueuedConnection);
        // mpv_format is a C enum — not visible from QML — so the property
        // observations live here (viddeck does the same in its controller).
        c->observeProperty(QStringLiteral("pause"), MPV_FORMAT_FLAG, 1);
    }
}

// Deep-convert on the CALLING thread. QJSValue (JS array/object from QML)
// is unwrapped here — while the QML engine is reachable — instead of on
// mpv's controller thread inside setNode(), which is a data race on the
// JS heap and intermittently SEGVs (Android tombstone 2026-11-21).
QVariant WtvMpvItem::sanitize(const QVariant &v)
{
    if (v.canConvert<QJSValue>()) {
        const QJSValue js = v.value<QJSValue>();
        if (js.isArray()) {
            QVariantList out;
            const int len = int(js.property(QStringLiteral("length")).toInt());
            out.reserve(len);
            for (int i = 0; i < len; ++i)
                out.append(sanitize(js.property(quint32(i)).toVariant()));
            return out;
        }
        if (js.isObject()) {
            QVariantMap out;
            QJSValueIterator it(js);
            while (it.hasNext()) {
                it.next();
                out.insert(it.name(), sanitize(it.value().toVariant()));
            }
            return out;
        }
        return js.toVariant();
    }
    if (v.typeId() == QMetaType::QVariantList) {
        QVariantList out;
        out.reserve(v.toList().size());
        for (const QVariant &e : v.toList())
            out.append(sanitize(e));
        return out;
    }
    return v;
}

void WtvMpvItem::setProp(const QString &property, const QVariant &value)
{
    setProperty(property, sanitize(value));
}

void WtvMpvItem::sendCommand(const QStringList &params)
{
    // QStringList members are plain strings — already thread-safe.
    command(params);
}
