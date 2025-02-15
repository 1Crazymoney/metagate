#ifndef INITIALIZER_JAVASCRIPT_H
#define INITIALIZER_JAVASCRIPT_H

#include <QObject>
#include <functional>

#include "qt_utilites/WrapperJavascript.h"

namespace initializer {

class Initializer;

struct InitState;

class InitializerJavascript: public WrapperJavascript {
    Q_OBJECT

public:
    explicit InitializerJavascript();

    void setInitializerManager(Initializer &initializer) {
        m_initializer = &initializer;
    }

public slots:

    Q_INVOKABLE void resendEvents();

    Q_INVOKABLE void ready(bool force);

    Q_INVOKABLE void getAllTypes();

    Q_INVOKABLE void getAllSubTypes();

signals:

    void stateChangedSig(int number, int totalStates, int numberCritical, int totalCritical, const InitState &state);

    void initializedSig(bool isSuccess, const TypedException &exception);

    void initializedCriticalSig(bool isSuccess, const TypedException &exception);

private slots:

    void onStateChanged(int number, int totalStates, int numberCritical, int totalCritical, const InitState &state);

    void onInitialized(bool isSuccess, const TypedException &exception);

    void onInitializedCritical(bool isSuccess, const TypedException &exception);

private:

    Initializer *m_initializer;
};

}
#endif // INITIALIZER_JAVASCRIPT_H
