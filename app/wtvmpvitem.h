#pragma once

#include "mpvabstractitem.h"

#include <QVariant>

// worldtv's single mpv surface — thin subclass of mpvqt's MpvAbstractItem
// (same pattern as viddeck's VidMpvItem: upstream keeps mpvController()
// protected, subclassing is the sanctioned way to observe properties).
//
// ANDROID CRASH FIX (tombstone 2026-11-21, SIGSEGV in QJSValue::toVariant
// on the mpv worker thread): Qt 6 wraps JS-array arguments passed to a
// QVariant parameter as QVariant(QJSValue) and defers the conversion.
// MpvAbstractItem::setProperty() then QUEUES that QVariant to the mpv
// controller thread, where setNode() calls QJSValue::toVariant() — a QML
// engine call from a non-GUI thread. Racing the GUI thread's JS activity
// it intermittently faults (crash only "sometimes", emulator worse than
// desktop). setProp()/sendCommand() convert EVERYTHING to plain
// thread-safe containers on the calling (GUI) thread first.
class WtvMpvItem : public MpvAbstractItem {
    Q_OBJECT
public:
    explicit WtvMpvItem(QQuickItem *parent = nullptr);

    MpvController *controller() { return mpvController(); }

    // GUI-thread-sanitized wrappers for the base invokables. QML must use
    // these instead of setProperty()/command() when a value originates
    // from JS (arrays, objects).
    Q_INVOKABLE void setProp(const QString &property, const QVariant &value);
    Q_INVOKABLE void sendCommand(const QStringList &params);

signals:
    void propChanged(const QString &property, const QVariant &value);
    void fileEnded(const QString &reason);
    void playbackLive(); // MPV_EVENT_FILE_LOADED: demux+decode running

private:
    static QVariant sanitize(const QVariant &v);
};
