#ifndef TRANSACTIONSJAVASCRIPT_H
#define TRANSACTIONSJAVASCRIPT_H

#include <QObject>

#include <functional>

#include "qt_utilites/WrapperJavascript.h"

namespace transactions {

class Transactions;

struct BalanceInfo;
struct Transaction;

class TransactionsJavascript: public WrapperJavascript {
    Q_OBJECT
public:

    explicit TransactionsJavascript();

    void setTransactions(Transactions &trans) {
        transactionsManager = &trans;
    }

signals:

    void newBalanceSig(const QString &address, const QString &currency, const BalanceInfo &balance);

    void sendedTransactionsResponseSig(const QString &requestId, const QString &server, const QString &response, const TypedException &error);

    void transactionInTorrentSig(const QString &requestId, const QString &server, const QString &txHash, const Transaction &tx, const TypedException &error);

    void transactionStatusChangedSig(const QString &address, const QString &currency, const QString &txHash, const Transaction &tx);

    void transactionStatusChanged2Sig(const QString &txHash, const Transaction &tx);

private slots:

    void onNewBalance(const QString &address, const QString &currency, const BalanceInfo &balance);

    void onSendedTransactionsResponse(const QString &requestId, const QString &server, const QString &response, const TypedException &error);

    void onTransactionInTorrent(const QString &requestId, const QString &server, const QString &txHash, const Transaction &tx, const TypedException &error);

    void onTransactionStatusChanged(const QString &address, const QString &currency, const QString &txHash, const Transaction &tx);

    void onTransactionStatusChanged2(const QString &txHash, const Transaction &tx);

public slots:

    Q_INVOKABLE void registerAddress(QString address, QString currency, QString type, QString group, QString name);

    Q_INVOKABLE void registerAddresses(QString addressesJson);

    Q_INVOKABLE void getAddresses(QString group);

    Q_INVOKABLE void setCurrentGroup(QString group);

    Q_INVOKABLE void getTxs2(QString address, QString currency, int from, int count, bool asc);

    Q_INVOKABLE void getTxsAll2(QString currency, int from, int count, bool asc);

    Q_INVOKABLE void getTxsFilters(QString address, QString currency, QString filtersJson, int from, int count, bool asc);

    Q_INVOKABLE void getForgingTxsAll(QString address, QString currency, int from, int count, bool asc);

    Q_INVOKABLE void getDelegateTxsAll(QString address, QString currency, QString to, int from, int count, bool asc);

    Q_INVOKABLE void getDelegateTxsAll2(QString address, QString currency, int from, int count, bool asc);

    Q_INVOKABLE void getLastForgingTx(QString address, QString currency);

    Q_INVOKABLE void calcBalance(const QString &address, const QString &currency, const QString &callback);

    Q_INVOKABLE void getTxFromServer(QString txHash, QString type);

    Q_INVOKABLE void getLastUpdatedBalance(QString currency);

    Q_INVOKABLE void addCurrencyConformity(bool isMhc, QString currency);

    Q_INVOKABLE void getTokensForAddress(QString address);

    Q_INVOKABLE void clearDb(QString currency);

private:
    Transactions* transactionsManager;
};

}

#endif // TRANSACTIONSJAVASCRIPT_H
