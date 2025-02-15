#include "Transactions.h"

#include <functional>
using namespace std::placeholders;

#include <QSettings>

#include "check.h"
#include "qt_utilites/SlotWrapper.h"
#include "Paths.h"
#include "qt_utilites/QRegister.h"

#include "MainWindow.h"
#include "auth/Auth.h"
#include "Wallets/Wallet.h"
#include "NsLookup/NsLookup.h"
#include "NsLookup/InfrastructureNsLookup.h"

#include "qt_utilites/ManagerWrapperImpl.h"

#include "TransactionsMessages.h"
#include "TransactionsJavascript.h"
#include "TransactionsDBStorage.h"

#include "Wallets/Wallets.h"
#include "Wallets/WalletInfo.h"

#include <memory>

SET_LOG_NAMESPACE("TXS");

namespace transactions {

static const uint64_t ADD_TO_COUNT_TXS = 10;
static const uint64_t MAX_TXS_IN_RESPONSE = 2000;

static QString makeGroupName(const QString &userName) {
    if (userName.isEmpty()) {
        return "_unregistered";
    }
    return userName;
}

// Small hack to convert different currencies names
QString Transactions::convertCurrency(const QString &currency) const {
    return currency.toLower();
}

Transactions::Transactions(NsLookup &nsLookup, InfrastructureNsLookup &infrastructureNsLookup, TransactionsJavascript &javascriptWrapper, TransactionsDBStorage &db, auth::Auth &authManager, MainWindow &mainWin, wallets::Wallets &wallets, QObject *parent)
    : TimerClass(5s, parent)
    , nsLookup(nsLookup)
    , infrastructureNsLookup(infrastructureNsLookup)
    , wallets(wallets)
    , javascriptWrapper(javascriptWrapper)
    , db(db)
{
    wallets.setTransactions(this);

    Q_CONNECT(this, &Transactions::showNotification, &mainWin, &MainWindow::showNotification);
    Q_CONNECT(&authManager, &auth::Auth::logined, this, &Transactions::onLogined);
    Q_CONNECT(this, &Transactions::registerAddresses, this, &Transactions::onRegisterAddresses);
    Q_CONNECT(this, &Transactions::getAddresses, this, &Transactions::onGetAddresses);
    Q_CONNECT(this, &Transactions::setCurrentGroup, this, &Transactions::onSetCurrentGroup);
    Q_CONNECT(this, &Transactions::getTxs2, this, &Transactions::onGetTxs2);
    Q_CONNECT(this, &Transactions::getTxsFilters, this, &Transactions::onGetTxsFilters);
    Q_CONNECT(this, &Transactions::getTxsAll2, this, &Transactions::onGetTxsAll2);
    Q_CONNECT(this, &Transactions::getForgingTxs, this, &Transactions::onGetForgingTxs);
    Q_CONNECT(this, &Transactions::getDelegateTxs, this, &Transactions::onGetDelegateTxs);
    Q_CONNECT(this, &Transactions::getDelegateTxs2, this, &Transactions::onGetDelegateTxs2);
    Q_CONNECT(this, &Transactions::getLastForgingTx, this, &Transactions::onGetLastForgingTx);
    Q_CONNECT(this, &Transactions::calcBalance, this, &Transactions::onCalcBalance);
    Q_CONNECT(this, &Transactions::sendTransaction, this, &Transactions::onSendTransaction);
    Q_CONNECT(this, &Transactions::getTxFromServer, this, &Transactions::onGetTxFromServer);
    Q_CONNECT(this, &Transactions::getLastUpdateBalance, this, &Transactions::onGetLastUpdateBalance);
    Q_CONNECT(this, &Transactions::getNonce, this, &Transactions::onGetNonce);
    Q_CONNECT(this, &Transactions::getTokensAddress, this, &Transactions::onGetTokensAddress);
    Q_CONNECT(this, &Transactions::clearDb, this, &Transactions::onClearDb);
    Q_CONNECT(this, &Transactions::addCurrencyConformity, this, &Transactions::onAddCurrencyConformity);
    Q_CONNECT(this, &Transactions::getBalancesFromTorrent, this, &Transactions::onGetBalancesFromTorrent);

    Q_CONNECT(&wallets, &wallets::Wallets::mhcWalletCreated, this, &Transactions::onMthWalletCreated);
    Q_CONNECT(&wallets, &wallets::Wallets::mhcWatchWalletCreated, this, &Transactions::onMthWalletCreated);
    Q_CONNECT(&wallets, &wallets::Wallets::watchWalletsAdded, this, &Transactions::onMthWatchWalletsAdded);

    Q_REG(RegisterAddressCallback, "RegisterAddressCallback");
    Q_REG(GetTxsCallback, "GetTxsCallback");
    Q_REG(CalcBalanceCallback, "CalcBalanceCallback");
    Q_REG(SetCurrentGroupCallback, "SetCurrentGroupCallback");
    Q_REG(GetAddressesCallback, "GetAddressesCallback");
    Q_REG(GetTxCallback, "GetTxCallback");
    Q_REG(GetLastUpdateCallback, "GetLastUpdateCallback");
    Q_REG(GetNonceCallback, "GetNonceCallback");
    Q_REG(SendTransactionCallback, "SendTransactionCallback");
    Q_REG(GetTokensCallback, "GetTokensCallback");
    Q_REG(ClearDbCallback, "ClearDbCallback");
    Q_REG(AddCurrencyConformity, "AddCurrencyConformity");

    Q_REG2(size_t, "size_t", false);
    Q_REG2(seconds, "seconds", false);
    Q_REG(DelegateStatus, "DelegateStatus");
    Q_REG(SendParameters, "SendParameters");
    Q_REG(Filters, "Filters");

    Q_REG(std::vector<AddressInfo>, "std::vector<AddressInfo>");
    Q_REG(std::vector<IdBalancePair>, "std::vector<IdBalancePair>");

    QSettings settings(getSettingsPath(), QSettings::IniFormat);
    CHECK(settings.contains("timeouts_sec/transactions"), "settings timeout not found");
    timeout = seconds(settings.value("timeouts_sec/transactions").toInt());

    client.setParent(this);
    Q_CONNECT(&client, &SimpleClient::callbackCall, this, &Transactions::callbackCall);
    client.moveToThread(TimerClass::getThread());

    Q_CONNECT(&tcpClient, &HttpSimpleClient::callbackCall, this, &Transactions::callbackCall);
    tcpClient.moveToThread(TimerClass::getThread());

    timerSendTx.moveToThread(TimerClass::getThread());
    timerSendTx.setInterval(milliseconds(100).count());
    Q_CONNECT(&timerSendTx, &QTimer::timeout, this, &Transactions::onFindTxOnTorrentEvent);

    const int size = settings.beginReadArray("transactions_currency");
    for (int i = 0; i < size; i++) {
        settings.setArrayIndex(i);
        const bool isMhc = settings.value("isMhc").toBool();
        const QString currency = settings.value("currency").toString();

        db.addToCurrency(isMhc, currency);
    }
    settings.endArray();

    javascriptWrapper.setTransactions(*this);

    moveToThread(TimerClass::getThread()); // TODO вызывать в TimerClass
}

Transactions::~Transactions() {
    TimerClass::exit();
}

void Transactions::startMethod() {
    // empty
}

void Transactions::finishMethod() {
    emit timerSendTx.stop();
}

uint64_t Transactions::calcCountTxs(const QString &address, const QString &currency) const {
    return static_cast<uint64_t>(db.getPaymentsCountForAddress(address, currency));
}

void Transactions::newBalance(const QString &address, const QString &currency, uint64_t savedCountTxs, uint64_t confirmedCountTxsInThisLoop, const BalanceInfo &balance, const BalanceInfo &curBalance, const std::vector<Transaction> &txs, const std::shared_ptr<ServersStruct> &servStruct) {
    const uint64_t currCountTxs = calcCountTxs(address, currency);
    CHECK(savedCountTxs == currCountTxs, "Trancastions in db on address " + address.toStdString() + " " + currency.toStdString() + " changed");
    auto transactionGuard = db.beginTransaction();
    for (const Transaction &tx: txs) {
        db.addPayment(tx);
    }
    db.setBalance(currency, address, balance);
    transactionGuard.commit();

    BalanceInfo balanceCopy = balance;
    balanceCopy.savedTxs = std::min(confirmedCountTxsInThisLoop, balance.countTxs);
    if (balanceCopy.savedTxs == balance.countTxs) {
        BigNumber sumch = (balance.received - balance.spent) - (curBalance.received - curBalance.spent);
        QString value = sumch.getFracDecimal(BNModule);
        // remove '-' if present
        if (value.front() == QChar('-'))
            value = value.right(value.size() - 1);
        if (sumch.isNegative())
            value.prepend(tr("Out") + QStringLiteral(" "));
        else if (!sumch.isZero())
            value.prepend(tr("In") + QStringLiteral(" "));
        value += QStringLiteral(" ") + currency.toUpper();
        emit showNotification(tr("Balance for address %1 changed").arg(address), value);
    }
    emit javascriptWrapper.newBalanceSig(address, currency, balanceCopy);
    updateBalanceTime(currency, servStruct);
}

void Transactions::updateBalanceTime(const QString &currency, const std::shared_ptr<ServersStruct> &servStruct) {
    if (servStruct == nullptr) {
        return;
    }
    CHECK(servStruct->currency == currency, "Incorrect servStruct currency");
    servStruct->countRequests--;
    if (servStruct->countRequests == 0) {
        const system_time_point now = ::system_now();
        lastSuccessUpdateTimestamps[currency] = now;
    }
}

void Transactions::processPendings() {
    const auto processPendingTx = [this](const SimpleClient::Response &response) {
        CHECK(!response.exception.isSet(), "Server error: " + response.exception.toString());
        const Transaction tx = parseGetTxResponse(QString::fromStdString(response.response), "", "");
        if (tx.status != Transaction::PENDING) {
            const auto foundIter = std::remove_if(pendingTxsAfterSend.begin(), pendingTxsAfterSend.end(), [txHash=tx.tx](const auto &pair){
                return pair.first == txHash;
            });
            const bool found = foundIter != pendingTxsAfterSend.end();
            pendingTxsAfterSend.erase(foundIter, pendingTxsAfterSend.end());
            if (found) {
                emit javascriptWrapper.transactionStatusChanged2Sig(tx.tx, tx);
            }
        }
    };

    LOG << PeriodicLog::make("pas") << "Pending after send: " << pendingTxsAfterSend.size();

    const auto copyPending = pendingTxsAfterSend;
    for (const auto &pair: copyPending) {
        const QString message = makeGetTxRequest(pair.first);
        for (const QString &server: pair.second) {
            client.sendMessagePost(server, message, processPendingTx, timeout);
        }
    }
}

void Transactions::processPendings(const QString &address, const QString &currency, const std::vector<Transaction> &txsPending, const std::vector<QString> &serversContract, const std::vector<QString> &serversSimple) {
    const auto processPendingTx = [this, currency, address](const SimpleClient::Response &response) {
        CHECK(!response.exception.isSet(), "Server error: " + response.exception.toString());
        const Transaction tx = parseGetTxResponse(QString::fromStdString(response.response), address, currency);
        if (tx.status != Transaction::PENDING) {
            db.updatePayment(address, currency, tx.tx, tx.blockNumber, tx.blockIndex, tx);
            emit javascriptWrapper.transactionStatusChangedSig(address, currency, tx.tx, tx);
            emit javascriptWrapper.transactionStatusChanged2Sig(tx.tx, tx);
        }
    };

    if (!txsPending.empty()) {
        LOG << PeriodicLog::make("pt_" + address.right(4).toStdString()) << "Pending txs: " << txsPending.size();
    }

    for (const Transaction &tx: txsPending) {
        std::vector<QString> servers;
        if (tx.type == Transaction::Type::CONTRACT) {
            servers = serversContract;
        } else {
            if (tx.status == Transaction::Status::MODULE_NOT_SET) {
                continue;
            }
            servers = serversSimple;
        }

        if (servers.empty()) {
            continue;
        }
        const QString message = makeGetTxRequest(tx.tx);
        client.sendMessagePost(servers[0], message, std::bind(processPendingTx, _1), timeout);
    }
}

void Transactions::processAddressMth(const std::vector<QString> &addresses, const QString &currency, const std::vector<QString> &servers, const std::shared_ptr<ServersStruct> &servStruct) {
    if (servers.empty()) {
        return;
    }
    if (addresses.empty()) {
        return;
    }

    const auto getBlockHeaderCallback = [this, currency, servStruct](const QString &address, const BalanceInfo &balance, uint64_t savedCountTxs, uint64_t confirmedCountTxsInThisLoop, std::vector<Transaction> txs, const SimpleClient::Response &response) {
        CHECK(!response.exception.isSet(), "Server error: " + response.exception.toString());
        const BlockInfo bi = parseGetBlockInfoResponse(QString::fromStdString(response.response));
        for (Transaction &tx: txs) {
            if (tx.blockNumber == bi.number) {
                tx.blockHash = bi.hash;
            }
        }
        const BalanceInfo confirmedBalance = db.getBalance(currency, address);
        newBalance(address, currency, savedCountTxs, confirmedCountTxsInThisLoop, balance, confirmedBalance, txs, servStruct);
    };

    const auto processNewTransactions = [this, currency, getBlockHeaderCallback](const QString &address, const BalanceInfo &balance, uint64_t savedCountTxs, uint64_t confirmedCountTxsInThisLoop, const std::vector<Transaction> &txs, const QUrl &server) {
        const auto maxElement = std::max_element(txs.begin(), txs.end(), [](const Transaction &first, const Transaction &second) {
            return first.blockNumber < second.blockNumber;
        });
        CHECK(maxElement != txs.end(), "Incorrect max element");
        const int64_t blockNumber = maxElement->blockNumber;
        const QString request = makeGetBlockInfoRequest(blockNumber);
        client.sendMessagePost(server, request, std::bind(getBlockHeaderCallback, address, balance, savedCountTxs, confirmedCountTxsInThisLoop, txs, _1), timeout);
    };

    const auto getBalanceConfirmeCallback = [currency, processNewTransactions](const QString &address, const BalanceInfo &serverBalance, uint64_t savedCountTxs, uint64_t confirmedCountTxsInThisLoop, const std::vector<Transaction> &txs, const QUrl &server, const SimpleClient::Response &response) {
        CHECK(!response.exception.isSet(), "Server error: " + response.exception.toString());
        const BalanceInfo balance = parseBalanceResponse(QString::fromStdString(response.response));
        const uint64_t countInServer = balance.countTxs;
        const uint64_t countSave = serverBalance.countTxs;
        if (countInServer - countSave <= ADD_TO_COUNT_TXS) {
            LOG << "Balance " << address << " confirmed";
            processNewTransactions(address, serverBalance, savedCountTxs, confirmedCountTxsInThisLoop, txs, server);
        }
    };

    const auto getHistoryCallback = [this, currency, getBalanceConfirmeCallback](const QString &address, const BalanceInfo &serverBalance, uint64_t savedCountTxs, uint64_t confirmedCountTxsInThisLoop, const QUrl &server, const SimpleClient::Response &response) {
        CHECK(!response.exception.isSet(), "Server error: " + response.exception.toString());
        const std::vector<Transaction> txs = parseHistoryResponse(address, currency, QString::fromStdString(response.response));

        LOG << "geted with duplicates " << address << " " << txs.size();

        const QString requestBalance = makeGetBalanceRequest(address);

        client.sendMessagePost(server, requestBalance, std::bind(getBalanceConfirmeCallback, address, serverBalance, savedCountTxs, confirmedCountTxsInThisLoop, txs, server, _1), timeout);
    };

    const auto getBalanceCallback = [this, servStruct, addresses, currency, getHistoryCallback](const std::vector<QUrl> &servers, const std::vector<SimpleClient::Response> &responses) {
        CHECK(!servers.empty(), "Incorrect response size");
        CHECK(servers.size() == responses.size(), "Incorrect response size");

        std::vector<std::pair<QUrl, BalanceInfo>> bestAnswers(addresses.size());
        for (size_t i = 0; i < responses.size(); i++) {
            const auto &r = responses[i];
            const auto &exception = r.exception;
            const std::string &response = r.response;
            const QUrl &server = servers[i];
            if (!exception.isSet()) {
                const std::vector<BalanceInfo> balancesResponse = parseBalancesResponse(QString::fromStdString(response));
                CHECK(balancesResponse.size() == addresses.size(), "Incorrect balances response");
                for (size_t j = 0; j < balancesResponse.size(); j++) {
                    const BalanceInfo &balanceResponse = balancesResponse[j];
                    const QString &address = addresses[j];
                    CHECK(balanceResponse.address == address, "Incorrect response: address not equal. Expected " + address.toStdString() + ". Received " + balanceResponse.address.toStdString());
                    if (balanceResponse.currBlockNum > bestAnswers[j].second.currBlockNum) {
                        bestAnswers[j].second = balanceResponse;
                        bestAnswers[j].first = server;
                    }
                }
            } else {
                if (exception.isTimeout()) {
                    emit nsLookup.rejectServer(server.toString());
                }
            }
        }

        CHECK(bestAnswers.size() == addresses.size(), "Ups");
        for (size_t i = 0; i < bestAnswers.size(); i++) {
            const QUrl &bestServer = bestAnswers[i].first;
            const QString &address = addresses[i];
            const BalanceInfo &serverBalance = bestAnswers[i].second;
            const BalanceInfo confirmedBalance = db.getBalance(currency, address);

            if (bestServer.isEmpty()) {
                LOG << PeriodicLog::makeAuto("t_err") << "Best server with txs not found. Error: " << responses[0].exception.toString();
                return;
            }

            processTokens(address, bestServer, serverBalance, confirmedBalance);

            const uint64_t countAll = calcCountTxs(address, currency);
            const uint64_t countInServer = serverBalance.countTxs;
            LOG << PeriodicLog::make(std::string("t_") + currency[0].toLatin1() + "," + address.right(4).toStdString()) << "Automatic get txs " << address << " " << currency << " " << countAll << " " << countInServer;
            if (countAll < countInServer) {
                processCheckTxsOneServer(address, currency, bestServer);

                const uint64_t countMissingTxs = countInServer - countAll;
                const uint64_t beginTx = countMissingTxs >= MAX_TXS_IN_RESPONSE ? countMissingTxs - MAX_TXS_IN_RESPONSE : 0;
                const uint64_t requestCountTxs = std::min(countMissingTxs, MAX_TXS_IN_RESPONSE) + ADD_TO_COUNT_TXS;
                const QString requestForTxs = makeGetHistoryRequest(address, true, beginTx, requestCountTxs);

                client.sendMessagePost(bestServer, requestForTxs, std::bind(getHistoryCallback, address, serverBalance, countAll, requestCountTxs + countAll, bestServer, _1), timeout);
            } else if (countAll > countInServer) {
                //removeAddress(address, currency); Не удаляем, так как при перевыкачки базы начнется свистопляска
            } else {
                // TODO remove
                // const BalanceInfo confirmedBalance = db.getBalance(currency,
                // address);
                if (confirmedBalance.countTxs != countInServer) {
                    newBalance(address,
                        currency,
                        countAll,
                        serverBalance.countTxs,
                        serverBalance,
                        confirmedBalance,
                        {},
                        servStruct);
                } else {
                    updateBalanceTime(currency, servStruct);
                }
            }
        }
    };

    const QString requestBalance = makeGetBalancesRequest(addresses);
    const std::vector<QUrl> urls(servers.begin(), servers.end());
    client.sendMessagesPost(addresses[0].toStdString(), urls, requestBalance, std::bind(getBalanceCallback, urls, _1), timeout);
}

void Transactions::processTokens(const QString& address,
    const QUrl& server,
    const BalanceInfo& serverBalance,
    const BalanceInfo& dbBalance)
{
    LOG << "Transactions::processTokens" << serverBalance.tokenBlockNum << dbBalance.tokenBlockNum;
    const auto getHistoryCallback = [this](const QString& address, const QUrl& server, const SimpleClient::Response& response) {
        CHECK(!response.exception.isSet(), "Server error: " + response.exception.toString());
        std::vector<TokenBalance> tokens = parseAddressGetTokensResponse(address, QString::fromStdString(response.response));
        //qDebug() << "Tk: " << QString::fromStdString(response.response);
        for (const auto& tokenBalance : tokens) {
            LOG << "TOKEN: " << tokenBalance.tokenAddress;
            updateTokenInfo(tokenBalance.tokenAddress, server);
            db.updateTokenBalance(tokenBalance);
        }
    };

    if (serverBalance.tokenBlockNum > dbBalance.tokenBlockNum) {
        LOG << "Request tokens: " << address;
        const QString requestTokens = makeAddressGetTokensRequest(address);
        client.sendMessagePost(server, requestTokens, std::bind(getHistoryCallback, address, server, _1), timeout);
    }
}

void Transactions::updateTokenInfo(const QString& tokenAddress, const QUrl& server)
{
    LOG << "Transactions::updateTokenInfo" << tokenAddress;
    const auto getHistoryCallback = [this](const QString& tokenAddress, const SimpleClient::Response& response) {
        CHECK(!response.exception.isSet(), "Server error: " + response.exception.toString());
        Token info = parseTokenGetInfoResponse(tokenAddress, QString::fromStdString(response.response));
        //qDebug() << QString::fromStdString(response.response);
        LOG << "Token: " << info.name << info.type;
        db.updateToken(info);
    };

    const QString requestTokensInfo = makeTokenGetInfoRequest(tokenAddress);
    //qDebug() << requestTokensInfo;
    client.sendMessagePost(server, requestTokensInfo, std::bind(getHistoryCallback, tokenAddress, _1), timeout);
}

std::vector<AddressInfo> Transactions::getAddressesInfos(const QString &group) {
    return db.getTrackedForGroup(group);
}

BalanceInfo Transactions::getBalance(const QString &address, const QString &currency) {
    BalanceInfo balance = db.getBalance(currency, address);

    return balance;
}

void Transactions::processCheckTxsOneServer(const QString &address, const QString &currency, const QUrl &server) {
    const Transaction lastTx = db.getLastTransaction(address, currency);
    if (lastTx.blockHash.isEmpty()) {
        return;
    }

    const auto countBlocksCallback = [this, address, currency, lastTx](const QUrl &server, const SimpleClient::Response &response) {
        CHECK(!response.exception.isSet(), "Server exception: " + response.exception.toString());

        const int64_t blockNumber = parseGetCountBlocksResponse(QString::fromStdString(response.response));

        processCheckTxsInternal(address, currency, server, lastTx, blockNumber);
    };

    const QString countBlocksRequest = makeGetCountBlocksRequest();
    client.sendMessagePost(server, countBlocksRequest, std::bind(countBlocksCallback, server, _1), timeout);
}

void Transactions::removeAddress(const QString &address, const QString &currency) {
    LOG << "Remove txs " << address << " " << currency;
    db.removePaymentsForDest(address, currency);
    db.removeBalance(currency, address);
}

void Transactions::processCheckTxsInternal(const QString &address, const QString &currency, const QUrl &server, const Transaction &tx, int64_t serverBlockNumber) {
    const auto getBlockInfoCallback = [address, currency, this] (const QString &hash, const SimpleClient::Response &response) {
        CHECK(!response.exception.isSet(), "Server error: " + response.exception.toString());
        const BlockInfo bi = parseGetBlockInfoResponse(QString::fromStdString(response.response));
        if (bi.hash != hash) {
            removeAddress(address, currency);
        }
    };

    if (tx.blockNumber > serverBlockNumber) {
        removeAddress(address, currency);
        return;
    }

    const QString blockInfoRequest = makeGetBlockInfoRequest(tx.blockNumber);
    client.sendMessagePost(server, blockInfoRequest, std::bind(getBlockInfoCallback, tx.blockHash, _1));
}

void Transactions::processCheckTxs(const QString &address, const QString &currency, const std::vector<QString> &servers) {
    if (servers.empty()) {
        return;
    }
    const Transaction lastTx = db.getLastTransaction(address, currency);
    if (lastTx.blockHash.isEmpty()) {
        return;
    }

    const auto countBlocksCallback = [this, address, currency, lastTx](const std::vector<QUrl> &urls, const std::vector<SimpleClient::Response> &responses) {
        CHECK(!urls.empty(), "Incorrect response size");
        CHECK(urls.size() == responses.size(), "Incorrect responses size");

        int64_t maxBlockNumber = 0;
        QUrl maxServer;
        for (size_t i = 0; i < responses.size(); i++) {
            const SimpleClient::ServerException &exception = responses[i].exception;
            if (!exception.isSet()) {
                const QUrl &server = urls[i];
                const std::string &response = responses[i].response;
                const int64_t blockNumber = parseGetCountBlocksResponse(QString::fromStdString(response));
                if (blockNumber > maxBlockNumber) {
                    maxBlockNumber = blockNumber;
                    maxServer = server;
                }
            }
        }
        if (maxServer.isEmpty()) {
            LOG << PeriodicLog::makeAuto("t_err2") << "Best server with txs not found. Error: " << responses[0].exception.toString();
        } else {
            processCheckTxsInternal(address, currency, maxServer, lastTx, maxBlockNumber);
        }
    };

    const QString countBlocksRequest = makeGetCountBlocksRequest();
    const std::vector<QUrl> urls(servers.begin(), servers.end());
    client.sendMessagesPost(address.toStdString(), urls, countBlocksRequest, std::bind(countBlocksCallback, urls, _1), timeout);
}

void Transactions::timerMethod() {
    static const size_t COUNT_PARALLEL_REQUESTS = 15;
    static const size_t MAXIMUM_ADDRESSES_IN_BATCH = 20;

    const auto CHECK_TXS_PERIOD = 3min;


    if (posInAddressInfos >= addressesInfos.size()) {
        if (!addressesInfos.empty()) {
            LOG << "All txs getted";
        }
        addressesInfos.clear();
    }

    if (addressesInfos.empty()) {
        addressesInfos = getAddressesInfos(makeGroupName(currentUserName));
        std::sort(addressesInfos.begin(), addressesInfos.end(), [](const AddressInfo &first, const AddressInfo &second) {
            return first.currency < second.currency;
        });
        posInAddressInfos = 0;
    }

    LOG << PeriodicLog::make("f_bln") << "Try fetch balance " << addressesInfos.size();
    QString currentCurrency;
    std::map<QString, std::shared_ptr<ServersStruct>> servStructs;
    std::vector<QString> batch;
    size_t countParallelRequests = 0;

    const auto processBatch = [this, &batch, &countParallelRequests, &servStructs](const QString &currentCurrency) {
        if (servStructs.find(currentCurrency) != servStructs.end()) {
            infrastructureNsLookup.getTorrents(currentCurrency, 3, 3, InfrastructureNsLookup::GetServersCallback([this, batch, currentCurrency, servStruct=servStructs.at(currentCurrency)](const std::vector<QString> &servers) {
                infrastructureNsLookup.getContractTorrent(currentCurrency, 3, 3, InfrastructureNsLookup::GetServersCallback([this, batch, currentCurrency, serversSimple=servers](const std::vector<QString> &serversContract) {
                    for (const QString &address: batch) {
                        const std::vector<Transaction> pendingTxs = db.getPaymentsForAddressPending(address, currentCurrency, true);
                        processPendings(address, currentCurrency, pendingTxs, serversContract, serversSimple);
                    }
                }, [](const TypedException &error) {
                    LOG << "Error while get servers: " << error.description;
                }, signalFunc));

                if (servers.empty()) {
                    LOG << PeriodicLog::makeAuto("t_s0") << "Warn: servers empty: " << currentCurrency;
                    return;
                }
                processAddressMth(batch, currentCurrency, servers, servStruct);
            }, [](const TypedException &error) {
                LOG << "Error while get servers: " << error.description;
            }, signalFunc));

            batch.clear();
            countParallelRequests++;
        } else {
            // В предыдущий раз список серверов оказался пустым, поэтому структуру мы не заполнили. Пропускаем
        }
    };

    const time_point now = ::now();
    while (posInAddressInfos < addressesInfos.size()) {
        const AddressInfo &addr = addressesInfos[posInAddressInfos];
        if ((!currentCurrency.isEmpty() && (addr.currency != currentCurrency)) || batch.size() >= MAXIMUM_ADDRESSES_IN_BATCH) {
            processBatch(currentCurrency);
            if (countParallelRequests >= COUNT_PARALLEL_REQUESTS) {
                break;
            }
            currentCurrency = addr.currency;
        }
        if (currentCurrency.isEmpty()) {
            currentCurrency = addr.currency;
        }

        const auto found = servStructs.find(addr.currency);
        if (found == servStructs.end()) {
            servStructs.emplace(std::piecewise_construct, std::forward_as_tuple(addr.currency), std::forward_as_tuple(std::make_shared<ServersStruct>(addr.currency)));
        }
        servStructs.at(addr.currency)->countRequests++; // Не очень хорошо здесь прибавлять по 1, но пофиг
        if (now - lastCheckTxsTime >= CHECK_TXS_PERIOD) {
            infrastructureNsLookup.getTorrents(addr.currency, 3, 3, InfrastructureNsLookup::GetServersCallback([this, address=addr.address, currency=addr.currency](const std::vector<QString> &servers) {
                processCheckTxs(address, currency, servers);
            }, [](const TypedException &error) {
                LOG << "Error while get servers: " << error.description;
            }, signalFunc));
        }

        batch.emplace_back(addr.address);

        posInAddressInfos++;
    }
    processBatch(currentCurrency);

    if (now - lastCheckTxsTime >= CHECK_TXS_PERIOD && posInAddressInfos >= addressesInfos.size()) {
        LOG << "All txs checked";
        lastCheckTxsTime = now;
    }

    processPendings();
}

void Transactions::fetchBalanceAddress(const QString &address) {
    const std::vector<AddressInfo> infos = getAddressesInfos(makeGroupName(currentUserName));
    std::vector<AddressInfo> addressInfos;
    std::copy_if(infos.begin(), infos.end(), std::back_inserter(addressInfos), [&address](const AddressInfo &info) {
        return info.address == address;
    });

    LOG << "Found " << addressInfos.size() << " records on adrress " << address;
    for (const AddressInfo &addr: addressInfos) {
        infrastructureNsLookup.getTorrents(addr.currency, 3, 3, InfrastructureNsLookup::GetServersCallback([this, addr](const std::vector<QString> &servers) {
            if (servers.empty()) {
                LOG << "Warn: servers empty: " << addr.currency;
                return;
            }

            std::vector<QString> batch;
            batch.emplace_back(addr.address);
            processAddressMth(batch, addr.currency, servers, nullptr);
        }, [](const TypedException &e) {
            LOG << "Error while get servers: " << e.description;
        }, signalFunc));
    }
}

void Transactions::onRegisterAddresses(const std::vector<AddressInfo> &addresses, const RegisterAddressCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        auto transactionGuard = db.beginTransaction();
        for (const AddressInfo &address: addresses) {
            db.addTracked(address);
        }
        transactionGuard.commit();
    }, callback);
END_SLOT_WRAPPER
}

void Transactions::onGetAddresses(const QString &/*group*/, const GetAddressesCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        std::vector<AddressInfo> result = getAddressesInfos(makeGroupName(currentUserName));
        for (AddressInfo &info: result) {
            info.balance = getBalance(info.address, info.currency);
            info.balance.savedTxs = info.balance.countTxs;
        }
        return result;
    }, callback);
END_SLOT_WRAPPER
}

void Transactions::onSetCurrentGroup(const QString &/*group*/, const SetCurrentGroupCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&]{
        //currentGroup = group;
    }, callback);
END_SLOT_WRAPPER
}

void Transactions::onGetTxs2(const QString &address, const QString &currency, int from, int count, bool asc, const GetTxsCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        return db.getPaymentsForAddress(address, convertCurrency(currency), from, count, asc);
    }, callback);
END_SLOT_WRAPPER
}

void Transactions::onGetTxsFilters(const QString &address, const QString &currency, const Filters &filter, int from, int count, bool asc, const GetTxsCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        return db.getPaymentsForAddressFilter(address, convertCurrency(currency), filter, from, count, asc);
    }, callback);
END_SLOT_WRAPPER
}

void Transactions::onGetTxsAll2(const QString &currency, int from, int count, bool asc, const GetTxsCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        return db.getPaymentsForCurrency(makeGroupName(currentUserName), convertCurrency(currency), from, count, asc);
    }, callback);
END_SLOT_WRAPPER
}

void Transactions::onGetForgingTxs(const QString &address, const QString &currency, int from, int count, bool asc, const GetTxsCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        return db.getForgingPaymentsForAddress(address, convertCurrency(currency), from, count, asc);
    }, callback);
END_SLOT_WRAPPER
}

void Transactions::onGetDelegateTxs(const QString &address, const QString &currency, const QString &to, int from, int count, bool asc, const GetTxsCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        return db.getDelegatePaymentsForAddress(address, to, convertCurrency(currency), from, count, asc);
    }, callback);
END_SLOT_WRAPPER
}

void Transactions::onGetDelegateTxs2(const QString &address, const QString &currency, int from, int count, bool asc, const GetTxsCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        return db.getDelegatePaymentsForAddress(address, convertCurrency(currency), from, count, asc);
    }, callback);
END_SLOT_WRAPPER
}

void Transactions::onGetLastForgingTx(const QString &address, const QString &currency, const GetTxCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        return db.getLastForgingTransaction(address, convertCurrency(currency));
    }, callback);
END_SLOT_WRAPPER
}

void Transactions::onCalcBalance(const QString &address, const QString &currency, const CalcBalanceCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        BalanceInfo balance = getBalance(address, convertCurrency(currency));
        balance.savedTxs = balance.countTxs;
        return balance;
    }, callback);
END_SLOT_WRAPPER
}

Transactions::SendedTransactionWatcher::~SendedTransactionWatcher() {
    for (const QString &server: allServers) {
        txManager.sendErrorGetTx(requestId, hash, server);
        const auto found = errors.find(server);
        if (found != errors.end()) {
            LOG << "Get tx not parse " << server << " " << hash << " " << found->second;
        }
    }
}

void Transactions::sendErrorGetTx(const QString &requestId, const TransactionHash &hash, const QString &server) {
    emit javascriptWrapper.transactionInTorrentSig(requestId, server, QString::fromStdString(hash), Transaction(), TypedException(TypeErrors::TRANSACTIONS_SENDED_NOT_FOUND, "Transaction not found"));
}

void Transactions::onFindTxOnTorrentEvent() {
BEGIN_SLOT_WRAPPER
    const time_point now = ::now();

    for (auto iter = sendTxWathcers.begin(); iter != sendTxWathcers.end();) {
        const TransactionHash &hash = iter->first;
        SendedTransactionWatcher &watcher = iter->second;

        std::shared_ptr<bool> isFirst = std::make_shared<bool>(true);
        const QString message = makeGetTxRequest(QString::fromStdString(hash));
        const auto serversCopy = watcher.getServersCopy();
        for (const QString &server: serversCopy) {
            // Удаляем, чтобы не заддосить сервер на следующей итерации
            watcher.removeServer(server);
            client.sendMessagePost(server, message, [this, server, hash, isFirst, requestId=watcher.requestId, serversCopy](const SimpleClient::Response &response) {
                auto found = sendTxWathcers.find(hash);
                if (found == sendTxWathcers.end()) {
                    return;
                }
                if (!response.exception.isSet()) {
                    try {
                        const Transaction tx = parseGetTxResponse(QString::fromStdString(response.response), "", "");
                        emit javascriptWrapper.transactionInTorrentSig(requestId, server, QString::fromStdString(hash), tx, TypedException());
                        if (tx.status == Transaction::Status::PENDING) {
                            pendingTxsAfterSend.emplace_back(tx.tx, serversCopy);
                        }
                        if (*isFirst) {
                            *isFirst = false;
                            fetchBalanceAddress(tx.from);
                        }
                        found->second.okServer(server);
                        return;
                    } catch (const Exception &e) {
                        found->second.setError(server, QString::fromStdString(e.message));
                    } catch (...) {
                        // empty;
                    }
                }
                found->second.returnServer(server);
            }, timeout);
        }

        if (watcher.isTimeout(now)) {
            iter = sendTxWathcers.erase(iter);
        } else if (watcher.isEmpty()) {
            iter = sendTxWathcers.erase(iter);
        } else {
            iter++;
        }
    }
    if (sendTxWathcers.empty()) {
        LOG << "SendTxWatchers timer send stop";
        timerSendTx.stop();
    }
END_SLOT_WRAPPER
}

void Transactions::addTrackedForCurrentLogin() {
    const auto errorCallback = [](const TypedException &e) {
        LOG << "Error: " << e.description;
    };

    const auto processWallets = [this](bool isMhc, const QString &userName, const std::vector<wallets::WalletInfo> &walletAddresses) {
        CHECK(currentUserName == userName, "Current group already changed");

        const std::map<bool, std::set<QString>> currencyList = db.getAllCurrencys();
        const auto found = currencyList.find(isMhc);
        if (found == currencyList.end()) {
            return;
        }

        auto transactionGuard = db.beginTransaction();
        for (const QString &currency: found->second) {
            db.removeTrackedForGroup(currency, makeGroupName(currentUserName));
            for (const wallets::WalletInfo &wallet: walletAddresses) {
                db.addTracked(currency, wallet.address, makeGroupName(currentUserName));
            }
        }
        transactionGuard.commit();
    };

    emit wallets.getListWallets2(wallets::WalletCurrency::Mth, currentUserName, wallets::Wallets::WalletsListCallback([processWallets](const QString &userName, const std::vector<wallets::WalletInfo> &walletAddresses) {
        processWallets(true, userName, walletAddresses);
    }, errorCallback, signalFunc));
    emit wallets.getListWallets2(wallets::WalletCurrency::Tmh, currentUserName, wallets::Wallets::WalletsListCallback([processWallets](const QString &userName, const std::vector<wallets::WalletInfo> &walletAddresses) {
        processWallets(false, userName, walletAddresses);
    }, errorCallback, signalFunc));
}

void Transactions::onLogined(bool isInit, const QString login) {
BEGIN_SLOT_WRAPPER
    if (!isInit) {
        return;
    }
    if (login == currentUserName) {
        if (!login.isEmpty() || isUserNameSetted) {
            return;
        }
    }
    isUserNameSetted = true;
    currentUserName = login;
    addTrackedForCurrentLogin();
END_SLOT_WRAPPER
}

void Transactions::onMthWalletCreated(bool isMhc, const QString &address, const QString &userName) {
BEGIN_SLOT_WRAPPER
    const std::map<bool, std::set<QString>> currencyList = db.getAllCurrencys();
    const auto found = currencyList.find(isMhc);
    if (found == currencyList.end()) {
        return;
    }
    for (const QString &currency: found->second) {
        db.addTracked(currency, address, makeGroupName(userName));
    }
END_SLOT_WRAPPER
}

void Transactions::onMthWatchWalletsAdded(bool isMhc, const std::vector<std::pair<QString, QString>> &created, const QString &username) {
BEGIN_SLOT_WRAPPER
    const std::map<bool, std::set<QString>> currencyList = db.getAllCurrencys();
    const auto found = currencyList.find(isMhc);
    if (found == currencyList.end()) {
        return;
    }
    for (const QString &currency: found->second) {
        for (const auto &pair: created) {
            db.addTracked(currency, pair.first, makeGroupName(username));
        }
    }
END_SLOT_WRAPPER
}

void Transactions::addToSendTxWatcher(const QString &requestId, const TransactionHash &hash, size_t countServers, const std::vector<QString> &servers, const seconds &timeout) {
    if (sendTxWathcers.find(hash) != sendTxWathcers.end()) {
        return;
    }

    const size_t remainServersGet = countServers - servers.size();
    for (size_t i = 0; i < remainServersGet; i++) {
        emit javascriptWrapper.transactionInTorrentSig(requestId, "", QString::fromStdString(hash), Transaction(), TypedException(TypeErrors::TRANSACTIONS_SERVER_NOT_FOUND, "dns return less laid"));
    }
    const time_point now = ::now();
    sendTxWathcers.emplace(std::piecewise_construct, std::forward_as_tuple(hash), std::forward_as_tuple(*this, requestId, hash, now, servers, timeout));
    LOG << "SendTxWatchers timer send start";
    timerSendTx.start();
}

void Transactions::onSendTransaction(const QString &requestId, const QString &to, const QString &value, size_t nonce, const QString &data, const QString &fee, const QString &pubkey, const QString &sign, const SendParameters &sendParams, const SendTransactionCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        const QString request = makeSendTransactionRequest(to, value, nonce, data, fee, pubkey, sign);
        const auto sendTx = [this, sendParams, requestId, request, callback](const std::vector<QString> &servers) {
            const size_t countServersSend = sendParams.countServersSend;
            CHECK_TYPED(!servers.empty(), TypeErrors::TRANSACTIONS_SERVER_NOT_FOUND, "Not enough servers send");
            const size_t remainServersSend = countServersSend - servers.size();
            for (size_t i = 0; i < remainServersSend; i++) {
                emit javascriptWrapper.sendedTransactionsResponseSig(requestId, "", "", TypedException(TypeErrors::TRANSACTIONS_SERVER_NOT_FOUND, "dns return less laid"));
            }

            const auto getTx = [this, servers, request, requestId, sendParams](const std::vector<QString> &serversGet) {
                CHECK_TYPED(!serversGet.empty(), TypeErrors::TRANSACTIONS_SERVER_NOT_FOUND, "Not enough servers get");

                for (const QString &server: servers) {
                    tcpClient.sendMessagePost(server, request, [this, server, requestId, sendParams, serversGet](const std::string &response, const TypedException &error) {
                        QString result;
                        const TypedException exception = apiVrapper2([&] {
                            if (error.isSet()) {
                                nsLookup.rejectServer(server);
                            }
                            CHECK_TYPED(!error.isSet(), TypeErrors::TRANSACTIONS_SERVER_SEND_ERROR, error.description + ". " + server.toStdString());
                            result = parseSendTransactionResponse(QString::fromStdString(response));
                            addToSendTxWatcher(requestId, result.toStdString(), sendParams.countServersGet, serversGet, sendParams.timeout);
                        });
                        emit javascriptWrapper.sendedTransactionsResponseSig(requestId, server, result, exception);
                    }, timeout);
                }
            };

            if (!sendParams.currency.isEmpty()) {
                infrastructureNsLookup.getTorrents(sendParams.currency, sendParams.countServersGet, sendParams.countServersGet, InfrastructureNsLookup::GetServersCallback(getTx, callback, signalFunc)); // Получаем список серверов здесь, чтобы здесь же обработать ошибки
            } else {
                nsLookup.getRandomServers(sendParams.typeGet, sendParams.countServersGet, sendParams.countServersGet, NsLookup::GetServersCallback(getTx, callback, signalFunc)); // deprecated
            }
        };

        if (!sendParams.currency.isEmpty()) {
            infrastructureNsLookup.getProxy(sendParams.currency, sendParams.countServersSend, sendParams.countServersSend, InfrastructureNsLookup::GetServersCallback(sendTx, callback, signalFunc));
        } else {
            nsLookup.getRandomServers(sendParams.typeSend, sendParams.countServersSend, sendParams.countServersSend, NsLookup::GetServersCallback(sendTx, callback, signalFunc)); // deprecated
        }
    }, callback);
END_SLOT_WRAPPER
}

void Transactions::onGetNonce(const QString &from, const SendParameters &sendParams, const GetNonceCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitErrorCallback([&, this]{
        const auto processNonce = [this, from, callback](const std::vector<QString> &servers) {
            CHECK(!servers.empty(), "Not enough servers");

            struct NonceStruct {
                bool isSet = false;
                uint64_t nonce = 0;
                size_t count;
                TypedException exception;
                QString serverError;

                NonceStruct(size_t count)
                   : count(count)
                {}
            };

            std::shared_ptr<NonceStruct> nonceStruct = std::make_shared<NonceStruct>(servers.size());

            const auto getBalanceCallback = [nonceStruct, from, callback](const QString &server, const SimpleClient::Response &response) {
                nonceStruct->count--;

                if (!response.exception.isSet()) {
                    const BalanceInfo balanceResponse = parseBalanceResponse(QString::fromStdString(response.response));
                    nonceStruct->isSet = true;
                    nonceStruct->nonce = std::max(nonceStruct->nonce, balanceResponse.countSpent);
                } else {
                    nonceStruct->exception = TypedException(TypeErrors::CLIENT_ERROR, response.exception.description);
                    nonceStruct->serverError = server;
                }

                if (nonceStruct->count == 0) {
                    if (!nonceStruct->isSet) {
                        callback.emitFunc(nonceStruct->exception, 0, nonceStruct->serverError);
                    } else {
                        callback.emitFunc(TypedException(), nonceStruct->nonce + 1, "");
                    }
                }
            };

            const QString requestBalance = makeGetBalanceRequest(from);
            for (const QString &server: servers) {
                client.sendMessagePost(server, requestBalance, std::bind(getBalanceCallback, server, _1), timeout);
            }
        };

        if (!sendParams.currency.isEmpty()) {
            infrastructureNsLookup.getTorrents(sendParams.currency, sendParams.countServersGet, sendParams.countServersGet, InfrastructureNsLookup::GetServersCallback(processNonce, callback, signalFunc));
        } else {
            nsLookup.getRandomServers(sendParams.typeGet, sendParams.countServersGet, sendParams.countServersGet, NsLookup::GetServersCallback(processNonce, callback, signalFunc));
        }
    }, callback);
END_SLOT_WRAPPER
}

void Transactions::onGetTxFromServer(const QString &txHash, const QString &type, const GetTxCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitErrorCallback([&, this] {
        const QString message = makeGetTxRequest(txHash);

        nsLookup.getRandomServers(type, 1, 1, NsLookup::GetServersCallback([this, message, callback](const std::vector<QString> &servers){
            CHECK(!servers.empty(), "Not enough servers");
            const QString &server = servers[0];

            client.sendMessagePost(server, message, [callback](const SimpleClient::Response &response) mutable {
                Transaction tx;
                const TypedException exception = apiVrapper2([&] {
                    CHECK_TYPED(!response.exception.isSet(), TypeErrors::CLIENT_ERROR, response.exception.description);
                    tx = parseGetTxResponse(QString::fromStdString(response.response), "", "");
                });
                callback.emitFunc(exception, tx);
            }, timeout);
        }, callback, signalFunc));
    }, callback);
END_SLOT_WRAPPER
}

void Transactions::onGetLastUpdateBalance(const QString& currency, const GetLastUpdateCallback& callback)
{
    BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        const system_time_point now = ::system_now();
        auto found = lastSuccessUpdateTimestamps.find(currency);
        system_time_point result;
        if (found != lastSuccessUpdateTimestamps.end()) {
            result = found->second;
        }
        return std::make_tuple(result, now);
    },
        callback);
    END_SLOT_WRAPPER
}

void Transactions::onGetTokensAddress(const QString& address, const GetTokensCallback& callback)
{
    BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        return db.getTokensForAddress(address);
    },
        callback);
    END_SLOT_WRAPPER
}

void Transactions::onAddCurrencyConformity(bool isMhc, const QString &currency, const AddCurrencyConformity &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitErrorCallback([&, this]{
        db.addToCurrency(isMhc, currency);

        const auto process = [this, callback](const QString &currency, const std::vector<wallets::WalletInfo> &walletAddresses) {
            auto transactionGuard = db.beginTransaction();
            db.removeTrackedForGroup(currency, makeGroupName(currentUserName));
            for (const wallets::WalletInfo &wallet: walletAddresses) {
                db.addTracked(currency, wallet.address, makeGroupName(currentUserName));
            }
            transactionGuard.commit();

            callback.emitCallback();
        };

        if (isMhc) {
            emit wallets.getListWallets2(wallets::WalletCurrency::Mth, currentUserName, wallets::Wallets::WalletsListCallback([this, process, currency](const QString &userName, const std::vector<wallets::WalletInfo> &walletAddresses) {
                CHECK(currentUserName == userName, "Username changed while requested");
                process(currency, walletAddresses);
            }, callback, signalFunc));
        } else {
            emit wallets.getListWallets2(wallets::WalletCurrency::Tmh, currentUserName, wallets::Wallets::WalletsListCallback([this, process, currency](const QString &userName, const std::vector<wallets::WalletInfo> &walletAddresses) {
                CHECK(currentUserName == userName, "Username changed while requested");
                process(currency, walletAddresses);
            }, callback, signalFunc));
        }
    }, callback);
END_SLOT_WRAPPER
}

void Transactions::onGetBalancesFromTorrent(const QString &id, const  QUrl &url, const std::vector<std::pair<QString, QString>> &addresses)
{
BEGIN_SLOT_WRAPPER
    const auto getBalanceCallback = [this, id, addresses](const SimpleClient::Response &response) {
        std::vector<std::pair<QString, BalanceInfo>> res;
        res.reserve(addresses.size());
        if (response.exception.isSet()) {

            std::transform(addresses.begin(), addresses.end(), std::back_inserter(res), [](const auto &pair) {
                return std::make_pair(pair.first, BalanceInfo(pair.second));
            });
            emit getBalancesFromTorrentResult(id, false, QString::fromStdString(response.exception.toString()), res);

        } else {
            const std::string &resp = response.response;
            const std::map<QString, BalanceInfo> infos = parseBalancesResponseToMap(QString::fromStdString(resp));
            CHECK(infos.size() == addresses.size(), "Incorrect balances response");

            std::transform(addresses.begin(), addresses.end(), std::back_inserter(res), [infos](const auto &pair) {
                return std::make_pair(pair.first, infos.at(pair.second));
            });

            emit getBalancesFromTorrentResult(id, true, QString(), res);
        }
    };

    std::vector<QString> addrs;
    addrs.reserve(addresses.size());
    std::transform(addresses.begin(), addresses.end(), std::back_inserter(addrs), [](const auto &pair) {
        return pair.second;
    });

    const QString requestBalance = makeGetBalancesRequest(addrs);
    client.sendMessagePost(url, requestBalance, getBalanceCallback, timeout);
END_SLOT_WRAPPER
}

void Transactions::onClearDb(const QString &currency, const ClearDbCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        db.removePaymentsForCurrency(currency);
        nsLookup.resetFile();
    }, callback);
END_SLOT_WRAPPER
}

SendParameters parseSendParams(const QString &paramsJson) {
    return parseSendParamsInternal(paramsJson);
}

}
