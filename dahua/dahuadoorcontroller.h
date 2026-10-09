#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QAuthenticator>
#include <QUrlQuery>
#include <functional>

/*!
 * \brief Sends network requests to a Dahua door controller.
 *
 * Door commands are issued asynchronously through QNetworkAccessManager. Results
 * are reported through the doorResult signal and, when supplied, a callback.
 */
class DahuaDoorController : public QObject
{
    Q_OBJECT

public:
    /*!
     * \brief Creates a door controller and connects its authentication handler.
     * \param parent Optional QObject parent.
     */
    explicit DahuaDoorController(QObject *parent = nullptr);

    /*!
     * \brief Requests that a Dahua device unlock the specified door channel.
     *
     * The request is sent asynchronously. Completion is reported through doorResult.
     * \param host Device hostname or IP address.
     * \param user Username used for HTTP authentication.
     * \param pass Password used for HTTP authentication.
     * \param channel Door channel to unlock; defaults to channel 1.
     * \param port HTTP service port; defaults to port 80.
     */
    Q_INVOKABLE void openDoor(const QString &host,
                              const QString &user,
                              const QString &pass,
                              int channel = 1,
                              quint16 port = 80);

    /*!
     * \brief Requests a door unlock and invokes a callback when the request completes.
     *
     * The operation is asynchronous; the callback receives the success flag and result
     * message. The doorResult signal is also emitted when the request completes.
     * \param host Device hostname or IP address.
     * \param user Username used for HTTP authentication.
     * \param pass Password used for HTTP authentication.
     * \param callback Optional completion callback; an empty callback is allowed.
     * \param channel Door channel to unlock; defaults to channel 1.
     * \param port HTTP service port; defaults to port 80.
     */
    void openDoor(const QString &host,
                  const QString &user,
                  const QString &pass,
                  std::function<void(bool success, const QString &message)> callback,
                  int channel = 1,
                  quint16 port = 80);

signals:
    /*!
     * \brief Emitted when a door unlock request completes.
     * \param success true if the device accepted the request; otherwise false.
     * \param message Human-readable result or error details.
     */
    void doorResult(bool success, const QString &message);

private slots:
    /*!
     * \brief Supplies the stored credentials in response to an HTTP authentication challenge.
     * \param reply Network reply that raised the authentication challenge.
     * \param auth Authenticator that receives the username and password.
     */
    void onAuthenticationRequired(QNetworkReply *reply, QAuthenticator *auth);

private:
    /*! \brief Network manager used to send asynchronous door-control requests. */
    QNetworkAccessManager m_networkManager;

    /*! \brief Username used to answer the current HTTP authentication challenge. */
    QString m_currentUser;

    /*! \brief Password used to answer the current HTTP authentication challenge. */
    QString m_currentPass;
};
