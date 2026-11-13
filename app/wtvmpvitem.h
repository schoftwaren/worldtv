#pragma once

#include "mpvabstractitem.h"

// worldtv's single mpv surface — thin subclass of mpvqt's MpvAbstractItem
// (same pattern as viddeck's VidMpvItem: upstream keeps mpvController()
// protected, subclassing is the sanctioned way to observe properties).
class WtvMpvItem : public MpvAbstractItem {
    Q_OBJECT
public:
    explicit WtvMpvItem(QQuickItem *parent = nullptr);

    MpvController *controller() { return mpvController(); }

signals:
    void propChanged(const QString &property, const QVariant &value);
    void fileEnded(const QString &reason);
    void playbackLive(); // MPV_EVENT_FILE_LOADED: demux+decode running
};
