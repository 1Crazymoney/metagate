#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <memory>
#include <map>

#include <QMainWindow>
#include <QWebChannel>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEnginePage>

#include "Network/SimpleClient.h"
#include "Uploader.h"

#include "PagesMappings.h"
#include "qt_utilites/CallbackWrapper.h"

class QSystemTrayIcon;
class WebSocketClient;
class JavascriptWrapper;
class MHUrlSchemeHandler;
class MHPayUrlSchemeHandler;
class ExternalConnectorManager;

namespace tor {
class TorProxy;
}

namespace auth {
class AuthJavascript;
class Auth;
}
namespace messenger {
class MessengerJavascript;
}

namespace Ui {
class MainWindow;
}

namespace initializer {
class InitializerJavascript;
}

namespace wallet_names {
class WalletNamesJavascript;
}

namespace utils {
class UtilsJavascript;
}

namespace wallets {
class WalletsJavascript;
}

namespace metagate {
class MetaGateJavascript;
class MetaGate;
}

namespace proxy_client {
class ProxyClient;
class ProxyClientJavascript;
}

class EvFilter: public QObject {
    Q_OBJECT
public:

    EvFilter(QObject *parent, const QString &icoActive, const QString &icoHover)
        : QObject(parent)
        , icoActive(icoActive)
        , icoHover(icoHover)
    {}

    bool eventFilter(QObject * watched, QEvent * event);

    QIcon icoActive;
    QIcon icoHover;
};

struct TypedException;

namespace transactions {
class TransactionsJavascript;
}

class WebUrlRequestInterceptor : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT

public:
    WebUrlRequestInterceptor(QObject *p)
        : QWebEngineUrlRequestInterceptor(p) {}
    void interceptRequest(QWebEngineUrlRequestInfo &info);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:

    using SetJavascriptWrapperCallback = CallbackWrapper<void()>;

    using SetAuthCallback = CallbackWrapper<void()>;

    using SetMessengerJavascriptCallback = CallbackWrapper<void()>;

    using SetTransactionsJavascriptCallback = CallbackWrapper<void()>;

    using SetWalletNamesJavascriptCallback = CallbackWrapper<void()>;

    using SetUtilsJavascriptCallback = CallbackWrapper<void()>;

    using SetWalletsJavascriptCallback = CallbackWrapper<void()>;

    using SetMetaGateJavascriptCallback = CallbackWrapper<void()>;

    using SetProxyJavascriptCallback = CallbackWrapper<void()>;

public:

    explicit MainWindow(initializer::InitializerJavascript &initializerJs, tor::TorProxy &torProxy, QWidget *parent = nullptr);

    ~MainWindow() override;

    void showExpanded();

    QString getServerIp(const QString &text, const std::set<QString> &excludesIps);

    LastHtmlVersion getCurrentHtmls() const;

    QString getCurrenttUrl() const;
    void setCurrentUrl(const QString &url);

public slots:
    void showOnTop();

    virtual void setVisible(bool visible) override;

signals:

    void setJavascriptWrapper(JavascriptWrapper *jsWrapper, const SetJavascriptWrapperCallback &callback);

    void setAuth(auth::AuthJavascript *authJavascript, auth::Auth *authManager, const SetAuthCallback &callback);

    void setMessengerJavascript(messenger::MessengerJavascript *messengerJavascript, const SetMessengerJavascriptCallback &callback);

    void setTransactionsJavascript(transactions::TransactionsJavascript *transactionsJavascript, const SetTransactionsJavascriptCallback &callback);

    void setWalletNamesJavascript(wallet_names::WalletNamesJavascript *walletNamesJavascript, const SetWalletNamesJavascriptCallback &callback);

    void setUtilsJavascript(utils::UtilsJavascript *utilsJavascript, const SetUtilsJavascriptCallback &callback);

    void setWalletsJavascript(wallets::WalletsJavascript *javascript, const SetWalletsJavascriptCallback &callback);

    void setMetaGateJavascript(metagate::MetaGate *manager, metagate::MetaGateJavascript *javascript, const SetMetaGateJavascriptCallback &callback);

    void setProxyJavascript(proxy_client::ProxyClientJavascript *javascript, const SetProxyJavascriptCallback &callback);

    void initFinished();

    void processExternalUrl(const QUrl &url);

    void showNotification(const QString &title, const QString &message);

    // External Connector
    void urlChanged(const QString &url);
    void urlEntered(const QString &url);

protected:
    virtual bool eventFilter(QObject *obj, QEvent *event) override;

    virtual void closeEvent(QCloseEvent *event) override;

    virtual void changeEvent(QEvent *event) override;

private slots:

    void onSetJavascriptWrapper(JavascriptWrapper *jsWrapper, const SetJavascriptWrapperCallback &callback);

    void onSetAuth(auth::AuthJavascript *authJavascript, auth::Auth *authManager, const SetAuthCallback &callback);

    void onSetMessengerJavascript(messenger::MessengerJavascript *messengerJavascript, const SetMessengerJavascriptCallback &callback);

    void onSetTransactionsJavascript(transactions::TransactionsJavascript *transactionsJavascript, const SetTransactionsJavascriptCallback &callback);

    void onSetWalletNamesJavascript(wallet_names::WalletNamesJavascript *walletNamesJavascript, const SetWalletNamesJavascriptCallback &callback);

    void onSetUtilsJavascript(utils::UtilsJavascript *utilsJavascript, const SetUtilsJavascriptCallback &callback);

    void onSetWalletsJavascript(wallets::WalletsJavascript *javascript, const SetWalletsJavascriptCallback &callback);

    void onSetMetaGateJavascript(metagate::MetaGate *manager, metagate::MetaGateJavascript *javascript, const SetMetaGateJavascriptCallback &callback);

    void onSetProxyJavascript(proxy_client::ProxyClientJavascript *javascript, const SetProxyJavascriptCallback &callback);

    void onInitFinished();

    void onProcessExternalUrl(const QUrl &url);

    void onShowNotification(const QString &title, const QString &message);

private:

    void loadPagesMappings();

    void softReloadPage();

    void softReloadApp();

    void loadUrl(const QString &page);

    void loadFile(const QString &pageName);

    bool currentFileIsEqual(const QString &pageName);

    void configureMenu();

    void doConfigureMenu();

    void registerCommandLine();
    void unregisterCommandLine();

    void registerUrlChangedHandler();
    void unregisterUrlChangedHandler();

    void enterCommandAndAddToHistory(const QString &text1, bool isAddToHistory, bool isNoEnterDuplicate);

    void addElementToHistoryAndCommandLine(const QString &text, bool isAddToHistory, bool isReplace);

    void qtOpenInBrowser(QString url);

    void setUserName(QString userName);

    void registerWebChannel(const QString &name, QObject *obj);

    void unregisterAllWebChannels();

    void registerAllWebChannels();

public slots:

    void updateHtmlsEvent();

    void updateAppEvent(const QString appVersion, const QString reference, const QString message);

private slots:

    void onCallbackCall(SimpleClient::ReturnCallback callback);

    void onShowContextMenu(const QPoint &point);

    void onJsRun(QString jsString);

    void onSetCommandLineText(QString text);

    void onSetMappings(QString mapping);

    void onEnterCommandAndAddToHistory(const QString &text);

    void onEnterCommandAndAddToHistoryNoDuplicate(const QString &text);

    void onUrlChanged(const QUrl &url2);

    void onLogined(bool isInit, const QString &login);

private:

    void correctWindowSize(int iteration);

private:
    std::unique_ptr<Ui::MainWindow> ui;

    QSystemTrayIcon *systemTray;
    QMenu *trayMenu;
    QAction *hideAction;
    QAction *showAction;

    MHUrlSchemeHandler *shemeHandler = nullptr;

    MHPayUrlSchemeHandler *shemeHandler2 = nullptr;

    std::unique_ptr<QWebChannel> channel;

    metagate::MetaGate *metagate = nullptr;

    LastHtmlVersion last_htmls;
    mutable std::mutex mutLastHtmls;

    QString currentTextCommandLine;

    PagesMappings pagesMappings;

    QString hardwareId;

    size_t countFocusLineEditChanged = 0;

    std::vector<QString> history;
    size_t historyPos = 0;

    SimpleClient client;

    QString prevUrl;
    bool prevIsApp = true;

    QString prevTextCommandLine;

    QString currentUserName;

    QString urlDns;

    QString netDns;

    bool lineEditUserChanged = false;

    bool isInitFinished = false;

    QUrl saveUrlToMove;

    std::vector<std::pair<QString, QObject*>> registeredWebChannels;
    //bool isRegisteredWebChannels = true;
};

#endif // MAINWINDOW_H
