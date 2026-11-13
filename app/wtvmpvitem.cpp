#include "wtvmpvitem.h"

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
