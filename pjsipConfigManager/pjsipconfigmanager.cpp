#include "pjsipconfigmanager.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QProcess>

PjSipConfigManager::PjSipConfigManager(QObject *parent)
    : QObject(parent)
{
}

PjSipConfigManager::PjSipConfigManager(const QString &filePath, QObject *parent)
    : QObject(parent),
    m_filePath(filePath)
{
}

void PjSipConfigManager::setError(const QString &err)
{
    m_lastError = err;
    emit errorOccurred(err);
}

QString PjSipConfigManager::lastError() const
{
    return m_lastError;
}

// ---------------------------------------------------------------------------
// File I/O & Asterisk CLI
// ---------------------------------------------------------------------------

bool PjSipConfigManager::load(const QString &filePath)
{
    if (!filePath.isEmpty()) {
        m_filePath = filePath;
    }

    if (m_filePath.isEmpty()) {
        setError("File path is empty.");
        return false;
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(QString("Unable to open file: %1").arg(file.errorString()));
        return false;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    parseContent(content);
    emit loaded();
    return true;
}

bool PjSipConfigManager::save(const QString &filePath)
{
    QString targetPath = filePath.isEmpty() ? m_filePath : filePath;
    if (targetPath.isEmpty()) {
        setError("Target file path is empty.");
        return false;
    }

    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        setError(QString("Failed to open file for writing: %1").arg(file.errorString()));
        return false;
    }

    QTextStream out(&file);
    out << serializeContent();
    file.close();

    m_filePath = targetPath;
    emit saved();
    return true;
}

bool PjSipConfigManager::reloadAsterisk()
{
    QProcess process;
    process.start("asterisk", QStringList() << "-rx" << "pjsip reload");
    if (!process.waitForFinished(5000)) {
        setError("Failed to execute 'asterisk -rx \"pjsip reload\"' (Timeout or missing binary).");
        return false;
    }

    if (process.exitCode() != 0) {
        setError(QString("Asterisk reload error: %1").arg(QString::fromUtf8(process.readAllStandardError()).trimmed()));
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Parser & Serializer
// ---------------------------------------------------------------------------

void PjSipConfigManager::parseContent(const QString &content)
{
    m_sections.clear();

    const QStringList lines = content.split('\n');
    static const QRegularExpression sectionHeaderRegex(R"(^\s*\[([^\]\(!]+)(?:\(([^)]*)\))?\])");
    static const QRegularExpression keyValueRegex(R"(^\s*([^=;]+?)\s*=\s*([^;]*))");

    Section currentSection;
    bool inSection = false;
    QList<QString> initialPreambleLines;

    for (const QString &rawLine : lines) {
        QString line = rawLine;
        if (line.endsWith('\r')) {
            line.chop(1);
        }

        QRegularExpressionMatch match = sectionHeaderRegex.match(line);
        if (match.hasMatch()) {
            if (inSection) {
                m_sections.append(currentSection);
                currentSection = Section();
            } else if (!initialPreambleLines.isEmpty()) {
                Section preambleSection;
                preambleSection.rawLines = initialPreambleLines;
                m_sections.append(preambleSection);
                initialPreambleLines.clear();
            }

            inSection = true;
            currentSection.rawHeader = line;
            currentSection.name = match.captured(1).trimmed();
            currentSection.templateName = match.captured(2).trimmed();
            currentSection.rawLines.append(line);
            continue;
        }

        if (inSection) {
            currentSection.rawLines.append(line);
            QRegularExpressionMatch kvMatch = keyValueRegex.match(line);
            if (kvMatch.hasMatch()) {
                QString key = kvMatch.captured(1).trimmed();
                QString val = kvMatch.captured(2).trimmed();
                currentSection.keyValuePairs.append(qMakePair(key, val));
            }
        } else {
            initialPreambleLines.append(line);
        }
    }

    if (inSection) {
        m_sections.append(currentSection);
    } else if (!initialPreambleLines.isEmpty()) {
        Section preambleSection;
        preambleSection.rawLines = initialPreambleLines;
        m_sections.append(preambleSection);
    }
}

void PjSipConfigManager::updateSectionRawLines(Section &sec)
{
    sec.rawLines.clear();
    sec.rawLines.append(sec.rawHeader);
    for (const auto &pair : sec.keyValuePairs) {
        sec.rawLines.append(QString("%1 = %2").arg(pair.first, pair.second));
    }
}

QString PjSipConfigManager::serializeContent() const
{
    QString output;
    QTextStream stream(&output);
    QSet<QString> processedExtensions;

    for (const Section &sec : m_sections) {
        // If it is the first endpoint of an extension, add a visual header banner
        if (sec.templateName.compare("endpoint-template", Qt::CaseInsensitive) == 0 &&
            !processedExtensions.contains(sec.name.toLower())) {
            processedExtensions.insert(sec.name.toLower());
            stream << QString("\n; ====================================================================\n");
            stream << QString("; Extension %1\n").arg(sec.name);
            stream << QString("; ====================================================================\n");
        }

        for (const QString &rawLine : sec.rawLines) {
            if (rawLine.startsWith("; ==================") || rawLine.startsWith("; Extension ")) {
                continue;
            }
            stream << rawLine << "\n";
        }
    }

    return output;
}

int PjSipConfigManager::findSectionIndex(const QString &name, const QString &templateName) const
{
    for (int i = 0; i < m_sections.size(); ++i) {
        if (m_sections[i].name.compare(name, Qt::CaseInsensitive) == 0) {
            if (templateName.isNull()) {
                return i;
            }
            if (m_sections[i].templateName.compare(templateName, Qt::CaseInsensitive) == 0) {
                return i;
            }
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Extension Operations
// ---------------------------------------------------------------------------

bool PjSipConfigManager::containsExtension(const QString &number) const
{
    return findSectionIndex(number, "endpoint-template") != -1;
}

bool PjSipConfigManager::addExtension(const QString &number, const QString &password, const QString &callerId)
{
    PjSipExtension ext;
    ext.number = number;
    ext.username = number;
    ext.password = password;
    ext.callerId = callerId.isEmpty() ? QString("\"Extension %1\" <%1>").arg(number) : callerId;
    return addExtension(ext);
}

bool PjSipConfigManager::addExtension(const PjSipExtension &ext)
{
    if (ext.number.trimmed().isEmpty() || ext.password.trimmed().isEmpty()) {
        setError("Extension number and password cannot be empty.");
        return false;
    }

    if (containsExtension(ext.number)) {
        setError(QString("Extension %1 already exists.").arg(ext.number));
        return false;
    }

    QString finalCallerId = ext.callerId.isEmpty()
                                ? QString("\"Extension %1\" <%1>").arg(ext.number)
                                : ext.callerId;
    QString finalUsername = ext.username.isEmpty() ? ext.number : ext.username;

    // 1. Endpoint Section
    Section endpointSec;
    endpointSec.name = ext.number;
    endpointSec.templateName = "endpoint-template";
    endpointSec.rawHeader = QString("[%1](endpoint-template)").arg(ext.number);

    // Set context based on enabled status
    QString activeContext = ext.enabled ? ext.context : ext.disabledContext;
    endpointSec.keyValuePairs.append(qMakePair(QString("context"), activeContext));
    endpointSec.keyValuePairs.append(qMakePair(QString("auth"), ext.number));
    endpointSec.keyValuePairs.append(qMakePair(QString("aors"), ext.number));
    endpointSec.keyValuePairs.append(qMakePair(QString("callerid"), finalCallerId));

    if (ext.videoEnabled) {
        endpointSec.keyValuePairs.append(qMakePair(QString("max_audio_streams"), QString("1")));
        endpointSec.keyValuePairs.append(qMakePair(QString("max_video_streams"), QString("1")));
        // If no custom codecs were explicitly set, ensure h264 and vp8 are available
        if (ext.customCodecs.isEmpty()) {
            endpointSec.keyValuePairs.append(qMakePair(QString("allow"), QString("h264")));
            endpointSec.keyValuePairs.append(qMakePair(QString("allow"), QString("vp8")));
        }
    }

    for (const QString &codec : ext.customCodecs) {
        endpointSec.keyValuePairs.append(qMakePair(QString("allow"), codec));
    }

    updateSectionRawLines(endpointSec);

    // 2. Auth Section
    Section authSec;
    authSec.name = ext.number;
    authSec.templateName = "auth-template";
    authSec.rawHeader = QString("[%1](auth-template)").arg(ext.number);
    authSec.keyValuePairs.append(qMakePair(QString("username"), finalUsername));
    authSec.keyValuePairs.append(qMakePair(QString("password"), ext.password));
    updateSectionRawLines(authSec);

    // 3. AOR Section
    Section aorSec;
    aorSec.name = ext.number;
    aorSec.templateName = "aor-template";
    aorSec.rawHeader = QString("[%1](aor-template)").arg(ext.number);
    if (ext.maxContacts != 1) {
        aorSec.keyValuePairs.append(qMakePair(QString("max_contacts"), QString::number(ext.maxContacts)));
    }
    updateSectionRawLines(aorSec);

    m_sections.append(endpointSec);
    m_sections.append(authSec);
    m_sections.append(aorSec);

    emit extensionAdded(ext.number);
    return true;
}

bool PjSipConfigManager::editExtension(const PjSipExtension &ext)
{
    if (!containsExtension(ext.number)) {
        setError(QString("Extension %1 does not exist.").arg(ext.number));
        return false;
    }

    if (!removeExtension(ext.number)) {
        return false;
    }

    if (!addExtension(ext)) {
        return false;
    }

    emit extensionModified(ext.number);
    return true;
}

bool PjSipConfigManager::removeExtension(const QString &number)
{
    bool removedAny = false;

    for (int i = m_sections.size() - 1; i >= 0; --i) {
        if (m_sections[i].name.compare(number, Qt::CaseInsensitive) == 0 &&
            m_sections[i].templateName.compare("!", Qt::CaseInsensitive) != 0) {
            m_sections.removeAt(i);
            removedAny = true;
        }
    }

    if (removedAny) {
        emit extensionRemoved(number);
        return true;
    }

    setError(QString("Extension %1 not found.").arg(number));
    return false;
}

std::optional<PjSipExtension> PjSipConfigManager::getExtension(const QString &number) const
{
    int epIdx = findSectionIndex(number, "endpoint-template");
    if (epIdx == -1) {
        return std::nullopt;
    }

    PjSipExtension ext;
    ext.number = number;

    const Section &epSec = m_sections[epIdx];
    for (const auto &kv : epSec.keyValuePairs) {
        if (kv.first.compare("callerid", Qt::CaseInsensitive) == 0) {
            ext.callerId = kv.second;
        } else if (kv.first.compare("context", Qt::CaseInsensitive) == 0) {
            ext.context = kv.second;
            if (kv.second.compare("disabled-extensions", Qt::CaseInsensitive) == 0 ||
                kv.second.compare("disabled", Qt::CaseInsensitive) == 0) {
                ext.enabled = false;
                ext.disabledContext = kv.second;
            } else {
                ext.enabled = true;
            }
        } else if (kv.first.compare("allow", Qt::CaseInsensitive) == 0) {
            ext.customCodecs.append(kv.second);
            if (kv.second.compare("h264", Qt::CaseInsensitive) == 0 ||
                kv.second.compare("vp8", Qt::CaseInsensitive) == 0) {
                ext.videoEnabled = true;
            }
        } else if (kv.first.compare("max_video_streams", Qt::CaseInsensitive) == 0 && kv.second.toInt() > 0) {
            ext.videoEnabled = true;
        }
    }

    int authIdx = findSectionIndex(number, "auth-template");
    if (authIdx != -1) {
        const Section &authSec = m_sections[authIdx];
        for (const auto &kv : authSec.keyValuePairs) {
            if (kv.first.compare("username", Qt::CaseInsensitive) == 0) {
                ext.username = kv.second;
            } else if (kv.first.compare("password", Qt::CaseInsensitive) == 0) {
                ext.password = kv.second;
            }
        }
    }

    int aorIdx = findSectionIndex(number, "aor-template");
    if (aorIdx != -1) {
        const Section &aorSec = m_sections[aorIdx];
        for (const auto &kv : aorSec.keyValuePairs) {
            if (kv.first.compare("max_contacts", Qt::CaseInsensitive) == 0) {
                ext.maxContacts = kv.second.toInt();
            }
        }
    }

    return ext;
}

QList<PjSipExtension> PjSipConfigManager::extensionList() const
{
    QList<PjSipExtension> list;
    for (const Section &sec : m_sections) {
        if (sec.templateName.compare("endpoint-template", Qt::CaseInsensitive) == 0) {
            auto ext = getExtension(sec.name);
            if (ext.has_value()) {
                list.append(*ext);
            }
        }
    }
    return list;
}

bool PjSipConfigManager::setExtensionEnabled(const QString &number, bool enabled, const QString &disabledContext)
{
    int epIdx = findSectionIndex(number, "endpoint-template");
    if (epIdx == -1) {
        setError(QString("Extension %1 not found.").arg(number));
        return false;
    }

    QString targetContext = enabled ? "internal" : disabledContext;

    bool foundContext = false;
    for (auto &kv : m_sections[epIdx].keyValuePairs) {
        if (kv.first.compare("context", Qt::CaseInsensitive) == 0) {
            kv.second = targetContext;
            foundContext = true;
            break;
        }
    }

    if (!foundContext) {
        m_sections[epIdx].keyValuePairs.append(qMakePair(QString("context"), targetContext));
    }

    updateSectionRawLines(m_sections[epIdx]);
    emit extensionStateChanged(number, enabled);
    emit configChanged(number, "context", targetContext);
    return true;
}

bool PjSipConfigManager::isExtensionEnabled(const QString &number) const
{
    auto ext = getExtension(number);
    if (!ext.has_value()) {
        return false;
    }
    return ext->enabled;
}

bool PjSipConfigManager::setExtensionVideoSupport(const QString &number, bool enabled)
{
    int epIdx = findSectionIndex(number, "endpoint-template");
    if (epIdx == -1) {
        setError(QString("Extension %1 not found.").arg(number));
        return false;
    }

    Section &sec = m_sections[epIdx];

    // Remove existing video entries
    for (int i = sec.keyValuePairs.size() - 1; i >= 0; --i) {
        const QString &k = sec.keyValuePairs[i].first;
        const QString &v = sec.keyValuePairs[i].second;
        if (k.compare("max_video_streams", Qt::CaseInsensitive) == 0 ||
            (k.compare("allow", Qt::CaseInsensitive) == 0 && (v.compare("h264", Qt::CaseInsensitive) == 0 || v.compare("vp8", Qt::CaseInsensitive) == 0))) {
            sec.keyValuePairs.removeAt(i);
        }
    }

    if (enabled) {
        sec.keyValuePairs.append(qMakePair(QString("max_video_streams"), QString("1")));
        sec.keyValuePairs.append(qMakePair(QString("allow"), QString("h264")));
        sec.keyValuePairs.append(qMakePair(QString("allow"), QString("vp8")));
    }

    updateSectionRawLines(sec);
    emit extensionModified(number);
    return true;
}

// ---------------------------------------------------------------------------
// Transport Operations
// ---------------------------------------------------------------------------

bool PjSipConfigManager::addTransport(const PjSipTransport &transport)
{
    if (transport.name.trimmed().isEmpty()) {
        setError("Transport name cannot be empty.");
        return false;
    }

    if (findSectionIndex(transport.name) != -1) {
        setError(QString("Transport %1 already exists.").arg(transport.name));
        return false;
    }

    Section sec;
    sec.name = transport.name;
    sec.rawHeader = QString("[%1]").arg(transport.name);
    sec.keyValuePairs.append(qMakePair(QString("type"), QString("transport")));
    sec.keyValuePairs.append(qMakePair(QString("protocol"), transport.protocol.toLower()));
    sec.keyValuePairs.append(qMakePair(QString("bind"), transport.bind));

    if (transport.protocol.toLower() == "tls" || transport.protocol.toLower() == "wss") {
        if (!transport.certFile.isEmpty()) sec.keyValuePairs.append(qMakePair(QString("cert_file"), transport.certFile));
        if (!transport.privKeyFile.isEmpty()) sec.keyValuePairs.append(qMakePair(QString("priv_key_file"), transport.privKeyFile));
        if (!transport.caListFile.isEmpty()) sec.keyValuePairs.append(qMakePair(QString("ca_list_file"), transport.caListFile));
        if (!transport.tlsMethod.isEmpty()) sec.keyValuePairs.append(qMakePair(QString("method"), transport.tlsMethod));
        sec.keyValuePairs.append(qMakePair(QString("verify_client"), transport.verifyClient ? "yes" : "no"));
        sec.keyValuePairs.append(qMakePair(QString("verify_server"), transport.verifyServer ? "yes" : "no"));
    }

    updateSectionRawLines(sec);
    m_sections.append(sec);

    emit transportAdded(transport.name);
    return true;
}

bool PjSipConfigManager::editTransport(const PjSipTransport &transport)
{
    if (!removeTransport(transport.name)) {
        return false;
    }
    if (!addTransport(transport)) {
        return false;
    }
    emit transportModified(transport.name);
    return true;
}

bool PjSipConfigManager::removeTransport(const QString &transportName)
{
    int idx = findSectionIndex(transportName);
    if (idx == -1) {
        setError(QString("Transport %1 not found.").arg(transportName));
        return false;
    }

    m_sections.removeAt(idx);
    emit transportRemoved(transportName);
    return true;
}

std::optional<PjSipTransport> PjSipConfigManager::getTransport(const QString &transportName) const
{
    int idx = findSectionIndex(transportName);
    if (idx == -1) {
        return std::nullopt;
    }

    const Section &sec = m_sections[idx];
    PjSipTransport transport;
    transport.name = sec.name;

    for (const auto &kv : sec.keyValuePairs) {
        if (kv.first.compare("protocol", Qt::CaseInsensitive) == 0) transport.protocol = kv.second;
        else if (kv.first.compare("bind", Qt::CaseInsensitive) == 0) transport.bind = kv.second;
        else if (kv.first.compare("cert_file", Qt::CaseInsensitive) == 0) transport.certFile = kv.second;
        else if (kv.first.compare("priv_key_file", Qt::CaseInsensitive) == 0) transport.privKeyFile = kv.second;
        else if (kv.first.compare("ca_list_file", Qt::CaseInsensitive) == 0) transport.caListFile = kv.second;
        else if (kv.first.compare("method", Qt::CaseInsensitive) == 0) transport.tlsMethod = kv.second;
        else if (kv.first.compare("verify_client", Qt::CaseInsensitive) == 0) transport.verifyClient = (kv.second.compare("yes", Qt::CaseInsensitive) == 0);
        else if (kv.first.compare("verify_server", Qt::CaseInsensitive) == 0) transport.verifyServer = (kv.second.compare("yes", Qt::CaseInsensitive) == 0);
    }

    return transport;
}

QList<PjSipTransport> PjSipConfigManager::transportList() const
{
    QList<PjSipTransport> list;
    for (const Section &sec : m_sections) {
        for (const auto &kv : sec.keyValuePairs) {
            if (kv.first.compare("type", Qt::CaseInsensitive) == 0 &&
                kv.second.compare("transport", Qt::CaseInsensitive) == 0) {
                auto t = getTransport(sec.name);
                if (t.has_value()) {
                    list.append(*t);
                }
                break;
            }
        }
    }
    return list;
}

// ---------------------------------------------------------------------------
// Generic Section & Key-Value Management
// ---------------------------------------------------------------------------

bool PjSipConfigManager::setConfigValue(const QString &sectionName, const QString &key, const QString &value, const QString &templateName)
{
    int secIdx = findSectionIndex(sectionName, templateName);

    if (secIdx == -1) {
        Section newSec;
        newSec.name = sectionName;
        newSec.templateName = templateName;
        newSec.rawHeader = templateName.isEmpty()
                               ? QString("[%1]").arg(sectionName)
                               : QString("[%1](%2)").arg(sectionName, templateName);
        newSec.keyValuePairs.append(qMakePair(key, value));
        updateSectionRawLines(newSec);
        m_sections.append(newSec);
    } else {
        bool found = false;
        for (auto &pair : m_sections[secIdx].keyValuePairs) {
            if (pair.first.compare(key, Qt::CaseInsensitive) == 0) {
                pair.second = value;
                found = true;
                break;
            }
        }
        if (!found) {
            m_sections[secIdx].keyValuePairs.append(qMakePair(key, value));
        }
        updateSectionRawLines(m_sections[secIdx]);
    }

    emit configChanged(sectionName, key, value);
    return true;
}

bool PjSipConfigManager::addConfigValue(const QString &sectionName, const QString &key, const QString &value, const QString &templateName)
{
    int secIdx = findSectionIndex(sectionName, templateName);
    if (secIdx == -1) {
        return setConfigValue(sectionName, key, value, templateName);
    }

    m_sections[secIdx].keyValuePairs.append(qMakePair(key, value));
    updateSectionRawLines(m_sections[secIdx]);
    emit configChanged(sectionName, key, value);
    return true;
}

QString PjSipConfigManager::getConfigValue(const QString &sectionName, const QString &key, const QString &templateName, const QString &defaultValue) const
{
    int secIdx = findSectionIndex(sectionName, templateName);
    if (secIdx == -1) return defaultValue;

    for (const auto &pair : m_sections[secIdx].keyValuePairs) {
        if (pair.first.compare(key, Qt::CaseInsensitive) == 0) {
            return pair.second;
        }
    }
    return defaultValue;
}

QStringList PjSipConfigManager::getConfigValues(const QString &sectionName, const QString &key, const QString &templateName) const
{
    QStringList values;
    int secIdx = findSectionIndex(sectionName, templateName);
    if (secIdx == -1) return values;

    for (const auto &pair : m_sections[secIdx].keyValuePairs) {
        if (pair.first.compare(key, Qt::CaseInsensitive) == 0) {
            values.append(pair.second);
        }
    }
    return values;
}

bool PjSipConfigManager::removeConfigKey(const QString &sectionName, const QString &key, const QString &templateName)
{
    int secIdx = findSectionIndex(sectionName, templateName);
    if (secIdx == -1) {
        setError(QString("Section [%1] not found.").arg(sectionName));
        return false;
    }

    for (int i = 0; i < m_sections[secIdx].keyValuePairs.size(); ++i) {
        if (m_sections[secIdx].keyValuePairs[i].first.compare(key, Qt::CaseInsensitive) == 0) {
            m_sections[secIdx].keyValuePairs.removeAt(i);
            updateSectionRawLines(m_sections[secIdx]);
            return true;
        }
    }

    setError(QString("Key '%1' not found in section [%2].").arg(key, sectionName));
    return false;
}

bool PjSipConfigManager::removeConfigValuesByKey(const QString &sectionName, const QString &key, const QString &templateName)
{
    int secIdx = findSectionIndex(sectionName, templateName);
    if (secIdx == -1) return false;

    bool removed = false;
    for (int i = m_sections[secIdx].keyValuePairs.size() - 1; i >= 0; --i) {
        if (m_sections[secIdx].keyValuePairs[i].first.compare(key, Qt::CaseInsensitive) == 0) {
            m_sections[secIdx].keyValuePairs.removeAt(i);
            removed = true;
        }
    }

    if (removed) {
        updateSectionRawLines(m_sections[secIdx]);
    }
    return removed;
}

bool PjSipConfigManager::removeSection(const QString &sectionName, const QString &templateName)
{
    int idx = findSectionIndex(sectionName, templateName);
    if (idx == -1) {
        setError(QString("Section [%1] not found.").arg(sectionName));
        return false;
    }

    m_sections.removeAt(idx);
    return true;
}

// ---------------------------------------------------------------------------
// Template & Convenience Helpers
// ---------------------------------------------------------------------------

bool PjSipConfigManager::setGlobalSetting(const QString &key, const QString &value)
{
    return setConfigValue("global", key, value, QString());
}

bool PjSipConfigManager::setTemplateSetting(const QString &templateName, const QString &key, const QString &value)
{
    return setConfigValue(templateName, key, value, "!");
}

bool PjSipConfigManager::addTemplate(const QString &templateName, const QString &type, const QList<QPair<QString, QString>> &settings)
{
    if (findSectionIndex(templateName, "!") != -1) {
        setError(QString("Template %1 already exists.").arg(templateName));
        return false;
    }

    Section sec;
    sec.name = templateName;
    sec.templateName = "!";
    sec.rawHeader = QString("[%1](!)").arg(templateName);
    sec.keyValuePairs.append(qMakePair(QString("type"), type));
    for (const auto &pair : settings) {
        sec.keyValuePairs.append(pair);
    }

    updateSectionRawLines(sec);
    m_sections.append(sec);
    return true;
}
