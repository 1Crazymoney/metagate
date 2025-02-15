#include "TransactionsDBStorage.h"

#include <QtSql>
#include <QDebug>

#include "TransactionsDBRes.h"
#include "check.h"
#include "Log.h"

SET_LOG_NAMESPACE("TXS");

namespace transactions {

static void addFilter(QString &request, const Filters &filters) {
    QString filter;
    if (filters.isDelegate == FilterType::False) {
        filter += " AND type != " + QString::number(Transaction::Type::DELEGATE) + " ";
    } else if (filters.isDelegate == FilterType::True) {
        filter += " AND type = " + QString::number(Transaction::Type::DELEGATE) + " ";
    }
    if (filters.isForging == FilterType::False) {
        filter += " AND type != " + QString::number(Transaction::Type::FORGING) + " ";
    } else if (filters.isForging == FilterType::True) {
        filter += " AND type = " + QString::number(Transaction::Type::FORGING) + " ";
    }
    if (filters.isInput == FilterType::False) {
        filter += " AND ufrom != :from ";
    } else if (filters.isInput == FilterType::True) {
        filter += " AND ufrom = :from ";
    }
    if (filters.isOutput == FilterType::False) {
        filter += " AND uto != :to ";
    } else if (filters.isOutput == FilterType::True) {
        filter += " AND uto = :to ";
    }
    if (filters.isTesting == FilterType::False) {
        filter += " AND intStatus != " + QString::number(STATUS_TESTING) + " ";
    } else if (filters.isTesting == FilterType::True) {
        filter += " AND intStatus = " + QString::number(STATUS_TESTING) + " ";
    }
    if (filters.isSuccess == FilterType::False) {
        filter += " AND status != " + QString::number(Transaction::Status::OK) + " ";
    } else if (filters.isTesting == FilterType::True) {
        filter += " AND status = " + QString::number(Transaction::Status::OK) + " ";
    }

    request.replace("%filter%", filter);
    (void)filter;
}

TransactionsDBStorage::TransactionsDBStorage(const QString &path)
    : DBStorage(path, databaseName)
{

}

int TransactionsDBStorage::currentVersion() const
{
    return databaseVersion;
}

void TransactionsDBStorage::addPayment(const QString &currency, const QString &txid, const QString &address, qint64 index,
                                       const QString &ufrom, const QString &uto, const QString &value,
                                       quint64 ts, const QString &data, const QString &fee, qint64 nonce,
                                       bool isDelegate, const QString &delegateValue, const QString &delegateHash,
                                       Transaction::Status status, Transaction::Type type, qint64 blockNumber, const QString &blockHash, int intStatus)
{
    QSqlQuery query(database());
    CHECK(query.prepare(insertPayment), query.lastError().text().toStdString());
    query.bindValue(":currency", currency);
    query.bindValue(":txid", txid);
    query.bindValue(":address", address);
    query.bindValue(":ind", index);
    query.bindValue(":ufrom", ufrom);
    query.bindValue(":uto", uto);
    query.bindValue(":value", value);
    query.bindValue(":ts", ts);
    query.bindValue(":data", data);
    query.bindValue(":fee", fee);
    query.bindValue(":nonce", nonce);
    query.bindValue(":isDelegate", isDelegate);
    query.bindValue(":delegateValue", delegateValue);
    query.bindValue(":delegateHash", delegateHash);
    query.bindValue(":status", status);
    query.bindValue(":type", type);
    query.bindValue(":blockNumber", blockNumber);
    query.bindValue(":blockHash", blockHash);
    query.bindValue(":intStatus", intStatus);
    CHECK(query.exec(), query.lastError().text().toStdString());

}

void TransactionsDBStorage::addPayment(const Transaction &trans)
{
    addPayment(trans.currency, trans.tx, trans.address, trans.blockIndex,
               trans.from, trans.to, trans.value,
               trans.timestamp, trans.data, trans.fee, trans.nonce,
               trans.isDelegate, trans.delegateValue, trans.delegateHash,
               trans.status, trans.type, trans.blockNumber, trans.blockHash, trans.intStatus);
}

void TransactionsDBStorage::addPayments(const std::vector<Transaction> &transactions)
{
    auto transactionGuard = beginTransaction();
    for (const Transaction &transaction: transactions) {
        addPayment(transaction);
    }
    transactionGuard.commit();
}

std::vector<Transaction> TransactionsDBStorage::getPaymentsForAddress(const QString &address, const QString &currency,
                                                                      qint64 offset, qint64 count, bool asc)
{
    std::vector<Transaction> res;
    QSqlQuery query(database());
    QString q = selectPaymentsForDestFilter.arg(asc ? QStringLiteral("ASC") : QStringLiteral("DESC"));
    addFilter(q, Filters());
    CHECK(query.prepare(q),
          query.lastError().text().toStdString());
    query.bindValue(":address", address);
    query.bindValue(":currency", currency);
    query.bindValue(":offset", offset);
    query.bindValue(":count", count);
    CHECK(query.exec(), query.lastError().text().toStdString());
    createPaymentsList(query, res);
    return res;
}

std::vector<Transaction> TransactionsDBStorage::getPaymentsForAddressFilter(const QString &address, const QString &currency, const Filters &filters,
                                                     qint64 offset, qint64 count, bool asc) {
    std::vector<Transaction> res;
    QSqlQuery query(database());
    QString q = selectPaymentsForDestFilter.arg(asc ? QStringLiteral("ASC") : QStringLiteral("DESC"));
    addFilter(q, filters);
    CHECK(query.prepare(q),
          query.lastError().text().toStdString());
    query.bindValue(":address", address);
    query.bindValue(":from", address);
    query.bindValue(":to", address);
    query.bindValue(":currency", currency);
    query.bindValue(":offset", offset);
    query.bindValue(":count", count);
    CHECK(query.exec(), query.lastError().text().toStdString());
    createPaymentsList(query, res);
    return res;
}

std::vector<Transaction> TransactionsDBStorage::getPaymentsForCurrency(const QString &group, const QString &currency,
                                                                       qint64 offset, qint64 count, bool asc) const
{
    std::vector<Transaction> res;
    QSqlQuery query(database());
    CHECK(query.prepare(selectPaymentsForCurrency.arg(asc ? QStringLiteral("ASC") : QStringLiteral("DESC"))),
          query.lastError().text().toStdString());
    query.bindValue(":currency", currency);
    query.bindValue(":tgroup", group);
    query.bindValue(":offset", offset);
    query.bindValue(":count", count);
    CHECK(query.exec(), query.lastError().text().toStdString());
    createPaymentsList(query, res);
    return res;
}

std::vector<Transaction> TransactionsDBStorage::getPaymentsForAddressPending(const QString &address, const QString &currency, bool asc) const
{
    std::vector<Transaction> res;
    QSqlQuery query(database());
    CHECK(query.prepare(selectPaymentsForDestPending.arg(asc ? QStringLiteral("ASC") : QStringLiteral("DESC")).arg(Transaction::Status::PENDING).arg(Transaction::Status::MODULE_NOT_SET)),
          query.lastError().text().toStdString());
    query.bindValue(":address", address);
    query.bindValue(":currency", currency);
    CHECK(query.exec(), query.lastError().text().toStdString());
    createPaymentsList(query, res);
    return res;
}

std::vector<transactions::Transaction> transactions::TransactionsDBStorage::getForgingPaymentsForAddress(const QString &address, const QString &currency, qint64 offset, qint64 count, bool asc)
{
    std::vector<Transaction> res;
    QSqlQuery query(database());
    QString q = selectPaymentsForDestFilter.arg(asc ? QStringLiteral("ASC") : QStringLiteral("DESC"));
    Filters filter;
    filter.isForging = FilterType::True;
    addFilter(q, filter);
    CHECK(query.prepare(q),
          query.lastError().text().toStdString());
    query.bindValue(":address", address);
    query.bindValue(":currency", currency);
    query.bindValue(":offset", offset);
    query.bindValue(":count", count);
    CHECK(query.exec(), query.lastError().text().toStdString());
    createPaymentsList(query, res);
    return res;
}

std::vector<Transaction> TransactionsDBStorage::getDelegatePaymentsForAddress(const QString &address, const QString &to, const QString &currency, qint64 offset, qint64 count, bool asc) {
    std::vector<Transaction> res;
    QSqlQuery query(database());
    QString q = selectPaymentsForDestFilter.arg(asc ? QStringLiteral("ASC") : QStringLiteral("DESC"));
    Filters filter;
    filter.isDelegate = FilterType::True;
    filter.isSuccess = FilterType::True;
    filter.isInput = FilterType::True;
    filter.isOutput = FilterType::True;
    addFilter(q, filter);
    CHECK(query.prepare(q),
          query.lastError().text().toStdString());
    query.bindValue(":address", address);
    query.bindValue(":from", address);
    query.bindValue(":to", to);
    query.bindValue(":currency", currency);
    query.bindValue(":offset", offset);
    query.bindValue(":count", count);
    CHECK(query.exec(), query.lastError().text().toStdString());
    createPaymentsList(query, res);
    return res;
}

std::vector<Transaction> TransactionsDBStorage::getDelegatePaymentsForAddress(const QString &address, const QString &currency, qint64 offset, qint64 count, bool asc) {
    std::vector<Transaction> res;
    QSqlQuery query(database());
    QString q = selectPaymentsForDestFilter.arg(asc ? QStringLiteral("ASC") : QStringLiteral("DESC"));
    Filters filter;
    filter.isDelegate = FilterType::True;
    filter.isSuccess = FilterType::True;
    filter.isInput = FilterType::True;
    addFilter(q, filter);
    CHECK(query.prepare(q),
          query.lastError().text().toStdString());
    query.bindValue(":address", address);
    query.bindValue(":from", address);
    query.bindValue(":currency", currency);
    query.bindValue(":offset", offset);
    query.bindValue(":count", count);
    CHECK(query.exec(), query.lastError().text().toStdString());
    createPaymentsList(query, res);
    return res;
}

Transaction TransactionsDBStorage::getLastTransaction(const QString &address, const QString &currency) {
    Transaction trans;
    QSqlQuery query(database());
    CHECK(query.prepare(selectLastTransaction), query.lastError().text().toStdString());
    query.bindValue(":address", address);
    query.bindValue(":currency", currency);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (query.next()) {
        setTransactionFromQuery(query, trans);
    }
    return trans;
}

Transaction TransactionsDBStorage::getLastForgingTransaction(const QString &address, const QString &currency)
{
    Transaction trans;
    QSqlQuery query(database());
    CHECK(query.prepare(selectLastForgingTransaction.arg(Transaction::FORGING)), query.lastError().text().toStdString());
    query.bindValue(":address", address);
    query.bindValue(":currency", currency);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (query.next()) {
        setTransactionFromQuery(query, trans);
    }
    return trans;
}

void TransactionsDBStorage::updatePayment(const QString &address, const QString &currency, const QString &txid, qint64 blockNumber, qint64 index, const Transaction &trans)
{
    QSqlQuery query(database());
    CHECK(query.prepare(updatePaymentForAddress), query.lastError().text().toStdString())
            query.bindValue(":address", address);
    query.bindValue(":currency", currency);
    query.bindValue(":txid", txid);
    query.bindValue(":blockNumber", blockNumber);
    query.bindValue(":ind", index);

    query.bindValue(":ufrom", trans.from);
    query.bindValue(":uto", trans.to);
    query.bindValue(":value", trans.value);
    query.bindValue(":ts", static_cast<qint64>(trans.timestamp));
    query.bindValue(":data", trans.data);
    query.bindValue(":fee", trans.fee);
    query.bindValue(":nonce", static_cast<qint64>(trans.nonce));
    query.bindValue(":isDelegate", trans.isDelegate);
    query.bindValue(":delegateValue", trans.delegateValue);
    query.bindValue(":delegateHash", trans.delegateHash);
    query.bindValue(":status", trans.status);
    query.bindValue(":type", trans.type);
    query.bindValue(":blockHash", trans.blockHash);
    query.bindValue(":intStatus", trans.intStatus);
    CHECK(query.exec(), query.lastError().text().toStdString());
}

void TransactionsDBStorage::removePaymentsForDest(const QString &address, const QString &currency)
{
    QSqlQuery query(database());
    CHECK(query.prepare(deletePaymentsForAddress), query.lastError().text().toStdString());
    query.bindValue(":address", address);
    query.bindValue(":currency", currency);
    CHECK(query.exec(), query.lastError().text().toStdString());
}

qint64 TransactionsDBStorage::getPaymentsCountForAddress(const QString &address, const QString &currency) {
    QSqlQuery query(database());
    CHECK(query.prepare(selectPaymentsCountForAddress2), query.lastError().text().toStdString());
    query.bindValue(":address", address);
    query.bindValue(":currency", currency);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (query.next()) {
        return query.value("count").toLongLong();
    }
    return 0;
}

void TransactionsDBStorage::addTracked(const QString &currency, const QString &address, const QString &tgroup)
{
    QSqlQuery query(database());
    CHECK(query.prepare(insertTracked), query.lastError().text().toStdString());
    query.bindValue(":currency", currency);
    query.bindValue(":address", address);
    query.bindValue(":tgroup", tgroup);
    CHECK(query.exec(), query.lastError().text().toStdString());
}

void TransactionsDBStorage::addTracked(const AddressInfo &info)
{
    addTracked(info.currency, info.address, info.group);
}

std::vector<AddressInfo> TransactionsDBStorage::getTrackedForGroup(const QString &tgroup)
{
    std::vector<AddressInfo> res;
    QSqlQuery query(database());
    CHECK(query.prepare(selectTrackedForGroup), query.lastError().text().toStdString());
    query.bindValue(":tgroup", tgroup);
    CHECK(query.exec(), query.lastError().text().toStdString());
    while (query.next()) {
        AddressInfo info(query.value("currency").toString(),
                         query.value("address").toString(),
                         tgroup
                         );
        res.push_back(info);
    }
    return res;
}

void TransactionsDBStorage::removeTrackedForGroup(const QString &currency, const QString &tgroup)
{
    QSqlQuery query(database());
    CHECK(query.prepare(removeTrackedForGroupQuery), query.lastError().text().toStdString());
    query.bindValue(":currency", currency);
    query.bindValue(":tgroup", tgroup);
    CHECK(query.exec(), query.lastError().text().toStdString());
}

void TransactionsDBStorage::removePaymentsForCurrency(const QString &currency)
{
    auto transactionGuard = beginTransaction();
    QSqlQuery query(database());
    CHECK(query.prepare(removePaymentsForCurrencyQuery.arg(currency.isEmpty() ? QStringLiteral(""): removePaymentsCurrencyWhere)), query.lastError().text().toStdString());
    if (!currency.isEmpty())
        query.bindValue(":currency", currency);
    CHECK(query.exec(), query.lastError().text().toStdString());
    CHECK(query.prepare(removeTrackedForCurrencyQuery.arg(currency.isEmpty() ? QStringLiteral(""): removePaymentsCurrencyWhere)), query.lastError().text().toStdString());
    if (!currency.isEmpty())
        query.bindValue(":currency", currency);
    CHECK(query.exec(), query.lastError().text().toStdString());
    transactionGuard.commit();
}

void TransactionsDBStorage::setBalance(const QString &currency, const QString &address, const BalanceInfo &balance) {
    removeBalance(currency, address);

    QSqlQuery query(database());
    CHECK(query.prepare(insertBalance), query.lastError().text().toStdString());
    query.bindValue(":currency", currency);
    query.bindValue(":address", address);
    query.bindValue(":received", balance.received.getDecimal());
    query.bindValue(":spent", balance.spent.getDecimal());
    query.bindValue(":countReceived", (qint64)balance.countReceived);
    query.bindValue(":countSpent", (qint64)balance.countSpent);
    query.bindValue(":countTxs", (qint64)balance.countTxs);
    query.bindValue(":currBlockNum", (qint64)balance.currBlockNum);
    query.bindValue(":countDelegated", (qint64)balance.countDelegated);
    query.bindValue(":delegate", balance.delegate.getDecimal());
    query.bindValue(":undelegate", balance.undelegate.getDecimal());
    query.bindValue(":delegated", balance.delegated.getDecimal());
    query.bindValue(":undelegated", balance.undelegated.getDecimal());
    query.bindValue(":reserved", balance.reserved.getDecimal());
    query.bindValue(":forged", balance.forged.getDecimal());
    query.bindValue(":tokenBlockNum", (qint64)balance.tokenBlockNum);
    CHECK(query.exec(), query.lastError().text().toStdString());
}

BalanceInfo TransactionsDBStorage::getBalance(const QString &currency, const QString &address) {
    QSqlQuery query(database());
    CHECK(query.prepare(selectBalance), query.lastError().text().toStdString());
    query.bindValue(":address", address);
    query.bindValue(":currency", currency);
    CHECK(query.exec(), query.lastError().text().toStdString());

    BalanceInfo balance;
    balance.address = address;

    if (query.next()) {
        balance.received = query.value("received").toByteArray();
        balance.spent = query.value("spent").toByteArray();
        balance.countReceived = static_cast<quint64>(query.value("countReceived").toLongLong());
        balance.countSpent = static_cast<quint64>(query.value("countSpent").toLongLong());
        balance.countTxs = static_cast<quint64>(query.value("countTxs").toLongLong());
        balance.currBlockNum = static_cast<quint64>(query.value("currBlockNum").toLongLong());
        balance.countDelegated = static_cast<quint64>(query.value("countDelegated").toLongLong());
        balance.delegate = query.value("delegate").toByteArray();
        balance.undelegate = query.value("undelegate").toByteArray();
        balance.delegated = query.value("delegated").toByteArray();
        balance.undelegated = query.value("undelegated").toByteArray();
        balance.reserved = query.value("reserved").toByteArray();
        balance.forged = query.value("forged").toByteArray();
        balance.tokenBlockNum = static_cast<quint64>(query.value("tokenBlockNum").toLongLong());
    }
    return balance;
}

void TransactionsDBStorage::removeBalance(const QString &currency, const QString &address) {
    QSqlQuery queryDelete(database());
    CHECK(queryDelete.prepare(deleteBalance), queryDelete.lastError().text().toStdString());
    queryDelete.bindValue(":currency", currency);
    queryDelete.bindValue(":address", address);
    CHECK(queryDelete.exec(), queryDelete.lastError().text().toStdString());
}

void TransactionsDBStorage::addToCurrency(bool isMhc, const QString &currency) {
    QSqlQuery queryDelete(database());
    CHECK(queryDelete.prepare(insertToCurrency), queryDelete.lastError().text().toStdString());
    queryDelete.bindValue(":currency", currency);
    queryDelete.bindValue(":isMhc", isMhc);
    CHECK(queryDelete.exec(), queryDelete.lastError().text().toStdString());
}

std::map<bool, std::set<QString>> TransactionsDBStorage::getAllCurrencys() {
    QSqlQuery query(database());
    CHECK(query.prepare(selectAllCurrency), query.lastError().text().toStdString());
    CHECK(query.exec(), query.lastError().text().toStdString());

    std::map<bool, std::set<QString>> result;

    while (query.next()) {
        const bool isMhc = query.value("isMhc").toBool();
        const QString currency = query.value("currency").toString();
        result[isMhc].emplace(currency);
    }
    return result;
}

void TransactionsDBStorage::updateToken(const Token& token)
{
    QSqlQuery query(database());
    CHECK(query.prepare(insertTokenInfo), query.lastError().text().toStdString());
    query.bindValue(":tokenAddress", token.tokenAddress);
    query.bindValue(":type", token.type);
    query.bindValue(":symbol", token.symbol);
    query.bindValue(":name", token.name);
    query.bindValue(":decimals", token.decimals);
    query.bindValue(":name", token.name);
    query.bindValue(":emission", (qint64)token.emission);
    query.bindValue(":owner", token.owner);
    CHECK(query.exec(), query.lastError().text().toStdString());
}

void TransactionsDBStorage::updateTokenBalance(const TokenBalance& tokenBalance)
{
    QSqlQuery query(database());
    CHECK(query.prepare(insertTokenBalanceInfo), query.lastError().text().toStdString());
    query.bindValue(":address", tokenBalance.address);
    query.bindValue(":tokenAddress", tokenBalance.tokenAddress);
    query.bindValue(":received", tokenBalance.received.getDecimal());
    query.bindValue(":spent", tokenBalance.spent.getDecimal());
    query.bindValue(":value", tokenBalance.value);
    query.bindValue(":countReceived", (qint64)tokenBalance.countReceived);
    query.bindValue(":countSpent", (qint64)tokenBalance.countSpent);
    query.bindValue(":countTxs", (qint64)tokenBalance.countTxs);
    CHECK(query.exec(), query.lastError().text().toStdString());
}

std::vector<TokenInfo> TransactionsDBStorage::getTokensForAddress(const QString& address)
{
    std::vector<TokenInfo> res;
    QSqlQuery query(database());
    CHECK(query.prepare(selectTokens.arg(address.isEmpty() ? QStringLiteral("") : selectTokensAddressWhere)), query.lastError().text().toStdString());
    if (!address.isEmpty())
        query.bindValue(":address", address);
    CHECK(query.exec(), query.lastError().text().toStdString());
    while (query.next()) {
        TokenInfo token;
        token.address = query.value("address").toString();
        token.tokenAddress = query.value("tokenAddress").toString();
        token.received = query.value("received").toByteArray();
        token.spent = query.value("spent").toByteArray();
        token.value = query.value("value").toString();
        token.countReceived = static_cast<quint64>(query.value("countReceived").toLongLong());
        token.countSpent = static_cast<quint64>(query.value("countSpent").toLongLong());
        token.countTxs = static_cast<quint64>(query.value("countTxs").toLongLong());
        token.type = query.value("type").toString();
        token.symbol = query.value("symbol").toString();
        token.name = query.value("name").toString();
        token.decimals = query.value("decimals").toInt();
        token.emission = static_cast<quint64>(query.value("emission").toLongLong());
        token.owner = query.value("owner").toString();

        res.push_back(token);
    }
    return res;
}

void TransactionsDBStorage::createDatabase()
{
    createTable(QStringLiteral("payments"), createPaymentsTable);
    createTable(QStringLiteral("tracked"), createTrackedTable);
    createTable(QStringLiteral("balance"), createBalanceTable);
    createTable(QStringLiteral("currency"), createCurrencyTable);
    createTable(QStringLiteral("tokens"), createTokensTable);
    createTable(QStringLiteral("tokenBalances"), createTokenBalancesTable);
    createIndex(createPaymentsIndex1);
    createIndex(createPaymentsIndex2);
    createIndex(createPaymentsIndex3);
    createIndex(createPaymentsIndex4);
    createIndex(createPaymentsIndex5);
    createIndex(createPaymentsIndex6);
    createIndex(createPaymentsIndex7);
    createIndex(createPaymentsIndex8);
    createIndex(createBalanceIndex1);
    createIndex(createBalanceUniqueIndex);
    createIndex(createPaymentsUniqueIndex);
    createIndex(createTrackedUniqueIndex);
    createIndex(createCurrencyUniqueIndex);
    createIndex(createTokenBalancesUniqueIndex);
}

void TransactionsDBStorage::setTransactionFromQuery(QSqlQuery& query, Transaction& trans) const
{
    trans.id = query.value("id").toLongLong();
    trans.currency = query.value("currency").toString();
    trans.address = query.value("address").toString();
    trans.tx = query.value("txid").toString();
    trans.from = query.value("ufrom").toString();
    trans.to = query.value("uto").toString();
    trans.value = query.value("value").toString();
    trans.data = query.value("data").toString();
    trans.timestamp = static_cast<quint64>(query.value("ts").toLongLong());
    trans.fee = query.value("fee").toString();
    trans.nonce = query.value("nonce").toLongLong();
    trans.isDelegate = query.value("isDelegate").toBool();
    trans.delegateValue = query.value("delegateValue").toString();
    trans.delegateHash = query.value("delegateHash").toString();
    trans.status = static_cast<Transaction::Status>(query.value("status").toInt());
    trans.type = static_cast<Transaction::Type>(query.value("type").toInt());
    trans.blockNumber = query.value("blockNumber").toLongLong();
    trans.blockIndex = query.value("ind").toLongLong();
    trans.blockHash = query.value("blockHash").toString();
    trans.intStatus = query.value("intStatus").toInt();
}

void TransactionsDBStorage::createPaymentsList(QSqlQuery& query, std::vector<Transaction>& payments) const
{
    while (query.next()) {
        Transaction trans;
        setTransactionFromQuery(query, trans);
        payments.push_back(trans);
    }
}
}
