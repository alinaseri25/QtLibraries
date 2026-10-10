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
class MyCall : public pj::Call
{
public:
    MyCall(pj::Account &acc, int call_id = PJSUA_INVALID_ID, SipClient *client = nullptr);
    ~MyCall() override = default;

    void onCallState(pj::OnCallStateParam &prm) override;
    void onCallMediaState(pj::OnCallMediaStateParam &prm) override;

private:
    SipClient *m_client;
};

// ==============================================================================
// 2. MyAccount: Handles SIP Registration and incoming calls
// ==============================================================================
class MyAccount : public pj::Account
{
public:
    explicit MyAccount(SipClient *client = nullptr);
    ~MyAccount() override;

    void onRegState(pj::OnRegStateParam &prm) override;
    void onIncomingCall(pj::OnIncomingCallParam &prm) override;

private:
    SipClient *m_client;
};

// ==============================================================================
// 3. SipClient: High-level C++ service for SIP management (single-call design)
// ==============================================================================
class SipClient : public QObject
{
    Q_OBJECT

public:
    explicit SipClient(QObject *parent = nullptr);
    ~SipClient() override;

    // Lifecycle
    bool initEndpoint(int listenPort = 5060, bool enableVideo = true);
    void destroyEndpoint();

    // Account & Registration
    bool registerAccount(const QString &sipServer,
                         const QString &username,
                         const QString &password,
                         int sipPort = 5060);
    void unregisterAccount();

    // Call Actions (callId is kept for API compatibility; this build
    // tracks a single active call in m_currentCall)
    void makeCall(const QString &destExtension, bool withVideo = false);
    void answerCall(int callId = -1);
    void hangupCall(int callId = -1);

public slots:
    void onMakeCallRequested(const QString &destExtension, bool withVideo) {
        makeCall(destExtension, withVideo);
    }
    void onHangupRequested() {
        hangupCall();
    }

signals:
    void endpointInitialized(bool success, const QString &message);
    void registrationStateChanged(bool registered, int statusCode, const QString &reason);
    void incomingCallReceived(int callId, const QString &remoteUri);
    void callStateChanged(int callId, const QString &stateText, int lastStatusCode);
    void callMediaActive(int callId, bool hasAudio, bool hasVideo);
    void errorOccurred(const QString &errorMessage);

private:
    friend class MyAccount;
    friend class MyCall;

    // These helpers marshal PJSUA2 worker-thread callbacks onto the
    // Qt event loop thread via QMetaObject::invokeMethod (queued).
    void notifyIncomingCall(int callId, const QString &remoteUri);
    void notifyCallState(int callId, const QString &stateText, int lastStatusCode);
    void notifyCallMedia(int callId, bool hasAudio, bool hasVideo);
    void notifyRegState(bool registered, int statusCode, const QString &reason);
    void notifyError(const QString &errorMessage);

private:
    std::unique_ptr<pj::Endpoint> m_endpoint;
    std::unique_ptr<MyAccount> m_account;
    std::unique_ptr<MyCall> m_currentCall;

    QString m_serverDomain;
    bool m_isInitialized = false;
    bool m_enableVideo   = true;
};

#endif // SIPCLIENT_H
