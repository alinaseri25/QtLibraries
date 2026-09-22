#include "pjsipconfigmanager.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QSet>

PjSipConfigManager::PjSipConfigManager(QObject *parent)
    : QObject(parent)
{
}

PjSipConfigManager::PjSipConfigManager(const QString &filePath, QObject *parent)
    : QObject(parent), m_filePath(filePath)
{
}

bool PjSipConfigManager::load(const QString &filePath)
{
    if (!filePath.isEmpty()) {
        m_filePath = filePath;
    }

    if (m_filePath.isEmpty()) {
        m_lastError = "File path is empty.";
        emit errorOccurred(m_lastError);
        return false;
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = QString("Failed to open file: %1. Error: %2")
        .arg(m_filePath, file.errorString());
        emit errorOccurred(m_lastError);
        return false;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    parseContent(content);
    emit loaded();
    return true;
}

void PjSipConfigManager::parseContent(const QString &content)
{
    m_sections.clear();
    const QStringList lines = content.split('\n');

    static const QRegularExpression headerRegex(R"(^\[([^\]\(\)]+)\](?:\(([^)]+)\))?)");
    static const QRegularExpression kvRegex(R"(^\s*([^=;#]+)\s*=\s*(.*?)\s*$)");

    Section currentSection;
    bool hasActiveSection = false;

    for (const QString &rawLine : lines) {
        QString trimmedLine = rawLine.trimmed();

        auto headerMatch = headerRegex.match(trimmedLine);
        if (headerMatch.hasMatch()) {
            if (hasActiveSection) {
                m_sections.append(currentSection);
            }

            currentSection = Section();
            currentSection.rawHeader = trimmedLine;
            currentSection.name = headerMatch.captured(1).trimmed();
            currentSection.templateName = headerMatch.captured(2).trimmed();
            currentSection.rawLines.append(rawLine);
            hasActiveSection = true;
            continue;
        }

        if (hasActiveSection) {
            currentSection.rawLines.append(rawLine);
            auto kvMatch = kvRegex.match(trimmedLine);
            if (kvMatch.hasMatch()) {
                currentSection.keyValuePairs.append(
                    qMakePair(kvMatch.captured(1).trimmed(), kvMatch.captured(2).trimmed()));
            }
        } else {
            // Lines before the first section (if any)
            if (!currentSection.rawLines.isEmpty() || !trimmedLine.isEmpty()) {
                currentSection.rawLines.append(rawLine);
            }
        }
    }

    if (hasActiveSection || !currentSection.rawLines.isEmpty()) {
        m_sections.append(currentSection);
    }
}

void PjSipConfigManager::updateSectionRawLines(Section &sec)
{
    // Re-generate rawLines from header + keyValuePairs
    sec.rawLines.clear();
    sec.rawLines.append(sec.rawHeader);
    for (const auto &pair : sec.keyValuePairs) {
        sec.rawLines.append(QString("%1 = %2").arg(pair.first, pair.second));
    }
}

QString PjSipConfigManager::serializeContent() const
{
    QString output;
    QTextStream out(&output);

    QSet<QString> processedExtensions;

    for (int i = 0; i < m_sections.size(); ++i) {
        const auto &sec = m_sections.at(i);

        // Check if this section is an extension endpoint
        bool isEndpoint = (sec.templateName == "endpoint-template");

        if (isEndpoint && !processedExtensions.contains(sec.name)) {
            // Write standard clean separator banner
            out << "\n; ==============================\n";
            out << QString("; Extension %1\n").arg(sec.name);
            out << "; ==============================\n";
            processedExtensions.insert(sec.name);
        }

        // Write section lines, filtering out any stray old banner comments
        for (const auto &line : sec.rawLines) {
            QString trimmed = line.trimmed();

            // Skip old leftover banners so they don't duplicate
            if (trimmed.startsWith("; ==============================") ||
                trimmed.startsWith("; Extension ")) {
                continue;
            }

            out << line << "\n";
        }
    }

    return output;
}

bool PjSipConfigManager::save(const QString &filePath)
{
    QString targetPath = filePath.isEmpty() ? m_filePath : filePath;
    if (targetPath.isEmpty()) {
        m_lastError = "Target save path is empty.";
        emit errorOccurred(m_lastError);
        return false;
    }

    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        m_lastError = QString("Failed to write to file: %1. Error: %2")
        .arg(targetPath, file.errorString());
        emit errorOccurred(m_lastError);
        return false;
    }

    QTextStream out(&file);
    out << serializeContent();
    out.flush();
    file.close();

    m_filePath = targetPath;
    emit saved();
    return true;
}

int PjSipConfigManager::findSectionIndex(const QString &name, const QString &templateName) const
{
    for (int i = 0; i < m_sections.size(); ++i) {
        if (m_sections.at(i).name.compare(name, Qt::CaseInsensitive) == 0 &&
            m_sections.at(i).templateName.compare(templateName, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

bool PjSipConfigManager::containsExtension(const QString &number) const
{
    return (findSectionIndex(number, "endpoint-template") != -1);
}

bool PjSipConfigManager::addExtension(const QString &number, const QString &password, const QString &callerId)
{
    PjSipExtension ext;
    ext.number = number;
    ext.password = password;
    ext.callerId = callerId.isEmpty() ? QString("\"Extension %1\" <%1>").arg(number) : callerId;
    ext.username = number;
    return addExtension(ext);
}

bool PjSipConfigManager::addExtension(const PjSipExtension &extension)
{
    if (extension.number.isEmpty() || extension.password.isEmpty()) {
        m_lastError = "Extension number and password cannot be empty.";
        emit errorOccurred(m_lastError);
        return false;
    }

    if (containsExtension(extension.number)) {
        m_lastError = QString("Extension %1 already exists.").arg(extension.number);
        emit errorOccurred(m_lastError);
        return false;
    }

    QString callerId = extension.callerId.isEmpty()
                           ? QString("\"Extension %1\" <%1>").arg(extension.number)
                           : extension.callerId;
    QString username = extension.username.isEmpty() ? extension.number : extension.username;

    // 1. Endpoint Section
    Section endpointSec;
    endpointSec.name = extension.number;
    endpointSec.templateName = "endpoint-template";
    endpointSec.rawHeader = QString("[%1](endpoint-template)").arg(extension.number);
    endpointSec.keyValuePairs.append(qMakePair(QString("auth"), extension.number));
    endpointSec.keyValuePairs.append(qMakePair(QString("aors"), extension.number));
    endpointSec.keyValuePairs.append(qMakePair(QString("callerid"), callerId));
    updateSectionRawLines(endpointSec);

    // 2. Auth Section
    Section authSec;
    authSec.name = extension.number;
    authSec.templateName = "auth-template";
    authSec.rawHeader = QString("[%1](auth-template)").arg(extension.number);
    authSec.keyValuePairs.append(qMakePair(QString("username"), username));
    authSec.keyValuePairs.append(qMakePair(QString("password"), extension.password));
    updateSectionRawLines(authSec);

    // 3. AOR Section
    Section aorSec;
    aorSec.name = extension.number;
    aorSec.templateName = "aor-template";
    aorSec.rawHeader = QString("[%1](aor-template)").arg(extension.number);
    updateSectionRawLines(aorSec);

    m_sections.append(endpointSec);
    m_sections.append(authSec);
    m_sections.append(aorSec);

    emit extensionAdded(extension.number);
    return true;
}

bool PjSipConfigManager::editExtension(const PjSipExtension &extension)
{
    if (!containsExtension(extension.number)) {
        m_lastError = QString("Extension %1 does not exist.").arg(extension.number);
        emit errorOccurred(m_lastError);
        return false;
    }

    removeExtension(extension.number);
    bool status = addExtension(extension);

    if (status) {
        emit extensionModified(extension.number);
    }
    return status;
}

bool PjSipConfigManager::removeExtension(const QString &number)
{
    bool removedAny = false;

    for (int i = m_sections.size() - 1; i >= 0; --i) {
        if (m_sections.at(i).name.compare(number, Qt::CaseInsensitive) == 0 &&
            m_sections.at(i).templateName != "!") {
            m_sections.removeAt(i);
            removedAny = true;
        }
    }

    if (removedAny) {
        emit extensionRemoved(number);
        return true;
    }

    m_lastError = QString("Extension %1 not found.").arg(number);
    emit errorOccurred(m_lastError);
    return false;
}

std::optional<PjSipExtension> PjSipConfigManager::getExtension(const QString &number) const
{
    if (!containsExtension(number)) {
        return std::nullopt;
    }

    PjSipExtension ext;
    ext.number = number;

    int endpointIdx = findSectionIndex(number, "endpoint-template");
    if (endpointIdx != -1) {
        for (const auto &pair : m_sections.at(endpointIdx).keyValuePairs) {
            if (pair.first == "callerid") {
                ext.callerId = pair.second;
            }
        }
    }

    int authIdx = findSectionIndex(number, "auth-template");
    if (authIdx != -1) {
        for (const auto &pair : m_sections.at(authIdx).keyValuePairs) {
            if (pair.first == "username") {
                ext.username = pair.second;
            } else if (pair.first == "password") {
                ext.password = pair.second;
            }
        }
    }

    return ext;
}

QList<PjSipExtension> PjSipConfigManager::extensionList() const
{
    QList<PjSipExtension> list;
    for (const auto &sec : m_sections) {
        if (sec.templateName == "endpoint-template") {
            auto extOpt = getExtension(sec.name);
            if (extOpt.has_value()) {
                list.append(extOpt.value());
            }
        }
    }
    return list;
}

// ----------------------------------------------------
// Base Configuration & Template Modification Methods
// ----------------------------------------------------

bool PjSipConfigManager::setConfigValue(const QString &sectionName, const QString &key, const QString &value, const QString &templateName)
{
    int idx = findSectionIndex(sectionName, templateName);
    if (idx == -1) {
        // Create the section if it does not exist
        Section newSec;
        newSec.name = sectionName;
        newSec.templateName = templateName;
        if (templateName.isEmpty()) {
            newSec.rawHeader = QString("[%1]").arg(sectionName);
        } else {
            newSec.rawHeader = QString("[%1](%2)").arg(sectionName, templateName);
        }
        newSec.keyValuePairs.append(qMakePair(key, value));
        updateSectionRawLines(newSec);
        m_sections.append(newSec);
    } else {
        Section &sec = m_sections[idx];
        bool foundKey = false;
        for (auto &pair : sec.keyValuePairs) {
            if (pair.first.compare(key, Qt::CaseInsensitive) == 0) {
                pair.second = value;
                foundKey = true;
                break;
            }
        }
        if (!foundKey) {
            sec.keyValuePairs.append(qMakePair(key, value));
        }
        updateSectionRawLines(sec);
    }

    emit configChanged(sectionName, key, value);
    return true;
}

QString PjSipConfigManager::getConfigValue(const QString &sectionName, const QString &key, const QString &templateName, const QString &defaultValue) const
{
    int idx = findSectionIndex(sectionName, templateName);
    if (idx == -1) {
        return defaultValue;
    }

    for (const auto &pair : m_sections.at(idx).keyValuePairs) {
        if (pair.first.compare(key, Qt::CaseInsensitive) == 0) {
            return pair.second;
        }
    }
    return defaultValue;
}

bool PjSipConfigManager::removeConfigKey(const QString &sectionName, const QString &key, const QString &templateName)
{
    int idx = findSectionIndex(sectionName, templateName);
    if (idx == -1) {
        return false;
    }

    Section &sec = m_sections[idx];
    for (int i = 0; i < sec.keyValuePairs.size(); ++i) {
        if (sec.keyValuePairs.at(i).first.compare(key, Qt::CaseInsensitive) == 0) {
            sec.keyValuePairs.removeAt(i);
            updateSectionRawLines(sec);
            return true;
        }
    }
    return false;
}

bool PjSipConfigManager::setGlobalSetting(const QString &key, const QString &value)
{
    return setConfigValue("global", key, value, "");
}

bool PjSipConfigManager::setTransportUdpSetting(const QString &key, const QString &value)
{
    return setConfigValue("transport-udp", key, value, "");
}

bool PjSipConfigManager::setTemplateSetting(const QString &templateName, const QString &key, const QString &value)
{
    // Asterisk template definitions use (!) format, e.g. [endpoint-template](!)
    return setConfigValue(templateName, key, value, "!");
}

QString PjSipConfigManager::lastError() const
{
    return m_lastError;
}
