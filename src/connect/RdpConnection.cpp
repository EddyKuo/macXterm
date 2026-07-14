#include "connect/RdpConnection.h"

#ifdef MACXTERM_HAVE_FREERDP
#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/codec/color.h>
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
    // Accept self-signed / untrusted certs when explicitly requested (test
    // fixtures, or user opt-in for a known host).
    if (session.param("ignorecert") == "1")
        freerdp_settings_set_bool(settings, FreeRDP_IgnoreCertificate, TRUE);

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
