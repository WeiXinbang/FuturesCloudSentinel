#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QSettings>
#include <QCryptographicHash>
#include <nlohmann/json.hpp>
#include "../base/FuturesClient.h"

class AuthManager : public QObject {
    Q_OBJECT
public:
    explicit AuthManager(QObject *parent = nullptr);
    void setClient(FuturesClient* client);
    
    void login(const QString &username, const QString &password);
    void registerUser(const QString &username, const QString &password);
    
    void saveCredentials(const QString &username, const QString &password, bool rememberUser, bool autoLogin);
    QVariantMap loadCredentials();
    void clearCredentials();
    
    QString currentUsername() const { return currentUsername_; }
    void setCurrentUsername(const QString& username) { 
        if (currentUsername_ != username) {
            currentUsername_ = username;
            emit usernameChanged();
        }
    }

signals:
    void loginSuccess();
    void loginFailed(const QString &message);
    void registerSuccess();
    void registerFailed(const QString &message);
    void usernameChanged();

public:
    void handleResponse(const std::string& type, bool success, const std::string& message, const nlohmann::json& j);

private:
    FuturesClient* client_ = nullptr;
    QString currentUsername_;
};
