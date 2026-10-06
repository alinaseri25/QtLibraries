#include "dahuadoorcontroller.h"
#include <QUrl>
#include <QDebug>

DahuaDoorController::DahuaDoorController(QObject *parent)
    : QObject(parent)
{
    // Configure Digest Authentication handler
    connect(&m_networkManager, &QNetworkAccessManager::authenticationRequired,
            this, &DahuaDoorController::onAuthenticationRequired);
}

void DahuaDoorController::onAuthenticationRequired(QNetworkReply *reply, QAuthenticator *auth)
{
    Q_UNUSED(reply);
    // Supply stored credentials to the challenge
    auth->setUser(m_currentUser);
    auth->setPassword(m_currentPass);
}

void DahuaDoorController::openDoor(const QString &host,
                                   const QString &user,
                                   const QString &pass,
                                   int channel,
                                   quint16 port)
{
    openDoor(host, user, pass, nullptr, channel, port);
}

void DahuaDoorController::openDoor(const QString &host,
                                   const QString &user,
                                   const QString &pass,
                                   std::function<void(bool, const QString &)> callback,
                                   int channel,
                                   quint16 port)
{
    m_currentUser = user;
    m_currentPass = pass;

    QUrl url(QString("http://%1:%2/cgi-bin/accessControl.cgi").arg(host).arg(port));
    QUrlQuery query;
    query.addQueryItem("action", "openDoor");
    query.addQueryItem("channel", QString::number(channel));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setTransferTimeout(5000); // 5 seconds timeout

    QNetworkReply *reply = m_networkManager.get(request);

    // Handle asynchronous response without blocking GUI loop
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        reply->deleteLater();

        bool success = false;
        QString resultMessage;

        if (reply->error() == QNetworkReply::NoError) {
            const QString responseBody = QString::fromUtf8(reply->readAll()).trimmed();
            qDebug() << "Dahua raw response:" << responseBody;

            // Dahua returns "OK" on success
            if (responseBody.contains("OK", Qt::CaseInsensitive)) {
                success = true;
                resultMessage = "Door unlocked successfully.";
            } else {
                resultMessage = QString("Unexpected response: %1").arg(responseBody);
            }
        } else {
            resultMessage = QString("Network error: %1").arg(reply->errorString());
            qWarning() << "Request failed:" << resultMessage;
        }

        // 1. Invoke Lambda callback if provided
        if (callback) {
            callback(success, resultMessage);
        }

        // 2. Emit Qt signal for UI bindings
        emit doorResult(success, resultMessage);
    });
}
