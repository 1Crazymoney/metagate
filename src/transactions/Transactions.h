#ifndef TRANSACTIONS_H
#define TRANSACTIONS_H

#include <QObject>
#include <QString>

#include <functional>
#include <vector>
#include <map>
#include <set>

#include "Network/SimpleClient.h"
#include "Network/HttpClient.h"
#include "qt_utilites/TimerClass.h"

#include "qt_utilites/CallbackWrapper.h"
#include "qt_utilites/ManagerWrapper.h"

#include "Transaction.h"
#include "TransactionsFilter.h"

class NsLookup;
class InfrastructureNsLookup;

struct TypedException;

class MainWindow;

namespace wallets {
class Wallets;
}

namespace auth {
class Auth;
}

namespace transactions {

class TransactionsJavascript;
class TransactionsDBStorage;
enum class DelegateStatus;

class Transactions : public ManagerWrapper, public TimerClass {
    Q_OBJECT
public:
    using IdBalancePair = std::pair<QString, BalanceInfo>;

private:
    using TransactionHash = std::string;

    class SendedTransactionWatcher {
    public:

        SendedTransactionWatcher(const SendedTransactionWatcher &) = delete;
        SendedTransactionWatcher(SendedTransactionWatcher &&) = delete;
        SendedTransactionWatcher& operator=(const SendedTransactionWatcher &) = delete;
        SendedTransactionWatcher& operator=(SendedTransactionWatcher &&) = delete;

        SendedTransactionWatcher(Transactions &txManager, const QString &requestId, const TransactionHash &hash, const time_point &startTime, const std::vector<QString> &servers, const seconds &timeout)
            : requestId(requestId)
            , startTime(startTime)
            , timeout(timeout)
            , txManager(txManager)
            , hash(hash)
            , servers(servers.begin(), servers.end())
            , allServers(servers.begin(), servers.end())
        {}

        ~SendedTransactionWatcher();

        std::set<QString> getServersCopy() const {
            return servers;
        }

        bool isEmpty() const {
            return allServers.empty();
        }

        bool isTimeout(const time_point &now) const {
            return now - startTime >= timeout;
        }

        void removeServer(const QString &server) {
            servers.erase(server);
        }

        void returnServer(const QString &server) {
            if (allServers.find(server) != allServers.end()) {
                servers.insert(server);
            }
        }

        void okServer(const QString &server) {
            if (allServers.find(server) != allServers.end()) {
                servers.erase(server);
                allServers.erase(server);
            }
        }

        void setError(const QString &server, const QString &error) {
            errors[server] = error;
        }

    public:

        const QString requestId;

    private:
        const time_point startTime;
        const seconds timeout;

        Transactions &txManager;

        TransactionHash hash;

        std::set<QString> servers;
        std::set<QString> allServers;
        std::map<QString, QString> errors;
    };

    struct ServersStruct {
        int countRequests = 0;
        QString currency;

        ServersStruct(const QString &currency)
            : currency(currency)
        {}
    };

public:

    using RegisterAddressCallback = CallbackWrapper<void()>;

    using GetTxsCallback = CallbackWrapper<void(const std::vector<Transaction> &txs)>;

    using CalcBalanceCallback = CallbackWrapper<void(const BalanceInfo &txs)>;

    using SetCurrentGroupCallback = CallbackWrapper<void()>;

    using GetAddressesCallback = CallbackWrapper<void(const std::vector<AddressInfo> &result)>;

    using GetTxCallback = CallbackWrapper<void(const Transaction &txs)>;

    using GetLastUpdateCallback = CallbackWrapper<void(const system_time_point &lastUpdate, const system_time_point &now)>;

    using GetNonceCallback = CallbackWrapper<void(size_t nonce, const QString &serverError)>;

    using SendTransactionCallback = CallbackWrapper<void()>;

    using GetTokensCallback = CallbackWrapper<void(const std::vector<TokenInfo>& tokens)>;

    using ClearDbCallback = CallbackWrapper<void()>;

    using AddCurrencyConformity = CallbackWrapper<void()>;

public:

    explicit Transactions(NsLookup &nsLookup, InfrastructureNsLookup &infrastructureNsLookup, TransactionsJavascript &javascriptWrapper, TransactionsDBStorage &db, auth::Auth &authManager, MainWindow &mainWin, wallets::Wallets &wallets, QObject *parent = nullptr);

    ~Transactions() override;

protected:

    void startMethod() override;

    void timerMethod() override;

    void finishMethod() override;

signals:
    void showNotification(const QString &title, const QString &message);

    void getBalancesFromTorrentResult(const QString &id, bool res, const QString &descr, const std::vector<IdBalancePair> &result);

signals:

    void registerAddresses(const std::vector<AddressInfo> &addresses, const RegisterAddressCallback &callback);

    void getAddresses(const QString &group, const GetAddressesCallback &callback);

    void setCurrentGroup(const QString &group, const SetCurrentGroupCallback &callback);

    void getTxs2(const QString &address, const QString &currency, int from, int count, bool asc, const GetTxsCallback &callback);

    void getTxsFilters(const QString &address, const QString &currency, const Filters &filter, int from, int count, bool asc, const GetTxsCallback &callback);

    void getTxsAll2(const QString &currency, int from, int count, bool asc, const GetTxsCallback &callback);

    void getForgingTxs(const QString &address, const QString &currency, int from, int count, bool asc, const GetTxsCallback &callback);

    void getDelegateTxs(const QString &address, const QString &currency, const QString &to, int from, int count, bool asc, const GetTxsCallback &callback);

    void getDelegateTxs2(const QString &address, const QString &currency, int from, int count, bool asc, const GetTxsCallback &callback);

    void getLastForgingTx(const QString &address, const QString &currency, const GetTxCallback &callback);

    void calcBalance(const QString &address, const QString &currency, const CalcBalanceCallback &callback);

    void getNonce(const QString &from, const SendParameters &sendParams, const GetNonceCallback &callback);

    void sendTransaction(const QString &requestId, const QString &to, const QString &value, size_t nonce, const QString &data, const QString &fee, const QString &pubkey, const QString &sign, const SendParameters &sendParams, const SendTransactionCallback &callback);

    void getTxFromServer(const QString &txHash, const QString &type, const GetTxCallback &callback);

    void getLastUpdateBalance(const QString& currency, const GetLastUpdateCallback& callback);

    void getTokensAddress(const QString& address, const GetTokensCallback& callback);

    void clearDb(const QString& currency, const ClearDbCallback& callback);

    void addCurrencyConformity(bool isMhc, const QString &currency, const AddCurrencyConformity &callback);

    void getBalancesFromTorrent(const QString &id, const QUrl &url, const std::vector<std::pair<QString, QString>> &addresses);

public slots:

    void onRegisterAddresses(const std::vector<AddressInfo> &addresses, const RegisterAddressCallback &callback);

    void onGetAddresses(const QString &group, const GetAddressesCallback &callback);

    void onSetCurrentGroup(const QString &group, const SetCurrentGroupCallback &callback);

    void onGetTxs2(const QString &address, const QString &currency, int from, int count, bool asc, const GetTxsCallback &callback);

    void onGetTxsFilters(const QString &address, const QString &currency, const Filters &filter, int from, int count, bool asc, const GetTxsCallback &callback);

    void onGetTxsAll2(const QString &currency, int from, int count, bool asc, const GetTxsCallback &callback);

    void onGetForgingTxs(const QString &address, const QString &currency, int from, int count, bool asc, const GetTxsCallback &callback);

    void onGetDelegateTxs(const QString &address, const QString &currency, const QString &to, int from, int count, bool asc, const GetTxsCallback &callback);

    void onGetDelegateTxs2(const QString &address, const QString &currency, int from, int count, bool asc, const GetTxsCallback &callback);

    void onGetLastForgingTx(const QString &address, const QString &currency, const GetTxCallback &callback);

    void onCalcBalance(const QString &address, const QString &currency, const CalcBalanceCallback &callback);

    void onGetNonce(const QString &from, const SendParameters &sendParams, const GetNonceCallback &callback);

    void onSendTransaction(const QString &requestId, const QString &to, const QString &value, size_t nonce, const QString &data, const QString &fee, const QString &pubkey, const QString &sign, const SendParameters &sendParams, const SendTransactionCallback &callback);

    void onGetTxFromServer(const QString &txHash, const QString &type, const GetTxCallback &callback);

    void onGetLastUpdateBalance(const QString& currency, const GetLastUpdateCallback& callback);

    void onGetTokensAddress(const QString& address, const GetTokensCallback& callback);

    void onClearDb(const QString& currency, const ClearDbCallback& callback);

    void onMthWalletCreated(bool isMhc, const QString &name, const QString &userName);

    void onMthWatchWalletsAdded(bool isMhc, const std::vector<std::pair<QString, QString>> &created, const QString &username);

    void onAddCurrencyConformity(bool isMhc, const QString &currency, const AddCurrencyConformity &callback);

    void onGetBalancesFromTorrent(const QString &id, const QUrl &url, const std::vector<std::pair<QString, QString>> &addresses);

private slots:

    void onFindTxOnTorrentEvent();

    void onLogined(bool isInit, const QString login);

private:

    void processCheckTxs(const QString &address, const QString &currency, const std::vector<QString> &servers);

    void processCheckTxsOneServer(const QString &address, const QString &currency, const QUrl &server);

    void processCheckTxsInternal(const QString &address, const QString &currency, const QUrl &server, const Transaction &tx, int64_t serverBlockNumber);

    void processAddressMth(const std::vector<QString>& addresses,
        const QString& currency,
        const std::vector<QString>& servers,
        const std::shared_ptr<ServersStruct>& servStruct);

    void processTokens(const QString& address,
        const QUrl& server,
        const BalanceInfo& serverBalance,
        const BalanceInfo& dbBalance);

    void updateTokenInfo(const QString& tokenAddress,
        const QUrl& server);

    void processPendings();

    void processPendings(const QString &address, const QString &currency, const std::vector<Transaction> &txsPending, const std::vector<QString> &serversContract, const std::vector<QString> &serversSimple);

    uint64_t calcCountTxs(const QString &address, const QString &currency) const;

    void newBalance(const QString &address, const QString &currency, uint64_t savedCountTxs, uint64_t confirmedCountTxsInThisLoop, const BalanceInfo &balance, const BalanceInfo &curBalance, const std::vector<Transaction> &txs, const std::shared_ptr<ServersStruct> &servStruct);

    void updateBalanceTime(const QString &currency, const std::shared_ptr<ServersStruct> &servStruct);

    std::vector<AddressInfo> getAddressesInfos(const QString &group);

    BalanceInfo getBalance(const QString &address, const QString &currency);

    void addToSendTxWatcher(const QString &requestId, const TransactionHash &hash, size_t countServers, const std::vector<QString> &servers, const seconds &timeout);

    void sendErrorGetTx(const QString &requestId, const TransactionHash &hash, const QString &server);

    void fetchBalanceAddress(const QString &address);

    void removeAddress(const QString &address, const QString &currency);

    void addTrackedForCurrentLogin();

    QString convertCurrency(const QString &currency) const;

private:

    NsLookup &nsLookup;

    InfrastructureNsLookup &infrastructureNsLookup;

    wallets::Wallets &wallets;

    TransactionsJavascript &javascriptWrapper;

    TransactionsDBStorage &db;

    SimpleClient client;

    HttpSimpleClient tcpClient;

    QString currentUserName;

    bool isUserNameSetted = false;

    QTimer timerSendTx;

    std::map<TransactionHash, SendedTransactionWatcher> sendTxWathcers;

    std::map<QString, system_time_point> lastSuccessUpdateTimestamps;

    std::vector<std::pair<QString, std::set<QString>>> pendingTxsAfterSend;

    seconds timeout;

    time_point lastCheckTxsTime;

    std::vector<AddressInfo> addressesInfos;

    size_t posInAddressInfos;
};

SendParameters parseSendParams(const QString &paramsJson);

}

#endif // TRANSACTIONS_H
