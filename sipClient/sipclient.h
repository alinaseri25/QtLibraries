#ifndef SIPCLIENT_H
#define SIPCLIENT_H

#include <QObject>
#include <QString>
#include <memory>
//#include <pjsua2.hpp>
#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include <mutex>

// Workaround for MinGW Unicode string function conflicts with PJSIP
#if defined(_WIN32) && !defined(_MSC_VER)
#if defined(UNICODE)
#define PJ_HAD_UNICODE
#undef UNICODE
#endif
#if defined(_UNICODE)
#define PJ_HAD_UNDERSCORE_UNICODE
#undef _UNICODE
#endif
#endif

#include <pjsua2.hpp>

// Restore Unicode macros for the remaining Qt compilation units
#if defined(_WIN32) && !defined(_MSC_VER)
#if defined(PJ_HAD_UNICODE)
#define UNICODE
#undef PJ_HAD_UNICODE
#endif
#if defined(PJ_HAD_UNDERSCORE_UNICODE)
#define _UNICODE
#undef PJ_HAD_UNDERSCORE_UNICODE
#endif
#endif


class SipClient;

// ==============================================================================
// 1. MyCall: Handles individual call states and media
// ==============================================================================
/*! \brief Bridges PJSUA2 callbacks for an individual call to the Qt SIP client. */
class MyCall : public pj::Call
{
public:
    /*! \brief Creates a call wrapper.
     * \param acc PJSUA2 account associated with the call.
     * \param call_id Existing call ID or PJSUA_INVALID_ID for a new call.
     * \param client Non-owning client pointer used to forward call events.
     */
    MyCall(pj::Account &acc, int call_id = PJSUA_INVALID_ID, SipClient *client = nullptr);
    /*! \brief Destroys the call wrapper. */
    ~MyCall() override = default;

    /*! \brief Handles a PJSUA2 call-state callback.
     * \param prm PJSUA2 callback parameters.
     */
    void onCallState(pj::OnCallStateParam &prm) override;
    /*! \brief Handles a PJSUA2 call-media callback and reports active media.
     * \param prm PJSUA2 callback parameters.
     */
    void onCallMediaState(pj::OnCallMediaStateParam &prm) override;

private:
    /*! \brief Non-owning client pointer used to dispatch call events. */
    SipClient *m_client;
};

// ==============================================================================
// 2. MyAccount: Handles SIP Registration and incoming calls
// ==============================================================================
/*! \brief Bridges PJSUA2 account-registration and incoming-call callbacks to SipClient. */
class MyAccount : public pj::Account
{
public:
    /*! \brief Creates an account callback adapter.
     * \param client Non-owning client pointer used to forward account events.
     */
    explicit MyAccount(SipClient *client = nullptr);
    /*! \brief Destroys the account callback adapter. */
    ~MyAccount() override;

    /*! \brief Handles a PJSUA2 registration-state callback.
     * \param prm PJSUA2 callback parameters.
     */
    void onRegState(pj::OnRegStateParam &prm) override;
    /*! \brief Handles an incoming call and transfers it to the SIP client.
     * \param prm PJSUA2 callback parameters, including the call ID.
     */
    void onIncomingCall(pj::OnIncomingCallParam &prm) override;

private:
    /*! \brief Non-owning client pointer used to dispatch account events. */
    SipClient *m_client;
};

// ==============================================================================
// 3. SipClient: High-level C++ service for SIP management (single-call design)
// ==============================================================================
/*!
 * \brief Qt-facing SIP service built on PJSUA2 with one tracked active call.
 *
 * PJSUA2 callback notifications are queued onto this object's Qt event-loop thread.
 */
class SipClient : public QObject
{
    Q_OBJECT

public:
    /*! \brief Creates the SIP client service.
     * \param parent Optional QObject parent.
     */
    explicit SipClient(QObject *parent = nullptr);
    /*! \brief Tears down the endpoint and any active account or call. */
    ~SipClient() override;

    // Lifecycle
    /*! \brief Creates, configures and starts the PJSUA2 endpoint.
     * \param listenPort Local UDP SIP port; defaults to 5060.
     * \param enableVideo Whether endpoint video support is enabled.
     * \return true if initialized or already ready; otherwise false.
     */
    bool initEndpoint(int listenPort = 5060, bool enableVideo = true);
    /*! \brief Hangs up the tracked call, shuts down the account and destroys the endpoint. */
    void destroyEndpoint();

    // Account & Registration
    /*! \brief Creates a SIP account and requests registration with the server.
     *
     * Registration completes asynchronously. A true return means local account
     * creation succeeded; registration status is reported through a signal.
     * \param sipServer SIP server hostname or domain.
     * \param username Account username and SIP identity user part.
     * \param password Password used for Digest authentication.
     * \param sipPort SIP server port; defaults to 5060.
     * \return true if local account creation succeeded; otherwise false.
     */
    bool registerAccount(const QString &sipServer,
                         const QString &username,
                         const QString &password,
                         int sipPort = 5060);
    /*! \brief Requests unregistration of the current valid account. */
    void unregisterAccount();

    // Call Actions (callId is kept for API compatibility; this build
    // tracks a single active call in m_currentCall)
    /*! \brief Starts an outgoing call to an extension on the configured SIP server.
     * \param destExtension Destination extension or SIP URI user part.
     * \param withVideo Whether to request video when video is enabled.
     */
    void makeCall(const QString &destExtension, bool withVideo = false);
    /*! \brief Answers the currently tracked incoming call.
     * \param callId Optional expected call ID; a negative value accepts the tracked call.
     *        A non-matching ID is rejected.
     */
    void answerCall(int callId = -1);
    /*! \brief Hangs up or declines the currently tracked call.
     * \param callId Optional expected call ID; a negative value targets the tracked call.
     *        A non-matching ID is rejected.
     */
    void hangupCall(int callId = -1);

public slots:
    /*! \brief Forwards a Qt outgoing-call request to makeCall().
     * \param destExtension Destination extension or SIP URI user part.
     * \param withVideo Whether to request video.
     */
    void onMakeCallRequested(const QString &destExtension, bool withVideo) {
        makeCall(destExtension, withVideo);
    }
    /*! \brief Forwards a Qt hangup request to hangupCall(). */
    void onHangupRequested() {
        hangupCall();
    }

signals:
    /*! \brief Reports completion of endpoint initialization.
     * \param success true if initialization succeeded.
     * \param message Initialization result or error details.
     */
    void endpointInitialized(bool success, const QString &message);
    /*! \brief Reports a change in SIP account registration state.
     * \param registered true if the account is registered.
     * \param statusCode SIP registration status code.
     * \param reason Human-readable registration status.
     */
    void registrationStateChanged(bool registered, int statusCode, const QString &reason);
    /*! \brief Reports an incoming SIP call.
     * \param callId PJSUA2 call ID.
     * \param remoteUri Remote caller's SIP URI.
     */
    void incomingCallReceived(int callId, const QString &remoteUri);
    /*! \brief Reports a change in the tracked call state.
     * \param callId PJSUA2 call ID.
     * \param stateText Human-readable call state.
     * \param lastStatusCode Last SIP response status code.
     */
    void callStateChanged(int callId, const QString &stateText, int lastStatusCode);
    /*! \brief Reports media types currently active for a call.
     * \param callId PJSUA2 call ID.
     * \param hasAudio true if active audio media is present.
     * \param hasVideo true if active video media is present.
     */
    void callMediaActive(int callId, bool hasAudio, bool hasVideo);
    /*! \brief Reports an error from a SIP operation or PJSUA2 callback.
     * \param errorMessage Human-readable error details.
     */
    void errorOccurred(const QString &errorMessage);

private:
    friend class MyAccount;
    friend class MyCall;

    // These helpers marshal PJSUA2 worker-thread callbacks onto the
    // Qt event loop thread via QMetaObject::invokeMethod (queued).
    /*! \brief Queues an incoming-call notification onto the Qt object's thread.
     * \param callId PJSUA2 call ID.
     * \param remoteUri Remote caller's SIP URI.
     */
    void notifyIncomingCall(int callId, const QString &remoteUri);
    /*! \brief Queues a call-state notification onto the Qt object's thread.
     * \param callId PJSUA2 call ID.
     * \param stateText Human-readable call state.
     * \param lastStatusCode Last SIP response status code.
     */
    void notifyCallState(int callId, const QString &stateText, int lastStatusCode);
    /*! \brief Queues active call-media details onto the Qt object's thread.
     * \param callId PJSUA2 call ID.
     * \param hasAudio true if active audio media is present.
     * \param hasVideo true if active video media is present.
     */
    void notifyCallMedia(int callId, bool hasAudio, bool hasVideo);
    /*! \brief Queues registration details onto the Qt object's thread.
     * \param registered true if the account is registered.
     * \param statusCode SIP registration status code.
     * \param reason Human-readable registration status.
     */
    void notifyRegState(bool registered, int statusCode, const QString &reason);
    /*! \brief Queues an error notification onto the Qt object's thread.
     * \param errorMessage Human-readable error details.
     */
    void notifyError(const QString &errorMessage);

private:
    /*! \brief PJSUA2 endpoint instance owned by this client. */
    std::unique_ptr<pj::Endpoint> m_endpoint;
    /*! \brief SIP account instance owned by this client. */
    std::unique_ptr<MyAccount> m_account;
    /*! \brief The single outgoing or incoming call currently tracked by this client. */
    std::unique_ptr<MyCall> m_currentCall;

    /*! \brief SIP server domain used to construct outgoing destination URIs. */
    QString m_serverDomain;
    /*! \brief Whether the PJSUA2 endpoint has been initialized successfully. */
    bool m_isInitialized = false;
    /*! \brief Whether video support is enabled for endpoint and account setup. */
    bool m_enableVideo   = true;
};

#endif // SIPCLIENT_H
