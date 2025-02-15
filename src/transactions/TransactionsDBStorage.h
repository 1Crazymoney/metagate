#ifndef TRANSACTIONSDBSTORAGE_H
#define TRANSACTIONSDBSTORAGE_H

#include "dbstorage.h"
#include "Transaction.h"
#include "utilites/BigNumber.h"

#include <vector>
#include <set>

#include "TransactionsFilter.h"

namespace transactions {

class TransactionsDBStorage : public DBStorage
{
public:
    TransactionsDBStorage(const QString &path = QString());

    virtual int currentVersion() const final;

    void addPayment(const QString &currency, const QString &txid, const QString &address, qint64 index,
                    const QString &ufrom, const QString &uto, const QString &value,
                    quint64 ts, const QString &data, const QString &fee, qint64 nonce,
                    bool isDelegate, const QString &delegateValue, const QString &delegateHash,
                    Transaction::Status status, Transaction::Type type, qint64 blockNumber, const QString &blockHash, int intStatus);

    void addPayment(const Transaction &trans);
    void addPayments(const std::vector<Transaction> &transactions);

    std::vector<Transaction> getPaymentsForAddress(const QString &address, const QString &currency,
                                              qint64 offset, qint64 count, bool asc);

    std::vector<Transaction> getPaymentsForAddressFilter(const QString &address, const QString &currency, const Filters &filters,
                                              qint64 offset, qint64 count, bool asc);

    std::vector<Transaction> getPaymentsForCurrency(const QString &group, const QString &currency,
                                                  qint64 offset, qint64 count, bool asc) const;

    std::vector<Transaction> getPaymentsForAddressPending(const QString &address, const QString &currency,
                                                            bool asc) const;

    std::vector<Transaction> getForgingPaymentsForAddress(const QString &address, const QString &currency,
                                              qint64 offset, qint64 count, bool asc);

    std::vector<Transaction> getDelegatePaymentsForAddress(const QString &address, const QString &to, const QString &currency, qint64 offset, qint64 count, bool asc);

    std::vector<Transaction> getDelegatePaymentsForAddress(const QString &address, const QString &currency, qint64 offset, qint64 count, bool asc);

    Transaction getLastTransaction(const QString &address, const QString &currency);

    Transaction getLastForgingTransaction(const QString &address, const QString &currency);

    void updatePayment(const QString &address, const QString &currency, const QString &txid, qint64 blockNumber, qint64 index, const Transaction &trans);
    void removePaymentsForDest(const QString &address, const QString &currency);

    qint64 getPaymentsCountForAddress(const QString &address, const QString &currency);

    qint64 getIsSetDelegatePaymentsCountForAddress(const QString &address, const QString &currency, Transaction::Status status = Transaction::OK);

    void addTracked(const QString &currency, const QString &address, const QString &tgroup);
    void addTracked(const AddressInfo &info);

    std::vector<AddressInfo> getTrackedForGroup(const QString &tgroup);

    void removeTrackedForGroup(const QString &currency, const QString &tgroup);

    void removePaymentsForCurrency(const QString &currency);

    void setBalance(const QString &currency, const QString &address, const BalanceInfo &balance);

    BalanceInfo getBalance(const QString &currency, const QString &address);

    void removeBalance(const QString &currency, const QString &address);

    void addToCurrency(bool isMhc, const QString &currency);

    std::map<bool, std::set<QString>> getAllCurrencys();

    void updateToken(const Token& token);
    void updateTokenBalance(const TokenBalance& tokenBalance);
    std::vector<TokenInfo> getTokensForAddress(const QString& address = QString());

protected:
    virtual void createDatabase() final;

private:
    void setTransactionFromQuery(QSqlQuery &query, Transaction &trans) const;

    void createPaymentsList(QSqlQuery &query, std::vector<Transaction> &payments) const;

};

}

#endif // TRANSACTIONSDBSTORAGE_H
