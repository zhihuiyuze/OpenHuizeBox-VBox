/* $Id$ */
/** @file
 * OpenHuizeBox host-side sandbox-RTT activity driver -- implementation.
 *
 * See UIOhbRttDriver.h for design and threat-model rationale (host-side
 * only, no guest agent).
 */

#include <QTimer>
#include <QVector>
#include <QRandomGenerator>

#include "UIOhbRttDriver.h"
#include "UIGlobalSession.h"

/* COM includes */
#include "CVirtualBox.h"
#include "CMachine.h"
#include "CConsole.h"
#include "CDisplay.h"
#include <VBox/com/VirtualBox.h>   /* CLSID_Session */

/* Tunables. Picked to look like an idle user, not a metronome. */
static const int kTickIntervalMs    = 80;
static const int kClickMinMs        = 5000;
static const int kClickJitterMs     = 11000;
static const int kDblClickMinMs     = 30000;
static const int kDblClickJitterMs  = 31000;
static const int kEnterProbeMs      = 1500;  /* inside Pafish RTT plausible window (0.5-5s) */
static const int kJitterPx          = 30;
static const int kEdgeGuardPx       = 20;

/* PS/2 Set 1 scancodes for the Enter key. Make = 0x1C, Break = 0x9C.
 * PutScancode takes a single int per call; PutScancodes takes a list. */
static const int kScanEnterMake  = 0x1C;
static const int kScanEnterBreak = 0x9C;

UIOhbRttDriver::UIOhbRttDriver(QObject *pParent /*= nullptr*/)
    : QObject(pParent)
    , m_pTimer(new QTimer(this))
    , m_screenW(1024)
    , m_screenH(768)
    , m_nextClickAfterMs(kClickMinMs + (int)QRandomGenerator::global()->bounded(kClickJitterMs))
    , m_nextDblAfterMs(kDblClickMinMs + (int)QRandomGenerator::global()->bounded(kDblClickJitterMs))
    , m_livenessCounter(0)
{
    m_pTimer->setInterval(kTickIntervalMs);
    connect(m_pTimer, &QTimer::timeout, this, &UIOhbRttDriver::sltTick);
}

UIOhbRttDriver::~UIOhbRttDriver()
{
    stop();
}

bool UIOhbRttDriver::isRunning() const
{
    return m_pTimer->isActive();
}

bool UIOhbRttDriver::start(const QUuid &uVmId)
{
    if (isRunning())
        stop();

    CVirtualBox comVBox = gpGlobalSession->virtualBox();
    if (!comVBox.isOk())
        return false;

    CMachine comMachine = comVBox.FindMachine(uVmId.toString());
    if (comMachine.isNull())
        return false;

    const KMachineState enmState = comMachine.GetState();
    if (enmState != KMachineState_Running && enmState != KMachineState_Paused)
        return false;

    /* Acquire a shared lock -- the running runtime UI holds the write lock,
     * we attach as a reader and use the same IConsole. */
    m_session.createInstance(CLSID_Session);
    comMachine.LockMachine(m_session, KLockType_Shared);
    if (!comMachine.isOk() || m_session.GetState() != KSessionState_Locked)
    {
        m_session.detach();
        return false;
    }

    CConsole console = m_session.GetConsole();
    if (console.isNull())
    {
        m_session.UnlockMachine();
        m_session.detach();
        return false;
    }

    m_mouse    = console.GetMouse();
    m_keyboard = console.GetKeyboard();
    if (m_mouse.isNull() || m_keyboard.isNull())
    {
        m_mouse.detach();
        m_keyboard.detach();
        m_session.UnlockMachine();
        m_session.detach();
        return false;
    }

    /* Probe guest resolution so jitter stays within the visible frame. */
    CDisplay display = console.GetDisplay();
    if (!display.isNull())
    {
        ULONG w = 0, h = 0, bpp = 0;
        LONG  x = 0, y = 0;
        KGuestMonitorStatus enmMon = KGuestMonitorStatus_Disabled;
        display.GetScreenResolution(0, w, h, bpp, x, y, enmMon);
        if (w > 0) m_screenW = (int)w;
        if (h > 0) m_screenH = (int)h;
    }

    m_uVmId      = uVmId;
    m_pos        = QPoint(m_screenW / 2, m_screenH / 2);
    m_lastClick  = QDateTime::currentDateTime();
    m_lastDblClk = QDateTime::currentDateTime();
    m_lastEnter  = QDateTime::currentDateTime();

    m_pTimer->start();
    return true;
}

void UIOhbRttDriver::stop()
{
    if (m_pTimer->isActive())
        m_pTimer->stop();

    m_mouse.detach();
    m_keyboard.detach();

    if (!m_session.isNull() && m_session.GetState() == KSessionState_Locked)
        m_session.UnlockMachine();
    m_session.detach();

    m_uVmId = QUuid();
}

void UIOhbRttDriver::sltTick()
{
    if (m_mouse.isNull())
        return;

    /* Liveness probe once per second (12 ticks at 80ms). If the session is
     * no longer locked -- because the user powered the VM off or the
     * runtime UI crashed -- stop cleanly and signal so the menu toggle can
     * uncheck itself. */
    if (++m_livenessCounter >= 12)
    {
        m_livenessCounter = 0;
        if (m_session.isNull() || m_session.GetState() != KSessionState_Locked)
        {
            stop();
            emit sigAutoStopped();
            return;
        }
    }

    QRandomGenerator *rng = QRandomGenerator::global();

    /* 1) random relative cursor jitter. We use PutMouseEvent (PS/2 relative
     * deltas) rather than PutMouseEventAbsolute -- absolute mode requires the
     * guest to negotiate the capability (typically via Guest Additions), and
     * since OHB builds with VBOX_WITHOUT_ADDITIONS=1 absolute injection is
     * silently dropped. Relative deltas work against the bare PS/2 emulation
     * and any HID mouse driver. */
    const int dx = (int)rng->bounded(2 * kJitterPx + 1) - kJitterPx;
    const int dy = (int)rng->bounded(2 * kJitterPx + 1) - kJitterPx;
    /* Track the synthesised position for clamping/logging only -- the actual
     * cursor position is wherever the guest cursor lands after applying the
     * deltas, not our internal accumulator. */
    m_pos.setX(qBound(kEdgeGuardPx, m_pos.x() + dx, m_screenW - kEdgeGuardPx));
    m_pos.setY(qBound(kEdgeGuardPx, m_pos.y() + dy, m_screenH - kEdgeGuardPx));
    m_mouse.PutMouseEvent(dx, dy, 0, 0, 0);

    const QDateTime nowDt = QDateTime::currentDateTime();

    /* 2) periodic single click -- defeats GetAsyncKeyState(VK_LBUTTON). */
    if (m_lastClick.msecsTo(nowDt) > m_nextClickAfterMs)
    {
        m_mouse.PutMouseEvent(0, 0, 0, 0, 0x01);
        m_mouse.PutMouseEvent(0, 0, 0, 0, 0x00);
        m_lastClick        = nowDt;
        m_nextClickAfterMs = kClickMinMs + (int)rng->bounded(kClickJitterMs);
    }

    /* 3) periodic double click. */
    if (m_lastDblClk.msecsTo(nowDt) > m_nextDblAfterMs)
    {
        for (int i = 0; i < 2; ++i)
        {
            m_mouse.PutMouseEvent(0, 0, 0, 0, 0x01);
            m_mouse.PutMouseEvent(0, 0, 0, 0, 0x00);
        }
        m_lastDblClk     = nowDt;
        m_nextDblAfterMs = kDblClickMinMs + (int)rng->bounded(kDblClickJitterMs);
    }

    /* 4) periodic Enter probe -- dismisses any modal that may be up. Period
     * ~1.5s keeps the post-dialog-appear click inside Pafish's plausible
     * (500-5000ms) window. We send a single scancode pair, not a flood. */
    if (!m_keyboard.isNull() && m_lastEnter.msecsTo(nowDt) > kEnterProbeMs)
    {
        QVector<LONG> codes;
        codes.append(kScanEnterMake);
        codes.append(kScanEnterBreak);
        m_keyboard.PutScancodes(codes);
        m_lastEnter = nowDt;
    }
}
