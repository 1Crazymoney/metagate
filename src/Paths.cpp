#include "Paths.h"

#include <QStandardPaths>
#include <QApplication>
#include <QSettings>
#include <QMessageBox>

#include <mutex>

#include "utilites/utils.h"
#include "check.h"
#include "Log.h"

constexpr auto *metahashWalletPagesPathEnv = "METAHASH_WALLET_PAGES_PATH";

const static QString WALLET_PATH_DEFAULT = ".metahash_wallets/";

const static QString METAGATE_COMMON_PATH = ".metagate/";

const static QString LOG_PATH = "logs/";

const static QString PAGES_PATH = "pages/";

const static QString SETTINGS_NAME = "settings.ini";

const static QString RUNTIME_SETTINGS_NAME = "runtimeSettings.ini";

const static QString SETTINGS_NAME_OLD = "settingsOld.ini";

const static QString STORAGE_NAME = "storage.ini";

const static QString MAC_ADDRESS_NAME = "mac.txt";

const static QString DB_PATH = "database/";

const static QString AUTOUPDATER_PATH = "autoupdater/";

const static QString NS_LOOKUP_PATH = "./";

const static QString PROXY_CONFIG_NAME = "proxy.ini";

const static QString LOCALSERVER_NAME = "metahash.metagate";

const static QString LOCALSERVERIN_NAME = "metahash.externalconnector.in";
const static QString LOCALSERVEROUT_NAME = "metahash.externalconnector.out";

const static QString TOR_PATH = QLatin1String("tor/");

const static QString TORDATA_PATH = QLatin1String("data/");

const static QString TORCONFIG_NAME = QLatin1String("torrc");

static bool isInitializePagesPath = false;

static bool isInitializeSettingsPath = false;

static QString getCommonMetagatePath() {
    return makePath(QStandardPaths::writableLocation(QStandardPaths::HomeLocation), METAGATE_COMMON_PATH);
}

QString getWalletPath() {
    const QString res = makePath(QStandardPaths::writableLocation(QStandardPaths::HomeLocation), WALLET_PATH_DEFAULT);
    createFolder(res);
    return res;
}

QString getLogPath() {
    const QString res = makePath(getCommonMetagatePath(), LOG_PATH);
    createFolder(res);
    return res;
}

QString getDbPath() {
    removeFolder(makePath(getCommonMetagatePath(), "bd"));
    const QString res = makePath(getCommonMetagatePath(), DB_PATH);
    createFolder(res);
    return res;
}

QString getNsLookupPath() {
    const QString res = makePath(getCommonMetagatePath(), NS_LOOKUP_PATH);
    createFolder(res);
    return res;
}

static QString getOldPagesPath() {
    const auto path = qgetenv(metahashWalletPagesPathEnv);
    if (!path.isEmpty())
        return QString(path);

    const QString path1(makePath(QApplication::applicationDirPath(), "startSettings/"));
    const QString path2(makePath(QApplication::applicationDirPath(), "../WalletMetahash/startSettings/"));
    const QString path3(makePath(QApplication::applicationDirPath(), "../../WalletMetahash/startSettings/"));
    QString currentBeginPath;
    QDir dirTmp;
    if (dirTmp.exists(path1)) {
        currentBeginPath = path1;
    } else if (dirTmp.exists(path2)){
        currentBeginPath = path2;
    } else {
        currentBeginPath = path3;
    }

    return currentBeginPath;
}

static void initializePagesPath() {
    CHECK(!isInitializePagesPath, "Already initialized page path");

    const QString oldPagesPath = getOldPagesPath();

    const QString newPagesPath = makePath(getCommonMetagatePath(), PAGES_PATH);

    if (!isExistFolder(newPagesPath)) {
        LOG << "Create pages folder: " << newPagesPath << " " << oldPagesPath;
        CHECK(copyRecursively(oldPagesPath, newPagesPath, true, false), "not copy pages");
        removeFile(makePath(newPagesPath, SETTINGS_NAME));
    }

    isInitializePagesPath = true;
}

QString getPagesPath() {
    CHECK(isInitializePagesPath, "Not initialize page path");

    return makePath(getCommonMetagatePath(), PAGES_PATH);
}

static void initializeSettingsPath() {
    CHECK(!isInitializeSettingsPath, "Already initialized settings path");

    const QString oldPagesPath = getOldPagesPath();
    const QString oldSettingsPath = makePath(oldPagesPath, SETTINGS_NAME);

    const QString res = getCommonMetagatePath();
    createFolder(res);
    const QString settings = makePath(res, SETTINGS_NAME);

    const QString pagesPath = getPagesPath();

    const auto replaceSettings = [&] {
        bool isNotify = false;
        if (isExistFile(settings)) {
            const QString settingsOld = makePath(res, SETTINGS_NAME_OLD);
            copyFile(settings, settingsOld, true);

            QSettings qsettings(settings, QSettings::IniFormat);
            if (qsettings.contains("notify") && qsettings.value("notify").toBool()) {
                isNotify = true;
            }
        }
        copyFile(oldSettingsPath, settings, true);
        removeFile(makePath(pagesPath, "nodes.txt"));
        removeFile(makePath(pagesPath, "servers.txt"));
        removeFile(makePath(pagesPath, "web_socket.txt"));
        removeFile(makePath(pagesPath, "version.txt"));

        if (isNotify) {
            QSettings qsettings(settings, QSettings::IniFormat);
            qsettings.setValue("notify", true);
            qsettings.sync();

            QMessageBox msgBox;
            msgBox.setText("Settings modified. See path " + settings);
            msgBox.exec();
        }
    };

    if (!isExistFile(settings)) {
        LOG << "Create settings: " << oldSettingsPath << " " << settings;
        replaceSettings();
    }

    const auto compareVersion = [&]() {// Чтобы в деструкторе закрылось
        const QSettings settingsFile(settings, QSettings::IniFormat);
        const QSettings oldSettingsFile(oldSettingsPath, QSettings::IniFormat);
        return settingsFile.value("version") == oldSettingsFile.value("version");
    };
    if (!compareVersion()) {
        LOG << "Replace settings: " << oldSettingsPath << " " << settings;
        replaceSettings();
    }

#ifdef VERSION_SETTINGS
    QSettings qsettings(settings, QSettings::IniFormat);
    CHECK(qsettings.value("version") == VERSION_SETTINGS, "Incorrect version settings");
#endif

    isInitializeSettingsPath = true;
}

QString getSettingsPath() {
    CHECK(isInitializeSettingsPath, "Not initialize settings path");

    const QString res = getCommonMetagatePath();
    const QString settings = makePath(res, SETTINGS_NAME);
    return settings;
}

QString getRuntimeSettingsPath() {
    const QString res = getCommonMetagatePath();
    const QString settings = makePath(res, RUNTIME_SETTINGS_NAME);
    return settings;
}

QString getStoragePath() {
    const QString res = getCommonMetagatePath();
    createFolder(res);
    const QString storage = makePath(res, STORAGE_NAME);
    return storage;
}

QString getMacFilePath() {
    const QString res = getCommonMetagatePath();
    createFolder(res);
    const QString storage = makePath(res, MAC_ADDRESS_NAME);
    return storage;
}

QString getAutoupdaterPath() {
    const QString path1(makePath(getCommonMetagatePath(), AUTOUPDATER_PATH));
    QDir dirTmp(path1);
    if (!dirTmp.exists()) {
        CHECK(dirTmp.mkpath(path1), "dont create autoupdater path");
    }
    return path1;
}

QString getTmpAutoupdaterPath() {
    return makePath(getAutoupdaterPath(), "folder/");
}

QString getProxyConfigPath() {
    return makePath(getCommonMetagatePath(), PROXY_CONFIG_NAME);
}

QString getLocalServerPath()
{
#ifdef Q_OS_UNIX
    return makePath(getCommonMetagatePath(), LOCALSERVER_NAME);
#else
    return LOCALSERVER_NAME;
#endif
}

QString getExternalConnectionInLocalServerPath()
{
#ifdef Q_OS_UNIX
    const QString basePath = "/tmp";
    return makePath(basePath, LOCALSERVERIN_NAME);
#else
    return LOCALSERVERIN_NAME;
#endif
}

QString getExternalConnectionOutLocalServerPath()
{
#ifdef Q_OS_UNIX
    const QString basePath = "/tmp";
    return makePath(basePath, LOCALSERVEROUT_NAME);
#else
    return LOCALSERVEROUT_NAME;
#endif
}

QString getTorConfigPath()
{
    const QString res = makePath(getCommonMetagatePath(), TOR_PATH);
    createFolder(res);
    const QString path = makePath(res, TORCONFIG_NAME);
    return path;
}

QString getTorDataPath()
{
    const QString res = makePath(getCommonMetagatePath(), TOR_PATH, TORDATA_PATH);
    createFolder(res);
    return res;
}

void clearAutoupdatersPath() {
    auto remove = [](const QString &dirPath) {
        QDir dir(dirPath);
        if (dir.exists()) {
            CHECK(dir.removeRecursively(), "dont remove autoupdater path");
            CHECK(dir.mkpath(dirPath), "dont create autoupdater path");
        }
    };

    remove(getAutoupdaterPath());
    remove(getTmpAutoupdaterPath());
}

void initializeAllPaths() {
    initializePagesPath();
    initializeSettingsPath();
}
