#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QAuthenticator>
#include <QUrlQuery>
#include <functional>

class DahuaDoorController : public QObject
{
    Q_OBJECT

public:
    explicit DahuaDoorController(QObject *parent = nullptr);

    // Q_INVOKABLE allows calling this method directly from QML
    Q_INVOKABLE void openDoor(const QString &host,
                              const QString &user,
                              const QString &pass,
                              int channel = 1,
                              quint16 port = 80);

    // Overload that accepts a C++ callback lambda
    void openDoor(const QString &host,
                  const QString &user,
                  const QString &pass,
                  std::function<void(bool success, const QString &message)> callback,
                  int channel = 1,
                  quint16 port = 80);

signals:
    // Emitted when operation completes
    void doorResult(bool success, const QString &message);

private slots:
    void onAuthenticationRequired(QNetworkReply *reply, QAuthenticator *auth);

private:
    QNetworkAccessManager m_networkManager;
    QString m_currentUser;
    QString m_currentPass;
};
