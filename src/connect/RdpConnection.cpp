#include "connect/RdpConnection.h"

#ifdef MACXTERM_HAVE_FREERDP
#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/codec/color.h>
#include <freerdp/input.h>
#include <freerdp/scancode.h>
#endif

namespace macxterm::connect {

QStringList RdpConnection::buildFreeRdpArgs(const core::Session& session) {
    // Maps a Session to FreeRDP-style command args (also used to configure the
    // linked FreeRDP context field-by-field when MACXTERM_HAVE_FREERDP is set).
    QStringList args;
    args << QStringLiteral("/v:%1:%2").arg(session.host()).arg(session.port());
    if (!session.username().isEmpty()) args << QStringLiteral("/u:%1").arg(session.username());
    if (session.param("admin") == "1") args << QStringLiteral("/admin");
    if (session.param("clipboard", "1") != "0") args << QStringLiteral("+clipboard");
    if (session.param("drives") == "1") args << QStringLiteral("/drive:home,/");
    return args;
}

RdpConnection::RdpConnection(QObject* parent) : IConnection(parent) {}

bool RdpConnection::connectSession(const core::Session& session) {
    setState(State::Connecting);
#ifdef MACXTERM_HAVE_FREERDP
    // Real FreeRDP 3 context bring-up: create instance, populate connection
    // settings from the Session, and attempt to connect. The framebuffer surface
    // is bound in RdpSurfaceWidget (GUI); this establishes the protocol session.
    m_instance = freerdp_new();
    if (!m_instance || !freerdp_context_new(m_instance)) {
        setState(State::Failed);
        emit errorOccurred(QStringLiteral("Failed to create FreeRDP context"));
        return false;
    }
    rdpSettings* settings = m_instance->context->settings;
    freerdp_settings_set_string(settings, FreeRDP_ServerHostname, session.host().toUtf8().constData());
    freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, static_cast<UINT32>(session.port()));
    if (!session.username().isEmpty())
        freerdp_settings_set_string(settings, FreeRDP_Username, session.username().toUtf8().constData());
    if (!session.param("password").isEmpty())
        freerdp_settings_set_string(settings, FreeRDP_Password, session.param("password").toUtf8().constData());
    if (!session.param("domain").isEmpty())
        freerdp_settings_set_string(settings, FreeRDP_Domain, session.param("domain").toUtf8().constData());
    // Desktop resolution (params "width"/"height"; leave FreeRDP's default when unset).
    {
        const int w = session.param("width").toInt();
        const int h = session.param("height").toInt();
        if (w > 0 && h > 0) {
            freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, static_cast<UINT32>(w));
            freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, static_cast<UINT32>(h));
        }
    }
    // Accept self-signed / untrusted certs when explicitly requested (test
    // fixtures, or user opt-in for a known host).
    if (session.param("ignorecert") == "1")
        freerdp_settings_set_bool(settings, FreeRDP_IgnoreCertificate, TRUE);

    // Device / resource redirection (MobaXterm RDP advanced options). Enabling
    // these advertises the corresponding virtual channels during negotiation.
    if (session.param("redirect_clipboard", "1") != "0")
        freerdp_settings_set_bool(settings, FreeRDP_RedirectClipboard, TRUE);
    if (session.param("redirect_drives") == "1")
        freerdp_settings_set_bool(settings, FreeRDP_RedirectDrives, TRUE);
    if (session.param("redirect_printers") == "1")
        freerdp_settings_set_bool(settings, FreeRDP_RedirectPrinters, TRUE);
    if (session.param("redirect_smartcard") == "1")
        freerdp_settings_set_bool(settings, FreeRDP_RedirectSmartCards, TRUE);
    if (session.param("redirect_audio") == "1")
        freerdp_settings_set_bool(settings, FreeRDP_AudioPlayback, TRUE);
    // RemoteFX / CredSSP (NLA) toggles.
    if (session.param("remotefx") == "1")
        freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, TRUE);
    if (session.param("nla") == "0")
        freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, FALSE);

    if (!freerdp_connect(m_instance)) {
        setState(State::Failed);
        emit errorOccurred(QStringLiteral("RDP connection to %1 failed").arg(session.host()));
        return false;
    }
    // Initialize the GDI in a 32-bpp BGRA framebuffer so currentFrame() can wrap
    // instance->context->gdi->primary_buffer directly as an ARGB QImage.
    gdi_init(m_instance, PIXEL_FORMAT_BGRA32);
    setState(State::Connected);
    return true;
#else
    (void)session;
    setState(State::Failed);
    emit errorOccurred(QStringLiteral(
        "RDP support requires building with FreeRDP (MACXTERM_HAVE_FREERDP)"));
    return false;
#endif
}

void RdpConnection::disconnectSession() {
#ifdef MACXTERM_HAVE_FREERDP
    if (m_instance) {
        freerdp_disconnect(m_instance);
        freerdp_context_free(m_instance);
        freerdp_free(m_instance);
        m_instance = nullptr;
    }
#endif
    setState(State::Disconnected);
}

qint64 RdpConnection::send(const QByteArray&) {
    return -1;   // RDP input is injected into the FreeRDP surface, not this stream
}

#ifdef MACXTERM_HAVE_FREERDP
// Map an X11 keysym (as emitted by the surface widget) to an RDP set-1 scancode
// for the non-printable keys. Returns 0 when the key should be sent as Unicode.
static UINT16 keysymToRdpScancode(quint32 keysym, bool* extended) {
    *extended = false;
    switch (keysym) {
        case 0xff08: return RDP_SCANCODE_BACKSPACE;
        case 0xff09: return RDP_SCANCODE_TAB;
        case 0xff0d: return RDP_SCANCODE_RETURN;
        case 0xff1b: return RDP_SCANCODE_ESCAPE;
        case 0xff50: *extended = true; return RDP_SCANCODE_HOME;
        case 0xff51: *extended = true; return RDP_SCANCODE_LEFT;
        case 0xff52: *extended = true; return RDP_SCANCODE_UP;
        case 0xff53: *extended = true; return RDP_SCANCODE_RIGHT;
        case 0xff54: *extended = true; return RDP_SCANCODE_DOWN;
        case 0xff55: *extended = true; return RDP_SCANCODE_PRIOR;
        case 0xff56: *extended = true; return RDP_SCANCODE_NEXT;
        case 0xff57: *extended = true; return RDP_SCANCODE_END;
        case 0xff63: *extended = true; return RDP_SCANCODE_INSERT;
        case 0xffff: *extended = true; return RDP_SCANCODE_DELETE;
        case 0xffe1: return RDP_SCANCODE_LSHIFT;
        case 0xffe3: return RDP_SCANCODE_LCONTROL;
        case 0xffe9: return RDP_SCANCODE_LMENU;
        default: return 0;
    }
}
#endif

void RdpConnection::sendPointerEvent(int x, int y, int buttonMask) {
#ifdef MACXTERM_HAVE_FREERDP
    if (!m_instance || !m_instance->context) return;
    rdpInput* input = m_instance->context->input;
    if (!input) return;
    const UINT16 px = static_cast<UINT16>(x < 0 ? 0 : x);
    const UINT16 py = static_cast<UINT16>(y < 0 ? 0 : y);
    freerdp_input_send_mouse_event(input, PTR_FLAGS_MOVE, px, py);
    // Emit a button event for each button whose state changed.
    const struct { int bit; UINT16 flag; } btns[] = {
        {1, PTR_FLAGS_BUTTON1},   // left
        {2, PTR_FLAGS_BUTTON3},   // middle
        {4, PTR_FLAGS_BUTTON2},   // right
    };
    for (const auto& b : btns) {
        const bool now = buttonMask & b.bit;
        const bool was = m_prevButtonMask & b.bit;
        if (now != was)
            freerdp_input_send_mouse_event(
                input, static_cast<UINT16>(b.flag | (now ? PTR_FLAGS_DOWN : 0)), px, py);
    }
    m_prevButtonMask = buttonMask;
#else
    (void)x; (void)y; (void)buttonMask;
#endif
}

void RdpConnection::sendKeyEvent(quint32 keysym, bool down) {
#ifdef MACXTERM_HAVE_FREERDP
    if (!m_instance || !m_instance->context) return;
    rdpInput* input = m_instance->context->input;
    if (!input) return;
    bool extended = false;
    const UINT16 sc = keysymToRdpScancode(keysym, &extended);
    if (sc != 0) {
        UINT16 flags = down ? KBD_FLAGS_DOWN : KBD_FLAGS_RELEASE;
        if (extended) flags |= KBD_FLAGS_EXTENDED;
        freerdp_input_send_keyboard_event(input, flags, sc);
    } else if (keysym >= 0x20 && keysym < 0xff00) {
        // Printable: send as a Unicode key event (layout-independent).
        freerdp_input_send_unicode_keyboard_event(
            input, down ? 0 : KBD_FLAGS_RELEASE, static_cast<UINT16>(keysym));
    }
#else
    (void)keysym; (void)down;
#endif
}

QImage RdpConnection::currentFrame() const {
#ifdef MACXTERM_HAVE_FREERDP
    if (m_instance && m_instance->context && m_instance->context->gdi) {
        rdpGdi* gdi = m_instance->context->gdi;
        if (gdi->primary_buffer && gdi->width > 0 && gdi->height > 0) {
            // Wrap the live BGRA framebuffer; .copy() detaches from FreeRDP memory
            // so the surface widget owns a stable frame.
            return QImage(gdi->primary_buffer, gdi->width, gdi->height,
                          gdi->stride, QImage::Format_ARGB32).copy();
        }
    }
#endif
    return QImage();
}

bool RdpConnection::poll() {
#ifdef MACXTERM_HAVE_FREERDP
    if (!m_instance) return false;
    if (freerdp_shall_disconnect_context(m_instance->context)) return false;
    HANDLE handles[64];
    const DWORD count = freerdp_get_event_handles(m_instance->context, handles, 64);
    if (count == 0) return false;
    WaitForMultipleObjects(count, handles, FALSE, 10);
    return freerdp_check_event_handles(m_instance->context) != FALSE;
#else
    return false;
#endif
}

} // namespace macxterm::connect
