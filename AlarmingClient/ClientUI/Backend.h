#pragma once
#include <QObject>
#include <QString>
#include <QVariant>
#include <thread>
#include <memory>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include "../base/FuturesClient.h"
#include "QuoteClient.h"
#include "AuthManager.h"
#include "WarningManager.h"

class Backend : public QObject {
    Q_OBJECT
public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend();

    Q_INVOKABLE void login(const QString &username, const QString &password, const QString &brokerId, const QString &frontAddr);
    Q_INVOKABLE void registerUser(const QString &username, const QString &password);
    
    Q_PROPERTY(QStringList contractCodes READ contractCodes NOTIFY contractCodesChanged)
    QStringList contractCodes() const { return filteredContractCodes_; }

    Q_PROPERTY(QVariantMap prices READ prices NOTIFY pricesChanged)
    QVariantMap prices() const { return prices_; }

    Q_INVOKABLE void filterContractCodes(const QString &text);

    // 预警管理接口
    Q_INVOKABLE void addPriceWarning(const QString &symbolText, double maxPrice, double minPrice);
    Q_INVOKABLE void addTimeWarning(const QString &symbolText, const QString &timeStr);
    Q_INVOKABLE void modifyPriceWarning(const QString &orderId, double maxPrice, double minPrice);
    Q_INVOKABLE void modifyTimeWarning(const QString &orderId, const QString &timeStr);
    Q_INVOKABLE void deleteWarning(const QString &orderId);

    // 测试接口
    Q_INVOKABLE void testTriggerAlert(const QString &symbol = "IF2310");

    Q_INVOKABLE void queryWarnings(const QString &statusFilter = "all");
    Q_INVOKABLE void subscribe(const QString &symbol);
    Q_INVOKABLE void setEmail(const QString &email);
    Q_INVOKABLE QString getSavedEmail();

    // Credentials Management
    Q_INVOKABLE void saveCredentials(const QString &username, const QString &password, bool rememberUser, bool autoLogin);
    Q_INVOKABLE QVariantMap loadCredentials(); 
    Q_INVOKABLE void clearCredentials();

    Q_PROPERTY(QVariantList warningList READ warningList NOTIFY warningListChanged)
    Q_PROPERTY(QString username READ username NOTIFY usernameChanged)
    Q_PROPERTY(bool isDebug READ isDebug CONSTANT)
    Q_PROPERTY(QString serverAddress READ serverAddress WRITE setServerAddress NOTIFY serverAddressChanged)
    
    QString username() const;
    QVariantList warningList() const;
    bool isDebug() const {
#ifdef GLOBAL_DEBUG_MODE
        return true;
#else
        return false;
#endif
    }
    QString serverAddress() const { return serverAddress_; }
    void setServerAddress(const QString &addr);

signals:
    void loginSuccess();
    void loginFailed(const QString &message);
    void registerSuccess();
    void registerFailed(const QString &message);
    void showMessage(const QString &message);
    void contractCodesChanged();
    void warningListChanged();
    void pricesChanged();
    void logReceived(const QString &time, const QString &level, const QString &message);
    void usernameChanged();
    void serverAddressChanged();

private:
    void onMessageReceived(const nlohmann::json& j);
    void onPriceUpdated(const QString& symbol, double price);
    void connectToServer();
    std::string extractContractCode(const QString &text);
    QString getContractName(const QString &code);
    QString constructAlertMessage(const nlohmann::json& j);

    QStringList allContractCodes_;
    QStringList filteredContractCodes_;
    QVariantMap prices_;
    QString serverAddress_ = "127.0.0.1"; // Default
    std::map<std::string, std::string> contractMap_; // "Name (Code)" -> "Code"

    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
    std::unique_ptr<FuturesClient> client_;
    QuoteClient* quoteClient_;
    std::thread io_thread_;

    // Managers
    AuthManager* authManager_;
    WarningManager* warningManager_;

    // 记录当前正在进行的请求类型 (用于分发响应)
    std::string current_request_type_;
};
