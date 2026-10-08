#include "sipclient.h"

#include <QDebug>
#include <QMetaObject>
#include <mutex>

// ==============================================================================
// Global mutex guarding SipClient shared state (endpoint, account, call).
// Serializes access between the Qt main thread and PJSUA2 worker threads.
// ==============================================================================
static std::mutex g_sipMutex;

// ==============================================================================
// MyCall Implementation
// ==============================================================================
MyCall::MyCall(pj::Account &acc, int call_id, SipClient *client)
    : pj::Call(acc, call_id),
      m_client(client)
{
}

void MyCall::onCallState(pj::OnCallStateParam &prm)
{
    PJ_UNUSED_ARG(prm);
    try {
        pj::CallInfo info = getInfo();
        if (m_client) {
            m_client->notifyCallState(getId(),
                                      QString::fromStdString(info.stateText),
                                      static_cast<int>(info.lastStatusCode));
        }
    } catch (pj::Error &err) {
        qWarning() << "[PJSUA2 Call Error]" << err.info().c_str();
    }
}

void MyCall::onCallMediaState(pj::OnCallMediaStateParam &prm)
{
    PJ_UNUSED_ARG(prm);
    try {
        pj::CallInfo info = getInfo();
        bool hasAudio = false;
        bool hasVideo = false;

        pj::Endpoint &ep = pj::Endpoint::instance();
        pj::AudDevManager &adm = ep.audDevManager();

        for (unsigned i = 0; i < info.media.size(); ++i) {
            if (info.media[i].type == PJMEDIA_TYPE_AUDIO &&
                info.media[i].status == PJSUA_CALL_MEDIA_ACTIVE)
            {
                hasAudio = true;
                pj::AudioMedia *audMed =
                    static_cast<pj::AudioMedia *>(getMedia(i));

                // Connect the local microphone and speaker to the call media
                adm.getCaptureDevMedia().startTransmit(*audMed);
                audMed->startTransmit(adm.getPlaybackDevMedia());
            }
            else if (info.media[i].type == PJMEDIA_TYPE_VIDEO &&
                     info.media[i].status == PJSUA_CALL_MEDIA_ACTIVE)
            {
                // Video is detected here; the actual frame injection from
                // the Qt Camera class will be attached in a later step.
                hasVideo = true;
            }
        }

        if (m_client) {
            m_client->notifyCallMedia(getId(), hasAudio, hasVideo);
        }
    } catch (pj::Error &err) {
        qWarning() << "[PJSUA2 Media Error]" << err.info().c_str();
    }
}

// ==============================================================================
// MyAccount Implementation
// ==============================================================================
MyAccount::MyAccount(SipClient *client)
    : m_client(client)
{
}

MyAccount::~MyAccount()
{
}

void MyAccount::onRegState(pj::OnRegStateParam &prm)
{
    try {
        pj::AccountInfo info = getInfo();
        bool isRegistered = (info.regStatus == PJSIP_SC_OK);
        QString reason = QString::fromStdString(info.regStatusText);

        if (m_client) {
            m_client->notifyRegState(isRegistered,
                                     static_cast<int>(info.regStatus),
                                     reason);
        }
    } catch (pj::Error &err) {
        qWarning() << "[PJSUA2 Reg Error]" << err.info().c_str();
    }
}

void MyAccount::onIncomingCall(pj::OnIncomingCallParam &prm)
{
    try {
        // unique_ptr guarantees cleanup if getInfo() throws before the
        // ownership transfer to the client below.
        auto call = std::make_unique<MyCall>(*this, prm.callId, m_client);
        pj::CallInfo info = call->getInfo();
        QString remoteUri = QString::fromStdString(info.remoteUri);

        if (m_client) {
            std::lock_guard<std::mutex> lock(g_sipMutex);
            m_client->m_currentCall = std::move(call);
            m_client->notifyIncomingCall(prm.callId, remoteUri);
        }
    } catch (pj::Error &err) {
        qWarning() << "[PJSUA2 Incoming Call Error]" << err.info().c_str();
    }
}

// ==============================================================================
// SipClient Implementation
// ==============================================================================
SipClient::SipClient(QObject *parent)
    : QObject(parent)
{
}

SipClient::~SipClient()
{
    destroyEndpoint();
}

bool SipClient::initEndpoint(int listenPort, bool enableVideo)
{
    m_enableVideo = enableVideo;

    if (m_isInitialized) {
        return true;
    }

    try {
        m_endpoint = std::make_unique<pj::Endpoint>();
        m_endpoint->libCreate();

        pj::EpConfig epConfig;
        epConfig.logConfig.level = 4;
        epConfig.logConfig.consoleLevel = 4;
        epConfig.uaConfig.mainThreadOnly = false;

        m_endpoint->libInit(epConfig);

        pj::TransportConfig tcfg;
        tcfg.port = static_cast<unsigned>(listenPort);
        m_endpoint->transportCreate(PJSIP_TRANSPORT_UDP, tcfg);

        m_endpoint->libStart();

        m_isInitialized = true;
        emit endpointInitialized(true, tr("PJSUA2 started successfully"));
        return true;

    } catch (pj::Error &err) {
        QString errStr = QString("PJSIP Init Failed: %1 (status=%2)")
                             .arg(QString::fromStdString(err.info()))
                             .arg(err.status);
        qCritical() << errStr;

        // Release the partially initialized library so a later retry
        // does not hit PJ_EBUSY or leak resources.
        if (m_endpoint) {
            try {
                m_endpoint->libDestroy();
            } catch (...) {
            }
            m_endpoint.reset();
        }

        emit endpointInitialized(false, errStr);
        emit errorOccurred(errStr);
        return false;
    }
}

void SipClient::destroyEndpoint()
{
    try {
        std::lock_guard<std::mutex> lock(g_sipMutex);

        if (m_currentCall) {
            try {
                pj::CallOpParam prm(true); // default status code per call state
                m_currentCall->hangup(prm);
            } catch (...) {
            }
            m_currentCall.reset();
        }

        if (m_account) {
            m_account->shutdown();
            m_account.reset();
        }

        if (m_isInitialized && m_endpoint) {
            m_endpoint->libDestroy();
        }
    } catch (pj::Error &err) {
        qWarning() << "[PJSUA2 Destroy Warning]" << err.info().c_str();
    } catch (...) {
        qWarning() << "[PJSUA2 Destroy Warning] Unknown exception during teardown.";
    }

    // Always mark as uninitialized and release the endpoint object, even if
    // teardown threw, so we never retry operations on a half-destroyed library.
    m_isInitialized = false;
    m_endpoint.reset();
}

bool SipClient::registerAccount(const QString &sipServer,
                                const QString &username,
                                const QString &password,
                                int sipPort)
{
    if (!m_isInitialized) {
        if (!initEndpoint()) {
            return false;
        }
    }

    QString errorStr;
    {
        std::lock_guard<std::mutex> lock(g_sipMutex);

        try {
            // Explicitly tear down any previously registered account
            if (m_account) {
                m_account->shutdown();
                m_account.reset();
            }

            m_serverDomain = sipServer;

            pj::AccountConfig accCfg;
            QString idUri  = QString("sip:%1@%2").arg(username, sipServer);
            QString regUri = QString("sip:%1:%2").arg(sipServer).arg(sipPort);

            accCfg.idUri = idUri.toStdString();
            accCfg.regConfig.registrarUri = regUri.toStdString();

            pj::AuthCredInfo cred("digest", "*",
                                  username.toStdString(), 0,
                                  password.toStdString());
            accCfg.sipConfig.authCreds.push_back(cred);

            // Honor the video capability chosen at initEndpoint()
            accCfg.videoConfig.autoTransmitOutgoing = m_enableVideo;
            accCfg.videoConfig.autoShowIncoming     = m_enableVideo;

            m_account = std::make_unique<MyAccount>(this);
            m_account->create(accCfg);

            qDebug() << "[PJSUA2] Registering account:" << idUri << "to" << regUri;
            return true;

        } catch (pj::Error &err) {
            errorStr = QString("Account Registration Exception: %1")
                           .arg(QString::fromStdString(err.info()));
            qCritical() << errorStr;
        }
    }

    // Emit outside the mutex to avoid deadlocks with direct-connected slots
    emit errorOccurred(errorStr);
    return false;
}

void SipClient::unregisterAccount()
{
    {
        std::lock_guard<std::mutex> lock(g_sipMutex);
        if (m_account && m_account->isValid()) {
            try {
                m_account->setRegistration(false);
            } catch (pj::Error &err) {
                qWarning() << "[PJSUA2 Unregister Warning]" << err.info().c_str();
            }
        }
    }
}

void SipClient::makeCall(const QString &destExtension, bool withVideo)
{
    QString errorStr;
    {
        std::lock_guard<std::mutex> lock(g_sipMutex);

        if (!m_account || !m_account->isValid()) {
            errorStr = tr("Cannot make call: No valid account found.");
        } else {
            try {
                QString destUri = QString("sip:%1@%2")
                                      .arg(destExtension, m_serverDomain);
                m_currentCall = std::make_unique<MyCall>(*m_account,
                                                         PJSUA_INVALID_ID,
                                                         this);

                pj::CallOpParam prm(true);
                prm.opt.audioCount = 1;
                // A video call requires both the endpoint and the account
                // to allow video; honor the requested flag here.
                prm.opt.videoCount = (withVideo && m_enableVideo) ? 1 : 0;

                m_currentCall->makeCall(destUri.toStdString(), prm);
                qDebug() << "[PJSUA2] Outgoing call to:" << destUri;
            } catch (pj::Error &err) {
                errorStr = QString("Make Call Error: %1")
                               .arg(QString::fromStdString(err.info()));
                qCritical() << errorStr;
            }
        }
    }

    // Emit outside the mutex to avoid deadlocks with direct-connected slots
    if (!errorStr.isEmpty()) {
        emit errorOccurred(errorStr);
    }
}

void SipClient::answerCall(int callId)
{
    // Single-call design: only the tracked call can be answered. A callId
    // mismatch is rejected so a stale UI cannot answer the wrong call.
    QString errorStr;
    {
        std::lock_guard<std::mutex> lock(g_sipMutex);

        if (!m_currentCall) {
            errorStr = tr("No incoming call to answer.");
        } else if (callId >= 0 && callId != m_currentCall->getId()) {
            errorStr = tr("Call ID %1 does not match the active call.").arg(callId);
        } else {
            try {
                pj::CallOpParam prm(true);
                prm.statusCode = PJSIP_SC_OK;
                m_currentCall->answer(prm);
            } catch (pj::Error &err) {
                errorStr = QString("Answer Call Error: %1")
                               .arg(QString::fromStdString(err.info()));
                qWarning() << errorStr;
            }
        }
    }

    if (!errorStr.isEmpty()) {
        emit errorOccurred(errorStr);
    }
}

void SipClient::hangupCall(int callId)
{
    // CallOpParam with useDefault=true selects the proper status code per
    // call state (decline for ringing calls, BYE for established ones).
    QString errorStr;
    {
        std::lock_guard<std::mutex> lock(g_sipMutex);

        if (!m_currentCall) {
            return; // Nothing to hang up; no error needed.
        }
        if (callId >= 0 && callId != m_currentCall->getId()) {
            errorStr = tr("Call ID %1 does not match the active call.").arg(callId);
        } else {
            try {
                pj::CallOpParam prm(true);
                m_currentCall->hangup(prm);
            } catch (pj::Error &err) {
                errorStr = QString("Hangup Call Error: %1")
                               .arg(QString::fromStdString(err.info()));
                qWarning() << errorStr;
            }
        }
    }

    if (!errorStr.isEmpty()) {
        emit errorOccurred(errorStr);
    }
}

// ==============================================================================
// Thread-safe dispatchers: marshal PJSUA2 worker-thread callbacks onto the
// Qt event loop via queued lambdas. Signals are therefore always emitted on
// the thread that owns this SipClient object.
// ==============================================================================
void SipClient::notifyIncomingCall(int callId, const QString &remoteUri)
{
    QMetaObject::invokeMethod(this, [this, callId, remoteUri]() {
        emit incomingCallReceived(callId, remoteUri);
    }, Qt::QueuedConnection);
}

void SipClient::notifyCallState(int callId, const QString &stateText, int lastStatusCode)
{
    QMetaObject::invokeMethod(this, [this, callId, stateText, lastStatusCode]() {
        emit callStateChanged(callId, stateText, lastStatusCode);
    }, Qt::QueuedConnection);
}

void SipClient::notifyCallMedia(int callId, bool hasAudio, bool hasVideo)
{
    QMetaObject::invokeMethod(this, [this, callId, hasAudio, hasVideo]() {
        emit callMediaActive(callId, hasAudio, hasVideo);
    }, Qt::QueuedConnection);
}

void SipClient::notifyRegState(bool registered, int statusCode, const QString &reason)
{
    QMetaObject::invokeMethod(this, [this, registered, statusCode, reason]() {
        emit registrationStateChanged(registered, statusCode, reason);
    }, Qt::QueuedConnection);
}

void SipClient::notifyError(const QString &errorMessage)
{
    QMetaObject::invokeMethod(this, [this, errorMessage]() {
        emit errorOccurred(errorMessage);
    }, Qt::QueuedConnection);
}
