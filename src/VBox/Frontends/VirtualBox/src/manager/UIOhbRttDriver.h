/* $Id$ */
/** @file
 * OpenHuizeBox host-side sandbox-RTT activity driver.
 *
 * Drives synthetic mouse and keyboard events into a running VM via the
 * IConsole.Mouse / IConsole.Keyboard COM interfaces. No code or files are
 * installed in the guest -- the guest sees emulated PS/2 hardware input,
 * indistinguishable from a real user under any user-mode detection.
 *
 * Used to defeat sandbox-RTT (reverse-turing) checks like Pafish v0.6's
 * "missing mouse movement / click / dialog confirmation" probes during an
 * authorised analysis run.
 */

#ifndef FEQT_INCLUDED_SRC_manager_UIOhbRttDriver_h
#define FEQT_INCLUDED_SRC_manager_UIOhbRttDriver_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <QObject>
#include <QPoint>
#include <QDateTime>
#include <QUuid>

#include "CSession.h"
#include "CMouse.h"
#include "CKeyboard.h"

class QTimer;

/** Lightweight host-side activity injector for one VM at a time.
 *
 * Lifecycle:
 *   - start(QUuid) acquires a shared session, fetches Mouse/Keyboard, kicks
 *     off an 80ms QTimer that randomises cursor position and periodically
 *     injects clicks + Enter scancodes.
 *   - stop() halts the timer and unlocks the session.
 *
 * Only one VM can be driven at a time (the simulator is per-instance).
 * Re-calling start() while running first stop()s the previous target.
 */
class UIOhbRttDriver : public QObject
{
    Q_OBJECT

signals:

    /** Emitted when the driver has auto-stopped (e.g. the target VM left the
     *  Running/Paused state). Consumers should sync any UI toggle back off. */
    void sigAutoStopped();

public:

    UIOhbRttDriver(QObject *pParent = nullptr);
    ~UIOhbRttDriver();

    /** Starts driving @a uVmId. Returns false if the VM is not running or
     *  the session could not be acquired -- caller should reset any UI
     *  toggle state in that case. */
    bool start(const QUuid &uVmId);

    /** Stops driving and releases the session. Safe to call when idle. */
    void stop();

    /** Whether the driver is currently active. */
    bool isRunning() const;

    /** The VM ID currently being driven, or null QUuid when idle. */
    QUuid currentVmId() const { return m_uVmId; }

private slots:

    void sltTick();

private:

    QTimer    *m_pTimer;
    CSession   m_session;
    CMouse     m_mouse;
    CKeyboard  m_keyboard;
    QUuid      m_uVmId;

    /** Current synthesised cursor position in guest pixel coords. */
    QPoint     m_pos;
    /** Guest screen extent at start() time. Used to clamp jitter to the
     *  visible area; we don't bother re-querying on resolution change. */
    int        m_screenW;
    int        m_screenH;

    /** Schedules of next click events -- staggered so the synthesised
     *  stream looks like an idle user, not a metronome. */
    QDateTime  m_lastClick;
    QDateTime  m_lastDblClk;
    int        m_nextClickAfterMs;
    int        m_nextDblAfterMs;

    /** When we last fired a blind Enter scancode to dismiss any modal that
     *  may be up in the guest. Period ~1500ms keeps the click timing inside
     *  Pafish's "plausible" window (500-5000ms after a dialog appears). */
    QDateTime  m_lastEnter;

    /** Counts ticks since the last liveness check. Once per second we make
     *  sure the session is still locked + the console is still around; if
     *  not, we auto-stop and tell anyone watching to flip their toggle. */
    int        m_livenessCounter;
};

#endif /* !FEQT_INCLUDED_SRC_manager_UIOhbRttDriver_h */
