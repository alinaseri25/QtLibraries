#ifndef PJSIPCONFIGMANAGER_H
#define PJSIPCONFIGMANAGER_H

#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QPair>
#include <optional>

struct PjSipExtension
{
    QString number;
    QString password;
    QString callerId;
    QString username;
};

class PjSipConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit PjSipConfigManager(QObject *parent = nullptr);
    explicit PjSipConfigManager(const QString &filePath, QObject *parent = nullptr);

    // File I/O
    bool load(const QString &filePath = QString());
    bool save(const QString &filePath = QString());

    // CRUD operations for Asterisk PJSIP extensions
    bool addExtension(const PjSipExtension &extension);
    bool addExtension(const QString &number, const QString &password, const QString &callerId = QString());
    bool editExtension(const PjSipExtension &extension);
    bool removeExtension(const QString &number);

    // Queries for extensions
    bool containsExtension(const QString &number) const;
    std::optional<PjSipExtension> getExtension(const QString &number) const;
    QList<PjSipExtension> extensionList() const;

    // Generic Section / Key-Value Editing (Global, Transport, Templates, etc.)
    bool setConfigValue(const QString &sectionName, const QString &key, const QString &value, const QString &templateName = QString());
    QString getConfigValue(const QString &sectionName, const QString &key, const QString &templateName = QString(), const QString &defaultValue = QString()) const;
    bool removeConfigKey(const QString &sectionName, const QString &key, const QString &templateName = QString());

    // Helper utilities for base settings
    bool setGlobalSetting(const QString &key, const QString &value);
    bool setTransportUdpSetting(const QString &key, const QString &value);
    bool setTemplateSetting(const QString &templateName, const QString &key, const QString &value);

    QString lastError() const;

signals:
    void loaded();
    void saved();
    void extensionAdded(const QString &number);
    void extensionModified(const QString &number);
    void extensionRemoved(const QString &number);
    void configChanged(const QString &sectionName, const QString &key, const QString &value);
    void errorOccurred(const QString &error);

private:
    struct Section
    {
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
    int findSectionIndex(const QString &name, const QString &templateName) const;
    void updateSectionRawLines(Section &sec);
};

#endif // PJSIPCONFIGMANAGER_H
