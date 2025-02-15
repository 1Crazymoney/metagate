#include "Messenger.h"

#include "check.h"
#include "qt_utilites/SlotWrapper.h"
#include "Log.h"
#include "qt_utilites/makeJsFunc.h"
#include "Paths.h"
#include "utilites/utils.h"
#include "qt_utilites/QRegister.h"

#include "MainWindow.h"
#include "MessengerMessages.h"
#include "MessengerJavascript.h"
#include "CryptographicManager.h"
#include "MessengerDBStorage.h"

#include "qt_utilites/ManagerWrapperImpl.h"

#include <functional>
using namespace std::placeholders;

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonObject>

#include <QSettings>

#include <QCryptographicHash>

SET_LOG_NAMESPACE("MSG");

namespace messenger {

static QString createHashMessage(const QString &message) {
    return QString(QCryptographicHash::hash(message.toUtf8(), QCryptographicHash::Sha512).toHex());
}

void Messenger::checkChannelTitle(const QString &title) {
    bool isUnicode = false;
    for (int i = 0; i < title.size(); ++i) {
        if (title.at(i).unicode() > 127) {
            isUnicode = true;
        }
    }
    CHECK_TYPED(!isUnicode, TypeErrors::CHANNEL_TITLE_INCORRECT, "Only ascii");
    bool isAlphanumeric = true;
    for (int i = 0; i < title.size(); i++) {
        const auto c = title.at(i);
        if (('0' <= c && c <= '9') || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '-' || c == '_') {
            // ok
        } else {
            isAlphanumeric = false;
        }
    }
    CHECK_TYPED(isAlphanumeric, TypeErrors::CHANNEL_TITLE_INCORRECT, "Only alphanumerics");
}

QString Messenger::getChannelSha(const QString &title) {
    checkChannelTitle(title);
    return QString(QCryptographicHash::hash(title.toUtf8(), QCryptographicHash::Sha256).toHex());
}

std::vector<QString> Messenger::stringsForSign() {
    return {makeTextForGetMyMessagesRequest(), makeTextForGetChannelRequest(), makeTextForGetChannelsRequest(), makeTextForMsgAppendKeyOnlineRequest(), makeTextForGetMyChannelsRequest()};
}

QString Messenger::makeTextForSignRegisterRequest(const QString &address, const QString &rsaPubkeyHex, uint64_t fee) {
    return messenger::makeTextForSignRegisterRequest(address, rsaPubkeyHex, fee);
}

QString Messenger::makeTextForSignRegisterBlockchainRequest(const QString &address, uint64_t fee, const QString &txHash, const QString &blockchain, const QString &blockchainName) {
    return messenger::makeTextForSignRegisterBlockchainRequest(address, fee, txHash, blockchain, blockchainName);
}

QString Messenger::makeTextForGetPubkeyRequest(const QString &address) {
    return messenger::makeTextForGetPubkeyRequest(address);
}

QString Messenger::makeTextForSendMessageRequest(const QString &address, const QString &dataHex, const QString &encryptedSelfDataHex, uint64_t fee, uint64_t timestamp) {
    return messenger::makeTextForSendMessageRequest(address, dataHex, encryptedSelfDataHex, fee, timestamp);
}

QString Messenger::makeTextForChannelCreateRequest(const QString &title, const QString titleSha, uint64_t fee) {
    return messenger::makeTextForChannelCreateRequest(title, titleSha, fee);
}

QString Messenger::makeTextForChannelAddWriterRequest(const QString &titleSha, const QString &address) {
    return messenger::makeTextForChannelAddWriterRequest(titleSha, address);
}

QString Messenger::makeTextForChannelDelWriterRequest(const QString &titleSha, const QString &address) {
    return messenger::makeTextForChannelDelWriterRequest(titleSha, address);
}

QString Messenger::makeTextForSendToChannelRequest(const QString &titleSha, const QString &text, uint64_t fee, uint64_t timestamp) {
    return messenger::makeTextForSendToChannelRequest(titleSha, text, fee, timestamp);
}

QString Messenger::makeTextForWantToTalkRequest(const QString &address) {
    return messenger::makeTextForWantTalkRequest(address);
}

static QString getWssServer() {
    QSettings settings(getSettingsPath(), QSettings::IniFormat);
    CHECK(settings.contains("web_socket/messenger"), "settings web_socket/messenger not found");
    return settings.value("web_socket/messenger").toString();
};

Messenger::Messenger(MessengerJavascript &javascriptWrapper, MessengerDBStorage &db, CryptographicManager &cryptManager, MainWindow &mainWin, QObject *parent)
    : TimerClass(1s, parent)
    , db(db)
    , javascriptWrapper(javascriptWrapper)
    , cryptManager(cryptManager)
    , wssClient(getWssServer())
{
    QSettings settings(getSettingsPath(), QSettings::IniFormat);
    CHECK(settings.contains("messenger/saveDecryptedMessage"), "settings timeout not found");
    isDecryptDataSave = settings.value("messenger/saveDecryptedMessage").toBool();

    Q_CONNECT(this, &Messenger::showNotification, &mainWin, &MainWindow::showNotification);

    Q_CONNECT(&wssClient, &WebSocketClient::messageReceived, this, &Messenger::onWssMessageReceived);

    Q_CONNECT(this, &Messenger::registerAddress, this, &Messenger::onRegisterAddress);
    Q_CONNECT(this, &Messenger::registerAddressFromBlockchain, this, &Messenger::onRegisterAddressFromBlockchain);
    Q_CONNECT(this, &Messenger::savePubkeyAddress, this, &Messenger::onSavePubkeyAddress);
    Q_CONNECT(this, &Messenger::getPubkeyAddress, this, &Messenger::onGetPubkeyAddress);
    Q_CONNECT(this, &Messenger::getUserInfo, this, &Messenger::onGetUserInfo);
    Q_CONNECT(this, &Messenger::getCollocutorInfo, this, &Messenger::onGetCollocutorInfo);
    Q_CONNECT(this, &Messenger::sendMessage, this, &Messenger::onSendMessage);
    Q_CONNECT(this, &Messenger::signedStrings, this, &Messenger::onSignedStrings);
    Q_CONNECT(this, &Messenger::getLastMessage, this, &Messenger::onGetLastMessage);
    Q_CONNECT(this, &Messenger::getSavedPos, this, &Messenger::onGetSavedPos);
    Q_CONNECT(this, &Messenger::getSavedsPos, this, &Messenger::onGetSavedsPos);
    Q_CONNECT(this, &Messenger::savePos, this, &Messenger::onSavePos);
    Q_CONNECT(this, &Messenger::getCountMessages, this, &Messenger::onGetCountMessages);
    Q_CONNECT(this, &Messenger::getHistoryAddress, this, &Messenger::onGetHistoryAddress);
    Q_CONNECT(this, &Messenger::getHistoryAddressAddress, this, &Messenger::onGetHistoryAddressAddress);
    Q_CONNECT(this, &Messenger::getHistoryAddressAddressCount, this, &Messenger::onGetHistoryAddressAddressCount);
    Q_CONNECT(this, &Messenger::createChannel, this, &Messenger::onCreateChannel);
    Q_CONNECT(this, &Messenger::addWriterToChannel, this, &Messenger::onAddWriterToChannel);
    Q_CONNECT(this, &Messenger::delWriterFromChannel, this, &Messenger::onDelWriterFromChannel);
    Q_CONNECT(this, &Messenger::getChannelList, this, &Messenger::onGetChannelList);
    Q_CONNECT(this, &Messenger::decryptMessages, this, &Messenger::onDecryptMessages);
    Q_CONNECT(this, &Messenger::isCompleteUser, this, &Messenger::onIsCompleteUser);
    Q_CONNECT(this, &Messenger::addAllAddressesInFolder, this, &Messenger::onAddAllAddressesInFolder);
    Q_CONNECT(this, &Messenger::wantToTalk, this, &Messenger::onWantToTalk);
    Q_CONNECT(this, &Messenger::reEmit, this, &Messenger::onReEmit);

    Q_REG2(uint64_t, "uint64_t", false);
    Q_REG(Message::Counter, "Message::Counter");
    Q_REG(GetMessagesCallback, "GetMessagesCallback");
    Q_REG(SavePosCallback, "SavePosCallback");
    Q_REG(GetSavedPosCallback, "GetSavedPosCallback");
    Q_REG(GetSavedsPosCallback, "GetSavedsPosCallback");
    Q_REG(Messenger::RegisterAddressCallback, "Messenger::RegisterAddressCallback");
    Q_REG(Messenger::RegisterAddressBlockchainCallback, "Messenger::RegisterAddressBlockchainCallback");
    Q_REG(SignedStringsCallback, "SignedStringsCallback");
    Q_REG(SavePubkeyCallback, "SavePubkeyCallback");
    Q_REG(GetPubkeyAddressCallback, "GetPubkeyAddressCallback");
    Q_REG(SendMessageCallback, "SendMessageCallback");
    Q_REG(GetCountMessagesCallback, "GetCountMessagesCallback");
    Q_REG(CreateChannelCallback, "CreateChannelCallback");
    Q_REG(AddWriterToChannelCallback, "AddWriterToChannelCallback");
    Q_REG(DelWriterToChannelCallback, "DelWriterToChannelCallback");
    Q_REG(GetChannelListCallback, "GetChannelListCallback");
    Q_REG(DecryptUserMessagesCallback, "DecryptUserMessagesCallback");
    Q_REG(CompleteUserCallback, "CompleteUserCallback");
    Q_REG(AddAllWalletsInFolderCallback, "AddAllWalletsInFolderCallback");
    Q_REG(WantToTalkCallback, "WantToTalkCallback");
    Q_REG(UserInfoCallback, "UserInfoCallback");

    Q_REG2(std::vector<QString>, "std::vector<QString>", false);

    if (!isDecryptDataSave) {
        db.removeDecryptedData();
    }

    moveToThread(TimerClass::getThread());

    wssClient.start();
}

Messenger::~Messenger() {
    TimerClass::exit();
}

void Messenger::invokeCallback(size_t requestId, const TypedException &exception) {
    CHECK(requestId != size_t(-1), "Incorrect request id");
    const auto found = callbacks.find(requestId);
    CHECK(found != callbacks.end(), "Not found callback for request " + std::to_string(requestId));
    const ResponseCallbacks callback = found->second; // копируем
    callbacks.erase(found);
    callback(exception);
}

void Messenger::finishMethod() {
    // empty
}

std::vector<QString> Messenger::getAddresses() const {
    const QStringList res = db.getUsersList();
    std::vector<QString> result;
    for (const QString r: res) {
        result.emplace_back(r);
    }
    return result;
}

void Messenger::getMessagesFromAddressFromWss(const QString &fromAddress, Message::Counter from, Message::Counter to, bool missed) {
    const QString pubkeyHex = db.getUserPublicKey(fromAddress);
    CHECK_TYPED(!pubkeyHex.isEmpty(), TypeErrors::INCOMPLETE_USER_INFO, "user pubkey not found " + fromAddress.toStdString());
    const QString signHex = getSignFromMethod(fromAddress, makeTextForGetMyMessagesRequest());
    size_t requestId = id.get();
    messageRetrieves.insert(requestId);
    if (missed) {
        loginMessagesRetrieveReqs.insert(requestId);
    }
    const QString message = makeGetMyMessagesRequest(pubkeyHex, signHex, from, to, requestId);
    emit wssClient.sendMessage(message);
}

void Messenger::getMessagesFromChannelFromWss(const QString &fromAddress, const QString &channelSha, Message::Counter from, Message::Counter to) {
    const QString pubkeyHex = db.getUserPublicKey(fromAddress);
    CHECK_TYPED(!pubkeyHex.isEmpty(), TypeErrors::INCOMPLETE_USER_INFO, "user pubkey not found " + fromAddress.toStdString());
    const QString signHex = getSignFromMethod(fromAddress, makeTextForGetChannelRequest());
    const QString message = makeGetChannelRequest(channelSha, from, to, pubkeyHex, signHex, id.get());
    emit wssClient.sendMessage(message);
}

void Messenger::clearAddressesToMonitored() {
    emit wssClient.setHelloString(std::vector<QString>{}, "Messenger");
}

void Messenger::addAddressToMonitored(const QString &address) {
    const QString pubkeyHex = db.getUserPublicKey(address);
    const bool pubkeyFound = !pubkeyHex.isEmpty();
    LOG << "Add address to monitored " << address << " " << pubkeyFound;
    if (!pubkeyFound) {
        return;
    }

    const QString signHexChannels = getSignFromMethod(address, makeTextForGetMyChannelsRequest());
    const QString messageGetMyChannels = makeGetMyChannelsRequest(pubkeyHex, signHexChannels, id.get());
    emit wssClient.addHelloString(messageGetMyChannels, "Messenger");
    emit wssClient.sendMessage(messageGetMyChannels);

    const QString signHex = getSignFromMethod(address, makeTextForMsgAppendKeyOnlineRequest());
    const QString message = makeAppendKeyOnlineRequest(pubkeyHex, signHex, id.get());
    emit wssClient.addHelloString(message, "Messenger");
    emit wssClient.sendMessage(message);
}

void Messenger::processMyChannels(const QString &address, const std::vector<ChannelInfo> &channels) {
    db.setChannelsNotVisited(address);
    const DBStorage::DbId userId = db.getUserId(address);
    CHECK(userId != DBStorage::not_found, "User not create: " + address.toStdString());
    for (const ChannelInfo &channel: channels) {
        const DBStorage::DbId dbId = db.getChannelForUserShaName(address, channel.titleSha);
        if (dbId != -1) {
            db.updateChannel(dbId, true);
        } else {
            db.addChannel(userId, channel.title, channel.titleSha, channel.admin == address, channel.admin, false, true, true);
            db.setLastReadCounterForUserContact(address, channel.titleSha, -1, true);
        }
    }
    db.setWriterForNotVisited(address);
    for (const ChannelInfo &channel: channels) {
        const Message::Counter counter = db.getMessageMaxCounter(address, channel.titleSha);
        if (counter < channel.counter) {
            getMessagesFromChannelFromWss(address, channel.titleSha, counter + 1, channel.counter);
        }
    }
}

void Messenger::processAddOrDeleteInChannel(const QString &address, const ChannelInfo &channel, bool isAdd) {
    if (!isAdd) {
        db.setChannelIsWriterForUserShaName(address, channel.titleSha, false);
        emit javascriptWrapper.deletedFromChannelSig(address, channel.title, channel.titleSha, channel.admin);
        MessengerDeleteFromChannelVariant event(address, channel.title, channel.titleSha, channel.admin);
        events.emplace_back(QVariant::fromValue(event));
    } else {
        const DBStorage::DbId userId = db.getUserId(address);
        CHECK(userId != DBStorage::not_found, "User not created: " + address.toStdString());
        db.addChannel(userId, channel.title, channel.titleSha, channel.admin == address, channel.admin, false, true, false);
        const Message::Counter cnt = channel.counter;
        if (cnt != -1) {
            getMessagesFromChannelFromWss(address, channel.titleSha, 0, cnt);
        }
        emit javascriptWrapper.addedToChannelSig(address, channel.title, channel.titleSha, channel.admin, channel.counter);
        MessengerAddedFromChannelVariant event(address, channel.title, channel.titleSha, channel.admin, channel.counter);
        events.emplace_back(QVariant::fromValue(event));
    }
}

static std::pair<QString, QString> getKVOnJson(const QJsonValue &val) {
    CHECK(val.isObject(), "Incorrect json");
    const QJsonObject v = val.toObject();
    CHECK(v.contains("key") && v.value("key").isString(), "Incorrect json: key field not found");
    const QString key = v.value("key").toString();
    CHECK(v.contains("value") && v.value("value").isString(), "Incorrect json: value field not found");
    const QString value = v.value("value").toString();

    return std::make_pair(key, value);
}

bool Messenger::checkSignsAddress(const QString &address) const {
    const QString jsonString = db.getUserSignatures(address);
    const QJsonDocument json = QJsonDocument::fromJson(jsonString.toUtf8());
    if (!json.isArray()) {
        return false;
    }
    const QJsonArray &array = json.array();
    std::map<QString, QString> allFields;
    for (const QJsonValue &val: array) {
        const auto valPair = getKVOnJson(val);

        allFields[valPair.first] = valPair.second;
    }

    const std::vector<QString> stringsForSigns = stringsForSign();
    for (const QString &str: stringsForSigns) {
        if (allFields.find(str) == allFields.end()) {
            return false;
        }
    }
    return true;
}

QString Messenger::getSignFromMethod(const QString &address, const QString &method) const {
    const QString jsonString = db.getUserSignatures(address);
    const QJsonDocument json = QJsonDocument::fromJson(jsonString.toUtf8());
    CHECK_TYPED(json.isArray(), TypeErrors::INCOMPLETE_USER_INFO, "Incorrect json " + jsonString.toStdString() + ". Address: " + address.toStdString());
    const QJsonArray &array = json.array();
    for (const QJsonValue &val: array) {
        const auto valPair = getKVOnJson(val);

        if (valPair.first == method) {
            return valPair.second;
        }
    }
    throwErrTyped(TypeErrors::INCOMPLETE_USER_INFO, ("Not found signed method " + method + " in address " + address).toStdString());
}

void Messenger::startMethod() {
    const std::vector<QString> monitoredAddresses = getAddresses();
    LOG << "Monitored addresses: " << monitoredAddresses.size();
    clearAddressesToMonitored();
    for (const QString &address: monitoredAddresses) {
        const TypedException exception = apiVrapper2([&]{
            addAddressToMonitored(address);
        });
        if (exception.isSet()) {
            LOG << "Monitored address exception: " << address << " " << exception.description;
        }
    }
}

void Messenger::timerMethod() {
    for (auto &pairDeferred: deferredMessages) {
        const QString &address = pairDeferred.first.first;
        const QString &channel = pairDeferred.first.second;
        DeferredMessage &deferred = pairDeferred.second;
        if (deferred.check()) {
            deferred.resetDeferred();
            const Message::Counter lastCnt = db.getMessageMaxCounter(address, "");
            LOG << "Defferred process message " << address << " " << channel << " " << lastCnt;
            if (channel.isEmpty()) {
                emit javascriptWrapper.newMessegesSig(address, lastCnt);
            } else {
                emit javascriptWrapper.newMessegesChannelSig(address, channel, lastCnt);
            }
        }
    }
}

void Messenger::processMessages(const QString &address, const std::vector<NewMessageResponse> &messages, bool isChannel, size_t requestId) {
    std::vector<Message> msgs;
    msgs.reserve(messages.size());
    std::transform(messages.begin(), messages.end(), std::back_inserter(msgs), [address, isChannel](const NewMessageResponse &m) {
        Message message;
        message.channel = m.channelName;
        message.isChannel = m.isChannel;
        message.collocutor = m.collocutor;
        message.counter = m.counter;
        message.dataHex = m.data;
        message.isDecrypted = false;
        message.fee = m.fee;
        const QString hashMessage = createHashMessage(m.data); // TODO брать хэш еще и по timestamp
        message.hash = hashMessage;
        message.isConfirmed = true;
        const bool isInput = isChannel ? (m.collocutor == address) : m.isInput;
        message.isCanDecrypted = true;
        message.isInput = isInput;
        message.timestamp = m.timestamp;
        message.username = address;

        return message;
    });

    const time_point now = ::now();
    if (now - notifiedCache.tp >= 2min) {
        notifiedCache.hashes.clear();
        notifiedCache.tp = now;
    }

    bool showNotifies = true;
    if (loginMessagesRetrieveReqs.contains(requestId)) {
        loginMessagesRetrieveReqs.remove(requestId);
        showNotifies = false;
        for_each(msgs.begin(), msgs.end(), [this](const Message &m) {
            if (m.isInput) {
                if (notifiedCache.hashes.find(m.hash) == notifiedCache.hashes.end()) {
                    retrievedMissed++;
                    notifiedCache.hashes.insert(m.hash);
                }
            }
        });

        if (loginMessagesRetrieveReqs.isEmpty() && retrievedMissed != 0) {
            emit showNotification(tr("Retrivied %1 messages").arg(retrievedMissed), QStringLiteral(""));
        }
    }
    if (messages.empty())
        return;


    const auto nextProcess = [this, isChannel, address, showNotifies](const std::vector<Message> &messages) {
        CHECK(!messages.empty(), "Empty messages");
        const QString channel = isChannel ? messages.front().channel : "";

        const Message::Counter currConfirmedCounter = db.getMessageMaxConfirmedCounter(address);
        CHECK(std::is_sorted(messages.begin(), messages.end()), "Messages not sorted");
        const Message::Counter minCounterInServer = messages.front().counter;
        const Message::Counter maxCounterInServer = messages.back().counter;

        bool deffer = false;
        for (const Message &m: messages) {
            if (!isChannel) {
                CHECK(!m.isChannel, "Message is channel");
            } else {
                CHECK(m.isChannel, "Message not channel");
                CHECK(m.channel == channel, "Mixed channels messagae");
            }
            CHECK(address == m.username, "Incorrect message");

            if (m.isInput) {
                LOG << "Add message " << m.username << " " << channel << " " << m.collocutor << " " << m.counter;
                if (showNotifies) {
                    if (notifiedCache.hashes.find(m.hash) == notifiedCache.hashes.end()) {
                        notifiedCache.hashes.insert(m.hash);
                        emit showNotification(tr("Message from %1").arg(m.collocutor), QStringLiteral(""));
                    }
                }
                db.addMessage(m);
                const QString collocutorOrChannel = isChannel ? channel : m.collocutor;
                const Message::Counter savedPos = db.getLastReadCounterForUserContact(m.username, collocutorOrChannel, isChannel); // TODO вместо метода get сделать метод is
                if (savedPos == -1) {
                    db.setLastReadCounterForUserContact(m.username, collocutorOrChannel, -1, isChannel); // TODO подумать нужен ли этот вызов
                }
            } else {
                const auto idPair = db.findFirstNotConfirmedMessageWithHash(m.username, m.hash, channel);
                const auto idDb = idPair.first;
                const Message::Counter counter = idPair.second;
                if (idDb != -1) {
                    LOG << "Update message " << m.username << " " << channel << " " << m.counter;
                    db.updateMessage(idDb, m.counter, true);
                    if (counter != m.counter && !db.hasMessageWithCounter(m.username, counter, channel)) {
                        if (!isChannel) {
                            getMessagesFromAddressFromWss(m.username, counter, counter);
                        } else {
                            getMessagesFromChannelFromWss(m.username, channel, counter, counter);
                        }
                        deffer = true;
                    }
                } else {
                    const auto idPair2 = db.findFirstMessageWithHash(m.username, m.hash, channel);
                    if (idPair2.first == -1) {
                        LOG << "Insert new output message " << m.username << " " << channel << " " << m.counter << " " << m.hash;
                        db.addMessage(m);
                    }
                }
            }
        }

        const auto deferrPair = std::make_pair(address, channel);
        if (deffer) {
            LOG << "Deffer message0 " << address << " " << channel;
            deferredMessages[deferrPair].setDeferred(2s);
        } else if (minCounterInServer > currConfirmedCounter + 1) {
            LOG << "Deffer message " << address << " " << channel << " " << minCounterInServer << " " << currConfirmedCounter << " " << maxCounterInServer;
            deferredMessages[deferrPair].setDeferred(2s);
            if (!isChannel) {
                getMessagesFromAddressFromWss(address, currConfirmedCounter + 1, minCounterInServer);
            } else {
                getMessagesFromChannelFromWss(address, channel, currConfirmedCounter + 1, minCounterInServer);
            }
        } else {
            if (!deferredMessages[deferrPair].isDeferred()) {
                if (!isChannel) {
                    emit javascriptWrapper.newMessegesSig(address, maxCounterInServer);
                } else {
                    emit javascriptWrapper.newMessegesChannelSig(address, channel, maxCounterInServer);
                }
            } else {
                LOG << "Deffer message2 " << address << " " << channel << " " << minCounterInServer << " " << currConfirmedCounter << " " << maxCounterInServer;
            }
        }
    };

    if (!isDecryptDataSave) {
        nextProcess(msgs);
    } else {
        emit cryptManager.tryDecryptMessages(msgs, address, CryptographicManager::DecryptMessagesCallback(nextProcess, [](const TypedException &exception) {
            LOG << "Error " << exception.numError << " " << exception.description;
        }, std::bind(&Messenger::callbackCall, this, _1), false));
    }
}

void Messenger::onWssMessageReceived(QString message) {
BEGIN_SLOT_WRAPPER
    const QJsonDocument messageJson = QJsonDocument::fromJson(message.toUtf8());
    const ResponseType responseType = getMethodAndAddressResponse(messageJson);

    if (responseType.isError) {
        LOG << "Messenger response error " << responseType.id << " " << responseType.method << " " << responseType.address << " " << responseType.error;
        if (responseType.id != size_t(-1)) {
            TypedException exception;
            if (responseType.errorType == ResponseType::ERROR_TYPE::ADDRESS_EXIST) {
                exception = TypedException(TypeErrors::MESSENGER_SERVER_ERROR_ADDRESS_EXIST, responseType.error.toStdString());
            } else if (responseType.errorType == ResponseType::ERROR_TYPE::SIGN_OR_ADDRESS_INVALID) {
                exception = TypedException(TypeErrors::MESSENGER_SERVER_ERROR_SIGN_OR_ADDRESS_INVALID, responseType.error.toStdString());
            } else if (responseType.errorType == ResponseType::ERROR_TYPE::INCORRECT_JSON) {
                exception = TypedException(TypeErrors::MESSENGER_SERVER_ERROR_INCORRECT_JSON, responseType.error.toStdString());
            } else if (responseType.errorType == ResponseType::ERROR_TYPE::ADDRESS_NOT_FOUND) {
                exception = TypedException(TypeErrors::MESSENGER_SERVER_ERROR_ADDRESS_NOT_FOUND, responseType.error.toStdString());
            } else if (responseType.errorType == ResponseType::ERROR_TYPE::CHANNEL_EXIST) {
                exception = TypedException(TypeErrors::MESSENGER_SERVER_ERROR_CHANNEL_EXIST, responseType.error.toStdString());
            } else if (responseType.errorType == ResponseType::ERROR_TYPE::CHANNEL_NOT_PERMISSION) {
                exception = TypedException(TypeErrors::MESSENGER_SERVER_ERROR_CHANNEL_NOT_PERMISSION, responseType.error.toStdString());
            } else if (responseType.errorType == ResponseType::ERROR_TYPE::CHANNEL_NOT_FOUND) {
                exception = TypedException(TypeErrors::MESSENGER_SERVER_ERROR_CHANNEL_NOT_FOUND, responseType.error.toStdString());
            } else if (responseType.errorType == ResponseType::ERROR_TYPE::INVALID_ADDRESS) {
                exception = TypedException(TypeErrors::MESSENGER_SERVER_ERROR_SIGN_OR_ADDRESS_INVALID, responseType.error.toStdString());
            } else {
                exception = TypedException(TypeErrors::MESSENGER_SERVER_ERROR_OTHER, responseType.error.toStdString());
            }

            invokeCallback(responseType.id, exception); // TODO Разбирать ответ от сервера
        }
        return;
    }

    if (responseType.method == METHOD::APPEND_KEY_TO_ADDR) {
        invokeCallback(responseType.id, TypedException());
    } else if (responseType.method == METHOD::COUNT_MESSAGES) {
        const Message::Counter currCounter = db.getMessageMaxConfirmedCounter(responseType.address);
        const Message::Counter messagesInServer = parseCountMessagesResponse(messageJson);
        if (currCounter < messagesInServer) {
            LOG << "Read missing messages " << responseType.address << " " << currCounter + 1 << " " << messagesInServer;
            getMessagesFromAddressFromWss(responseType.address, currCounter + 1, messagesInServer);
        } else {
            LOG << "Count messages " << responseType.address << " " << currCounter << " " << messagesInServer;
        }
    } else if (responseType.method == METHOD::GET_KEY_BY_ADDR) {
        const KeyMessageResponse publicKeyResult = parsePublicKeyMessageResponse(messageJson);
        LOG << "Save pubkey " << publicKeyResult.addr << " " << publicKeyResult.publicKey << " " << publicKeyResult.txHash << " " << publicKeyResult.blockchain_name;
        db.setContactPublicKey(publicKeyResult.addr, publicKeyResult.publicKey, publicKeyResult.txHash, publicKeyResult.blockchain_name);
        invokeCallback(responseType.id, TypedException());
    } else if (responseType.method == METHOD::NEW_MSG) {
        const NewMessageResponse messages = parseNewMessageResponse(messageJson);
        LOG << "New msg " << responseType.address << " " << messages.collocutor << " " << messages.counter;
        processMessages(responseType.address, {messages}, messages.isChannel, responseType.id);
    } else if (responseType.method == METHOD::NEW_MSGS) {
        const std::vector<NewMessageResponse> messages = parseNewMessagesResponse(messageJson);
        LOG << "New msgs " << responseType.address << " " << messages.size();
        //qDebug() << requestId << messageRetrieves.toList();
        if (messageRetrieves.contains(responseType.id)) {
            messageRetrieves.remove(responseType.id);
            processMessages(responseType.address, messages, false, responseType.id);
        }
    } else if (responseType.method == METHOD::GET_CHANNEL) {
        const std::vector<NewMessageResponse> messages = parseGetChannelResponse(messageJson);
        LOG << "New msgs " << responseType.address << " " << messages.size();
        processMessages(responseType.address, messages, true, responseType.id);
    } else if (responseType.method == METHOD::SEND_TO_ADDR) {
        LOG << "Send to addr ok " << responseType.address;
        invokeCallback(responseType.id, TypedException());
    } else if (responseType.method == METHOD::GET_MY_CHANNELS) {
        LOG << "Get my channels " << responseType.address;
        const std::vector<ChannelInfo> channelsInfos = parseGetMyChannelsResponse(messageJson);
        processMyChannels(responseType.address, channelsInfos);
    } else if (responseType.method == METHOD::CHANNEL_CREATE) {
        LOG << "Channel create ok " << responseType.address;
        invokeCallback(responseType.id, TypedException());
    } else if (responseType.method == METHOD::CHANNEL_ADD_WRITER) {
        LOG << "Channel add writer ok " << responseType.address;
        invokeCallback(responseType.id, TypedException());
    } else if (responseType.method == METHOD::CHANNEL_DEL_WRITER) {
        LOG << "Channel del writer ok " << responseType.address;
        invokeCallback(responseType.id, TypedException());
    } else if (responseType.method == METHOD::SEND_TO_CHANNEL) {
        LOG << "Send to channel ok " << responseType.address;
        invokeCallback(responseType.id, TypedException());
    } else if (responseType.method == METHOD::ADD_TO_CHANNEL) {
        const ChannelInfo channelInfo = parseAddToChannelResponse(messageJson);
        LOG << "Added to channel ok " << responseType.address << " " << channelInfo.titleSha;
        processAddOrDeleteInChannel(responseType.address, channelInfo, true);
    } else if (responseType.method == METHOD::DEL_FROM_CHANNEL) {
        const ChannelInfo channelInfo = parseDelToChannelResponse(messageJson);
        LOG << "Del to channel ok " << responseType.address << " " << channelInfo.titleSha;
        processAddOrDeleteInChannel(responseType.address, channelInfo, false);
    } else if (responseType.method == METHOD::ALL_KEYS_ADDED) {
        LOG << "Keys in folder added ok";
    } else if (responseType.method == METHOD::WANT_TO_TALK) {
        LOG << "Want to talk response";
        invokeCallback(responseType.id, TypedException());
    } else if (responseType.method == METHOD::REQUIRES_PUBKEY) {
        const RequiresPubkeyResponse response = parseRequiresPubkeyResponse(messageJson);
        const auto walFolders = walletFolders[response.address];
        if (walFolders.find(currentWalletFolder) != walFolders.end()) {
            emit javascriptWrapper.requiresPubkeySig(response.address, response.collocutor);
            MessengerRequiresPubkeyVariant event(response.address, response.collocutor);
            events.emplace_back(QVariant::fromValue(event));
        } else {
            LOG << "User change wallet folder. Ignore " << response.address << " " << response.collocutor;
        }
    } else if (responseType.method == METHOD::COLLOCUTOR_ADDED_PUBKEY) {
        const CollocutorAddedPubkeyResponse response = parseCollocutorAddedPubkeyResponse(messageJson);
        const auto walFolders = walletFolders[response.address];
        if (walFolders.find(currentWalletFolder) != walFolders.end()) {
            emit javascriptWrapper.collocutorAddedPubkeySig(response.address, response.collocutor);
            MessengerCollocutorAddedPubkeyVariant event(response.address, response.collocutor);
            events.emplace_back(QVariant::fromValue(event));
        } else {
            LOG << "User change wallet folder. Ignore " << response.address << " " << response.collocutor;
        }
    } else {
        throwErr("Incorrect response type");
    }
END_SLOT_WRAPPER
}

void Messenger::onRegisterAddress(bool isForcibly, const QString &address, const QString &rsaPubkeyHex, const QString &pubkeyAddressHex, const QString &signHex, uint64_t fee, const RegisterAddressCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitErrorCallback([&, this] {
        const QString currPubkey = db.getUserPublicKey(address);
        const bool isNew = currPubkey.isEmpty();
        if (!isNew && !isForcibly) {
            callback.emitFunc(TypedException(), isNew);
            return;
        }
        const size_t idRequest = id.get();
        const QString message = makeRegisterRequest(rsaPubkeyHex, pubkeyAddressHex, signHex, fee, idRequest);
        const auto callbackWrap = [this, callback, isNew, address, pubkeyAddressHex, rsaPubkeyHex, isForcibly](const TypedException &exception) {
            if (!exception.isSet() || isForcibly) { // TODO убрать isForcibly
                LOG << "Set user pubkey " << address << " " << pubkeyAddressHex;
                db.setUserPublicKey(address, pubkeyAddressHex, rsaPubkeyHex, "", "");
                addAddressToMonitored(address);
            }
            callback.emitFunc(exception, isNew);
        };
        callbacks[idRequest] = callbackWrap;
        emit wssClient.sendMessage(message);
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onRegisterAddressFromBlockchain(bool isForcibly, const QString &address, const QString &rsaPubkeyHex, const QString &pubkeyAddressHex, const QString &signHex, uint64_t fee, const QString &txHash, const QString &blockchain, const QString &blockchainName, const Messenger::RegisterAddressBlockchainCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitErrorCallback([&, this] {
        const QString currPubkey = db.getUserPublicKey(address);
        const bool isNew = currPubkey.isEmpty();
        if (!isNew && !isForcibly) {
            callback.emitFunc(TypedException(), isNew);
            return;
        }
        const size_t idRequest = id.get();
        const QString message = makeRegisterBlockchainRequest(pubkeyAddressHex, signHex, fee, txHash, blockchain, blockchainName, idRequest);
        const auto callbackWrap = [this, callback, isNew, address, pubkeyAddressHex, rsaPubkeyHex, isForcibly, txHash, blockchainName](const TypedException &exception) {
            if (!exception.isSet() || isForcibly) { // TODO убрать isForcibly
                LOG << "Set user pubkey2 " << address << " " << pubkeyAddressHex << " " << txHash << " " << blockchainName;
                db.setUserPublicKey(address, pubkeyAddressHex, rsaPubkeyHex, txHash, blockchainName);
                addAddressToMonitored(address);
            }
            callback.emitFunc(exception, isNew);
        };
        callbacks[idRequest] = callbackWrap;
        emit wssClient.sendMessage(message);
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onSignedStrings(const QString &address, const std::vector<QString> &signedHexs, const SignedStringsCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        const std::vector<QString> keys = stringsForSign();
        CHECK(keys.size() == signedHexs.size(), "Incorrect signed strings");

        QJsonArray arrJson;
        for (size_t i = 0; i < keys.size(); i++) {
            const QString &key = keys[i];
            const QString &value = signedHexs[i];

            QJsonObject obj;
            obj.insert("key", key);
            obj.insert("value", value);
            arrJson.push_back(obj);
        }

        const QString arr = QJsonDocument(arrJson).toJson(QJsonDocument::Compact);
        LOG << "Set user signature " << arr.size();
        db.setUserSignatures(address, arr);
        addAddressToMonitored(address);
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onSavePubkeyAddress(bool isForcibly, const QString &address, const QString &pubkeyHex, const QString &signHex, const SavePubkeyCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitErrorCallback([&, this] {
        const QString currSign = db.getContactPublicKey(address);
        const bool isNew = currSign.isEmpty();
        if (!isNew && !isForcibly) {
            callback.emitFunc(TypedException(), isNew);
            return;
        }
        const size_t idRequest = id.get();
        const QString message = makeGetPubkeyRequest(address, pubkeyHex, signHex, idRequest);
        callbacks[idRequest] = std::bind(callback, _1, isNew);
        emit wssClient.sendMessage(message);
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onGetPubkeyAddress(const QString &address, const GetPubkeyAddressCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        const QString pubkey = db.getContactPublicKey(address);
        CHECK_TYPED(!pubkey.isEmpty(), TypeErrors::INCOMPLETE_USER_INFO, "Collocutor pubkey not found " + address.toStdString());
        LOG << "Publickey found " << address << " " << pubkey;
        return pubkey;
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onGetUserInfo(const QString &address, const UserInfoCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        const ContactInfo info = db.getUserInfo(address);
        const bool complete = checkSignsAddress(address);
        return std::make_tuple(complete, info);
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onGetCollocutorInfo(const QString &address, const UserInfoCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        const ContactInfo info = db.getContactInfo(address);
        return std::make_tuple(true, info);
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onSendMessage(const QString &thisAddress, const QString &toAddress, bool isChannel, QString channel, const QString &dataHex, const QString &decryptedDataHex, const QString &pubkeyHex, const QString &signHex, uint64_t fee, uint64_t timestamp, const QString &encryptedDataHex, const SendMessageCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitErrorCallback([&, this] {
        if (!isChannel) {
            channel = "";
        }
        const QString hashMessage = createHashMessage(encryptedDataHex);
        Message::Counter lastCnt = db.getMessageMaxCounter(thisAddress, channel);
        if (lastCnt < 0) {
            lastCnt = -1;
        }
        QString dData;
        if (isDecryptDataSave) {
            dData = decryptedDataHex;
        } else {
            dData = "";
        }
        db.addMessage(thisAddress, toAddress, encryptedDataHex, dData, isDecryptDataSave, timestamp, lastCnt + 1, false, true, false, hashMessage, fee, channel);
        const size_t idRequest = id.get();
        QString message;
        if (!isChannel) {
            message = makeSendMessageRequest(toAddress, dataHex, encryptedDataHex, pubkeyHex, signHex, fee, timestamp, idRequest);
        } else {
            message = makeSendToChannelRequest(channel, dataHex, fee, timestamp, pubkeyHex, signHex, idRequest);
        }
        callbacks[idRequest] = callback;
        emit wssClient.sendMessage(message);
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onGetSavedPos(const QString &address, bool isChannel, const QString &collocutorOrChannel, const GetSavedPosCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        return db.getLastReadCounterForUserContact(address, collocutorOrChannel, isChannel);
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onGetSavedsPos(const QString &address, bool isChannel, const GetSavedsPosCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        if (isChannel) {
            return db.getLastReadCountersForChannels(address);
        } else {
            return db.getLastReadCountersForContacts(address);
        }
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onSavePos(const QString &address, bool isChannel, const QString &collocutorOrChannel, Message::Counter pos, const SavePosCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        db.setLastReadCounterForUserContact(address, collocutorOrChannel, pos, isChannel);
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onGetLastMessage(const QString &address, bool isChannel, QString channel, const GetSavedPosCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        if (!isChannel) {
            channel = "";
        }
        return db.getMessageMaxCounter(address, channel);
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onGetCountMessages(const QString &address, const QString &collocutor, Message::Counter from, const GetCountMessagesCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        return db.getMessagesCountForUserAndDest(address, collocutor, from);
    }, callback, 0);
END_SLOT_WRAPPER
}

void Messenger::onGetHistoryAddress(QString address, Message::Counter from, Message::Counter to, const GetMessagesCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        return db.getMessagesForUser(address, from, to);
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onGetHistoryAddressAddress(QString address, bool isChannel, const QString &collocutorOrChannel, Message::Counter from, Message::Counter to, const GetMessagesCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        return db.getMessagesForUserAndDest(address, collocutorOrChannel, from, to, isChannel);
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onGetHistoryAddressAddressCount(QString address, bool isChannel, const QString &collocutorOrChannel, Message::Counter count, Message::Counter to, const GetMessagesCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        return db.getMessagesForUserAndDestNum(address, collocutorOrChannel, to, count, isChannel);
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onCreateChannel(const QString &address, const QString &title, const QString &titleSha, const QString &pubkeyHex, const QString &signHex, uint64_t fee, const CreateChannelCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitErrorCallback([&, this] {
        const size_t idRequest = id.get();
        const QString message = makeCreateChannelRequest(title, titleSha, fee, pubkeyHex, signHex, idRequest);
        const auto callbackWrap = [this, callback, address, title, titleSha](const TypedException &exception) {
            if (!exception.isSet()) {
                const DBStorage::DbId userId = db.getUserId(address);
                CHECK(userId != DBStorage::not_found, "User not created: " + address.toStdString());
                db.addChannel(userId, title, titleSha, true, address, false, true, true);
                db.setLastReadCounterForUserContact(address, titleSha, -1, true);
            }
            callback.emitFunc(exception);
        };
        callbacks[idRequest] = callbackWrap;
        emit wssClient.sendMessage(message);
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onAddWriterToChannel(const QString &titleSha, const QString &address, const QString &pubkeyHex, const QString &signHex, const AddWriterToChannelCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitErrorCallback([&, this] {
        const size_t idRequest = id.get();
        const QString message = makeChannelAddWriterRequest(titleSha, address, pubkeyHex, signHex, idRequest);
        callbacks[idRequest] = std::bind(callback, _1);
        emit wssClient.sendMessage(message);
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onDelWriterFromChannel(const QString &titleSha, const QString &address, const QString &pubkeyHex, const QString &signHex, const DelWriterToChannelCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitErrorCallback([&, this] {
        const size_t idRequest = id.get();
        const QString message = makeChannelDelWriterRequest(titleSha, address, pubkeyHex, signHex, idRequest);
        callbacks[idRequest] = std::bind(callback, _1);
        emit wssClient.sendMessage(message);
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onGetChannelList(const QString &address, const GetChannelListCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        return db.getChannelsWithLastReadCounters(address);
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onDecryptMessages(const QString &address, const DecryptUserMessagesCallback &callback) {
BEGIN_SLOT_WRAPPER
    if (!isDecryptDataSave) {
        callback.emitCallback();
        return;
    }
    runAndEmitErrorCallback([&, this] {
        const auto notDecryptedMessagesPair = db.getNotDecryptedMessage(address);
        CHECK(notDecryptedMessagesPair.first.size() == notDecryptedMessagesPair.second.size(), "Incorrect db.getNotDecryptedMessage");
        const std::vector<Message> &notDecryptedMessages = notDecryptedMessagesPair.second;
        cryptManager.tryDecryptMessages(notDecryptedMessages, address, CryptographicManager::DecryptMessagesCallback([this, ids=notDecryptedMessagesPair.first, callback](const std::vector<Message> &answer) {
            CHECK(ids.size() == answer.size(), "Incorrect tryDecryptMessages");
            std::vector<std::tuple<MessengerDBStorage::DbId, bool, QString>> result;
            result.reserve(ids.size());
            for (size_t i = 0; i < ids.size(); i++) {
                result.emplace_back(ids[i], answer[i].isDecrypted, answer[i].decryptedDataHex);
            }
            db.updateDecryptedMessage(result);

            LOG << "Decrypted " << result.size() << " messages";

            callback.emitCallback();
        }, [callback](const TypedException &exception) {
            callback.emitException(exception);
        }, std::bind(&Messenger::callbackCall, this, _1), true));
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onIsCompleteUser(const QString &address, const CompleteUserCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        const bool isComplete = checkSignsAddress(address);
        if (!isComplete) {
            LOG << "Uncompleted signs";
        }
        return isComplete;
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onAddAllAddressesInFolder(const QString &folder, const std::vector<QString> &addresses, const AddAllWalletsInFolderCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitCallback([&, this] {
        LOG << "Add address in folder " << folder << " " << addresses.size();
        currentWalletFolder = folder;
        for (const QString &address: addresses) {
            walletFolders[address].insert(folder);
        }

        const QString messageGetMyChannels = makeAddAllKeysRequest(addresses, size_t(-1));
        emit wssClient.addHelloString(messageGetMyChannels, "Messenger");
        emit wssClient.sendMessage(messageGetMyChannels);
        // Get missed messages
        messageRetrieves.clear();
        loginMessagesRetrieveReqs.clear();
        retrievedMissed = 0;
        for (const QString &address: addresses) {
            const QString pubkeyHex = db.getUserPublicKey(address);
            if (pubkeyHex.isEmpty())
                continue;
            const Message::Counter currCounter = db.getMessageMaxConfirmedCounter(address);
            getMessagesFromAddressFromWss(address, currCounter + 1, -1, true);
        }
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onWantToTalk(const QString &address, const QString &pubkey, const QString &sign, const WantToTalkCallback &callback) {
BEGIN_SLOT_WRAPPER
    runAndEmitErrorCallback([&, this] {
        const size_t idRequest = id.get();
        const QString message = makeWantToTalkRequest(address, pubkey, sign, idRequest);
        callbacks[idRequest] = std::bind(callback, _1);
        emit wssClient.sendMessage(message);
    }, callback);
END_SLOT_WRAPPER
}

void Messenger::onReEmit() {
BEGIN_SLOT_WRAPPER
    for (const QVariant &event: events) {
        if (event.canConvert<MessengerDeleteFromChannelVariant>()) {
            const MessengerDeleteFromChannelVariant value = event.value<MessengerDeleteFromChannelVariant>();
            emit javascriptWrapper.deletedFromChannelSig(value.address, value.channelTitle, value.channelTitleSha, value.channelAdmin);
        } else if (event.canConvert<MessengerAddedFromChannelVariant>()) {
            const MessengerAddedFromChannelVariant value = event.value<MessengerAddedFromChannelVariant>();
            emit javascriptWrapper.addedToChannelSig(value.address, value.channelTitle, value.channelTitleSha, value.channelAdmin, value.counter);
        } else if (event.canConvert<MessengerRequiresPubkeyVariant>()) {
            const MessengerRequiresPubkeyVariant value = event.value<MessengerRequiresPubkeyVariant>();
            emit javascriptWrapper.requiresPubkeySig(value.address, value.collocutor);
        } else if (event.canConvert<MessengerCollocutorAddedPubkeyVariant>()) {
            const MessengerCollocutorAddedPubkeyVariant value = event.value<MessengerCollocutorAddedPubkeyVariant>();
            emit javascriptWrapper.collocutorAddedPubkeySig(value.address, value.collocutor);
        } else {
            throwErr("Incorrect type variant");
        }
    }
END_SLOT_WRAPPER
}

}
