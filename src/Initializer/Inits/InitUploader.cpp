﻿#include "InitUploader.h"

#include "Uploader.h"

#include "check.h"
#include "qt_utilites/SlotWrapper.h"
#include "qt_utilites/QRegister.h"

SET_LOG_NAMESPACE("INIT");

namespace initializer {

QString InitUploader::stateName() {
    return "uploader";
}

InitUploader::InitUploader(QThread *mainThread, Initializer &manager)
    : InitInterface(stateName(), mainThread, manager, true)
{
    Q_CONNECT(this, &InitUploader::checkedUpdatesHtmls, this, &InitUploader::onCheckedUpdatesHtmls);

    registerStateType("init", "uploader initialized", true, true);
    registerStateType("check_updates_htmls", "uploader check updates", false, false, 30s, "uploader check updates");
}

InitUploader::~InitUploader() = default;

void InitUploader::completeImpl() {
    CHECK(uploader != nullptr, "uploader not initialized");
}

void InitUploader::sendInitSuccess(const TypedException &exception) {
    sendState("init", false, exception);
}

void InitUploader::onCheckedUpdatesHtmls(const TypedException &exception) {
BEGIN_SLOT_WRAPPER
    sendState("check_updates_htmls", false, exception);
END_SLOT_WRAPPER
}

InitUploader::Return InitUploader::initialize(SharedFuture<MainWindow> mainWindow, SharedFuture<auth::Auth> auth) {
    const TypedException exception = apiVrapper2([&, this] {
        uploader = std::make_unique<Uploader>(auth.get(), mainWindow.get());
        Q_CONNECT(uploader.get(), &Uploader::checkedUpdatesHtmls, this, &InitUploader::checkedUpdatesHtmls);
        uploader->start();
    });
    sendInitSuccess(exception);
    if (exception.isSet()) {
        throw exception;
    }
    return uploader.get();
}

}
