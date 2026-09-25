#ifndef PJSIPCONFIGMANAGER_H
#define PJSIPCONFIGMANAGER_H

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QPair>
#include <optional>

/**
 * @brief Represents an Asterisk PJSIP Extension (Endpoint, Auth, and AOR).
 */
struct PjSipExtension {
    QString number;
    QString username;
    QString password;
    QString callerId;
    QString context = "internal";
    bool enabled = true;
    QString disabledContext = "disabled-extensions";
    bool videoEnabled = false;
    QStringList customCodecs;   // If non-empty, overrides template codecs (e.g. "alaw", "ulaw", "h264")
    int maxContacts = 1;
};

/**
 * @brief Represents an Asterisk PJSIP Transport configuration.
 */
struct PjSipTransport {
    QString name;               // e.g. "transport-udp", "transport-tcp", "transport-tls"
    QString protocol = "udp";   // udp, tcp, tls, ws, wss
    QString bind = "0.0.0.0:5060";
    bool localNetOnly = false;

    // TLS Specific options
    QString certFile;
    QString privKeyFile;
    QString caListFile;
    QString tlsMethod;          // e.g. "tlsv1_2", "tlsv1_3"
    bool verifyClient = false;
    bool verifyServer = false;
};

class PjSipConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit PjSipConfigManager(QObject *parent = nullptr);
    explicit PjSipConfigManager(const QString &filePath, QObject *parent = nullptr);

    // --- File Operations ---
    bool load(const QString &filePath = QString());
    bool save(const QString &filePath = QString());
    bool reloadAsterisk(); // Executes: asterisk -rx "pjsip reload"

    // --- Extension Management ---
    bool addExtension(const PjSipExtension &ext);
    bool addExtension(const QString &number, const QString &password, const QString &callerId = QString());
    bool editExtension(const PjSipExtension &ext);
    bool removeExtension(const QString &number);
    bool containsExtension(const QString &number) const;
    std::optional<PjSipExtension> getExtension(const QString &number) const;
    QList<PjSipExtension> extensionList() const;

    // --- Extension Status & Video Control ---
    bool setExtensionEnabled(const QString &number, bool enabled, const QString &disabledContext = "disabled-extensions");
    bool isExtensionEnabled(const QString &number) const;
    bool setExtensionVideoSupport(const QString &number, bool enabled);

    // --- Transport Management ---
    bool addTransport(const PjSipTransport &transport);
    bool editTransport(const PjSipTransport &transport);
    bool removeTransport(const QString &transportName);
    std::optional<PjSipTransport> getTransport(const QString &transportName) const;
    QList<PjSipTransport> transportList() const;

    // --- Generic Section & Key-Value Management ---
    bool setConfigValue(const QString &sectionName, const QString &key, const QString &value, const QString &templateName = QString());
    bool addConfigValue(const QString &sectionName, const QString &key, const QString &value, const QString &templateName = QString());
    QString getConfigValue(const QString &sectionName, const QString &key, const QString &templateName = QString(), const QString &defaultValue = QString()) const;
    QStringList getConfigValues(const QString &sectionName, const QString &key, const QString &templateName = QString()) const;
    bool removeConfigKey(const QString &sectionName, const QString &key, const QString &templateName = QString());
    bool removeConfigValuesByKey(const QString &sectionName, const QString &key, const QString &templateName = QString());
    bool removeSection(const QString &sectionName, const QString &templateName = QString());

    // --- Template Helpers ---
    bool setGlobalSetting(const QString &key, const QString &value);
    bool setTemplateSetting(const QString &templateName, const QString &key, const QString &value);
    bool addTemplate(const QString &templateName, const QString &type, const QList<QPair<QString, QString>> &settings);

    QString lastError() const;

signals:
    void loaded();
    void saved();
    void extensionAdded(const QString &number);
    void extensionModified(const QString &number);
    void extensionRemoved(const QString &number);
    void extensionStateChanged(const QString &number, bool enabled);
    void transportAdded(const QString &name);
    void transportModified(const QString &name);
    void transportRemoved(const QString &name);
    void configChanged(const QString &sectionName, const QString &key, const QString &value);
    void errorOccurred(const QString &error);

private:
    struct Section {
        QString rawHeader;
        QString name;
        QString templateName;
        QList<QPair<QString, QString>> keyValuePairs;
        QList<QString> rawLines;
    };

    QString m_filePath;
    QString m_lastError;
    QList<Section> m_sections;

    void parseContent(const QString &content);
    QString serializeContent() const;
    int findSectionIndex(const QString &name, const QString &templateName = QString()) const;
    void updateSectionRawLines(Section &sec);
    void setError(const QString &err);
};

#endif // PJSIPCONFIGMANAGER_H
