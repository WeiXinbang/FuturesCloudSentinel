#include "AuthManager.h"

AuthManager::AuthManager(QObject *parent) : QObject(parent) {}

void AuthManager::setClient(FuturesClient* client) {
    client_ = client;
}

void AuthManager::login(const QString &username, const QString &password) {
    if (client_) {
        setCurrentUsername(username);
        QString hashedPassword = QString(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
        client_->login(username.toStdString(), hashedPassword.toStdString());
    } else {
        emit loginFailed("Not connected to server");
    }
}

void AuthManager::registerUser(const QString &username, const QString &password) {
    if (client_) {
        // Note: We don't set currentUsername here usually, but we can if we want to pre-fill
        QString hashedPassword = QString(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
        client_->register_user(username.toStdString(), hashedPassword.toStdString());
    } else {
        emit registerFailed("Not connected to server");
    }
}

void AuthManager::handleResponse(const std::string& type, bool success, const std::string& message, const nlohmann::json& j) {
    if (type == "login") {
        if (success) {
            emit loginSuccess();
        } else {
            emit loginFailed(QString::fromStdString(message));
        }
    } else if (type == "register") {
        if (success) {
            emit registerSuccess();
        } else {
            emit registerFailed(QString::fromStdString(message));
        }
    }
}

void AuthManager::saveCredentials(const QString &username, const QString &password, bool rememberUser, bool autoLogin) {
    QSettings settings("FuturesCloudSentinel", "AlarmingClient");
    settings.setValue("rememberUser", rememberUser);
    settings.setValue("autoLogin", autoLogin);
    
    if (rememberUser) {
        settings.setValue("username", username);
        if (autoLogin) {
            QByteArray pwdBytes = password.toUtf8().toBase64();
            settings.setValue("password", pwdBytes);
        } else {
            settings.remove("password");
        }
    } else {
        settings.remove("username");
        settings.remove("password");
    }
}

QVariantMap AuthManager::loadCredentials() {
    QSettings settings("FuturesCloudSentinel", "AlarmingClient");
    QVariantMap map;
    bool rememberUser = settings.value("rememberUser", false).toBool();
    bool autoLogin = settings.value("autoLogin", false).toBool();
    
    map.insert("rememberUser", rememberUser);
    map.insert("autoLogin", autoLogin);
    
    if (rememberUser) {
        map.insert("username", settings.value("username").toString());
        if (autoLogin) {
            QByteArray pwdBytes = settings.value("password").toByteArray();
            QString password = QString::fromUtf8(QByteArray::fromBase64(pwdBytes));
            map.insert("password", password);
        }
    }
    return map;
}

void AuthManager::clearCredentials() {
    QSettings settings("FuturesCloudSentinel", "AlarmingClient");
    settings.remove("username");
    settings.remove("password");
    settings.setValue("rememberUser", false);
    settings.setValue("autoLogin", false);
}
