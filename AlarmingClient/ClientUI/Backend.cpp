#include "Backend.h"
#include "ContractData.h"
#include <QDebug>
#include <QCoreApplication>
#include <QTime>

Backend::Backend(QObject *parent) : QObject(parent) {
    // Initialize Managers
    authManager_ = new AuthManager(this);
    warningManager_ = new WarningManager(this);

    // Connect AuthManager signals
    connect(authManager_, &AuthManager::loginSuccess, this, &Backend::loginSuccess);
    connect(authManager_, &AuthManager::loginFailed, this, &Backend::loginFailed);
    connect(authManager_, &AuthManager::registerSuccess, this, &Backend::registerSuccess);
    connect(authManager_, &AuthManager::registerFailed, this, &Backend::registerFailed);
    connect(authManager_, &AuthManager::usernameChanged, this, &Backend::usernameChanged);
    connect(authManager_, &AuthManager::usernameChanged, [this](){
        warningManager_->setCurrentUsername(authManager_->currentUsername());
    });

    // Connect WarningManager signals
    connect(warningManager_, &WarningManager::warningListChanged, this, &Backend::warningListChanged);
    connect(warningManager_, &WarningManager::operationResult, [this](bool success, const QString& message){
        emit showMessage(message);
    });
    connect(warningManager_, &WarningManager::subscribeRequest, [this](const QStringList& symbols){
        if (quoteClient_) {
            quoteClient_->subscribe(symbols);
        }
    });

    // Load contract codes from hardcoded data
    allContractCodes_.clear();
    contractMap_.clear();
    
    for (const auto& entry : RAW_CONTRACT_DATA) {
        // 构造显示文本: "中文名 (代码)"
        QString displayText = QString("%1 (%2)").arg(entry.name, entry.code);
        allContractCodes_.append(displayText);
        
        // 存入映射表: "中文名 (代码)" -> "代码"
        contractMap_[displayText.toStdString()] = entry.code;
        contractMap_[entry.code] = entry.code; 
    }
    filteredContractCodes_ = allContractCodes_;
    emit contractCodesChanged();

    // Initialize QuoteClient
    quoteClient_ = new QuoteClient(this);
    connect(quoteClient_, &QuoteClient::priceUpdated, this, &Backend::onPriceUpdated);
    // Connect to CTP (using demo address for now)
    quoteClient_->connectToCtp("tcp://182.254.243.31:40011");

    work_guard_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(io_context_.get_executor());
    
    connectToServer();
}

void Backend::connectToServer() {
    if (client_) {
        client_->close();
        client_.reset();
    }

    tcp::resolver resolver(io_context_);
    try {
        auto endpoints = resolver.resolve(serverAddress_.toStdString(), "8888");
        client_ = std::make_unique<FuturesClient>(io_context_, endpoints);
        
        client_->set_message_callback([this](const nlohmann::json& j) {
            this->onMessageReceived(j);
        });

        // Update managers with new client
        authManager_->setClient(client_.get());
        warningManager_->setClient(client_.get());

        if (!io_thread_.joinable()) {
            io_thread_ = std::thread([this]() {
                io_context_.run();
            });
        }
    } catch (std::exception& e) {
        qCritical() << "Failed to initialize client:" << e.what();
    }
}

void Backend::setServerAddress(const QString &addr) {
    if (serverAddress_ != addr) {
        serverAddress_ = addr;
        emit serverAddressChanged();
        connectToServer();
    }
}

Backend::~Backend() {
    if (client_) {
        client_->close();
    }
    if (work_guard_) {
        work_guard_->reset();
    }
    io_context_.stop();
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
}

// --- Delegated Methods ---

void Backend::login(const QString &username, const QString &password, const QString &brokerId, const QString &frontAddr) {
    current_request_type_ = "login";
    authManager_->login(username, password);
}

void Backend::registerUser(const QString &username, const QString &password) {
    current_request_type_ = "register";
    authManager_->registerUser(username, password);
}

void Backend::saveCredentials(const QString &username, const QString &password, bool rememberUser, bool autoLogin) {
    authManager_->saveCredentials(username, password, rememberUser, autoLogin);
}

QVariantMap Backend::loadCredentials() {
    return authManager_->loadCredentials();
}

void Backend::clearCredentials() {
    authManager_->clearCredentials();
}

QString Backend::username() const {
    return authManager_->currentUsername();
}

void Backend::addPriceWarning(const QString &symbolText, double maxPrice, double minPrice) {
    current_request_type_ = "add_warning";
    std::string symbol = extractContractCode(symbolText);
    warningManager_->addPriceWarning(QString::fromStdString(symbol), maxPrice, minPrice);
}

void Backend::addTimeWarning(const QString &symbolText, const QString &timeStr) {
    current_request_type_ = "add_warning";
    std::string symbol = extractContractCode(symbolText);
    warningManager_->addTimeWarning(QString::fromStdString(symbol), timeStr);
}

void Backend::modifyPriceWarning(const QString &orderId, double maxPrice, double minPrice) {
    current_request_type_ = "modify_warning";
    warningManager_->modifyPriceWarning(orderId, maxPrice, minPrice);
}

void Backend::modifyTimeWarning(const QString &orderId, const QString &timeStr) {
    current_request_type_ = "modify_warning";
    warningManager_->modifyTimeWarning(orderId, timeStr);
}

void Backend::deleteWarning(const QString &orderId) {
    current_request_type_ = "delete_warning";
    warningManager_->deleteWarning(orderId);
}

void Backend::queryWarnings(const QString &statusFilter) {
    current_request_type_ = "query_warnings";
    warningManager_->queryWarnings(statusFilter);
}

QVariantList Backend::warningList() const {
    return warningManager_->warningList();
}

// --- Other Methods ---

void Backend::filterContractCodes(const QString &text) {
    if (text.isEmpty()) {
        filteredContractCodes_ = allContractCodes_;
    } else {
        filteredContractCodes_.clear();
        for (const QString &code : allContractCodes_) {
            if (code.contains(text, Qt::CaseInsensitive)) {
                filteredContractCodes_.append(code);
            }
        }
    }
    emit contractCodesChanged();
}

void Backend::onPriceUpdated(const QString& symbol, double price) {
    prices_[symbol] = price;
    emit pricesChanged();
}

void Backend::subscribe(const QString &symbol) {
    if (quoteClient_ && !symbol.isEmpty()) {
        quoteClient_->subscribe(QStringList() << symbol);
    }
}

void Backend::setEmail(const QString &email) {
    if (client_) {
        current_request_type_ = "set_email";
        client_->set_email(email.toStdString());
    }
    // Save locally
    QSettings settings("FuturesCloudSentinel", "AlarmingClient");
    settings.setValue("saved_email", email);
}

QString Backend::getSavedEmail() {
    QSettings settings("FuturesCloudSentinel", "AlarmingClient");
    return settings.value("saved_email", "").toString();
}

std::string Backend::extractContractCode(const QString &text) {
    std::string key = text.trimmed().toStdString();
    auto it = contractMap_.find(key);
    if (it != contractMap_.end()) {
        return it->second;
    }
    int start = text.lastIndexOf('(');
    int end = text.lastIndexOf(')');
    if (start != -1 && end != -1 && end > start) {
        return text.mid(start + 1, end - start - 1).trimmed().toStdString();
    }
    return key;
}

QString Backend::getContractName(const QString &code) {
    for (const auto& entry : RAW_CONTRACT_DATA) {
        if (entry.code == code.toStdString()) {
            return QString::fromStdString(entry.name);
        }
    }
    return code;
}

QString Backend::constructAlertMessage(const nlohmann::json& j) {
    std::string symbol = j.value("symbol", "");
    double currentPrice = j.value("current_price", 0.0);
    std::string warningType = j.value("warning_type", "price"); // price or time
    
    QString contractName = getContractName(QString::fromStdString(symbol));
    
    QString msg;
    if (warningType == "price") {
        double threshold = j.value("threshold", 0.0); 
        std::string condition = j.value("condition", ""); // e.g. "max" or "min" or ">="
        
        msg = QString("价格预警触发！\n合约: %1 (%2)\n当前价格: %3\n触发条件: %4 %5")
                .arg(contractName)
                .arg(QString::fromStdString(symbol))
                .arg(currentPrice)
                .arg(QString::fromStdString(condition))
                .arg(threshold);
    } else if (warningType == "time") {
        std::string timeStr = j.value("target_time", "");
        msg = QString("时间预警触发！\n合约: %1 (%2)\n设定时间: %3")
                .arg(contractName)
                .arg(QString::fromStdString(symbol))
                .arg(QString::fromStdString(timeStr));
    } else {
        // Fallback
            msg = QString("预警触发！\n合约: %1 (%2)\n当前价格: %3")
                .arg(contractName)
                .arg(QString::fromStdString(symbol))
                .arg(currentPrice);
    }
    return msg;
}

void Backend::onMessageReceived(const nlohmann::json& j) {
    std::string type = j.value("type", "");
    
    if (type == "response") {
        bool success = j.value("success", false);
        std::string message = j.value("message", "");
        
        std::string requestType = current_request_type_;
        current_request_type_ = ""; 

        if (requestType == "login" || requestType == "register") {
            authManager_->handleResponse(requestType, success, message, j);
        } else if (requestType == "query_warnings" || requestType == "add_warning" || 
                   requestType == "modify_warning" || requestType == "delete_warning") {
            warningManager_->handleResponse(requestType, success, message, j);
        } else if (requestType == "set_email") {
            if (success) {
                emit showMessage("Email set successfully");
            } else {
                emit showMessage("Failed to set email: " + QString::fromStdString(message));
            }
        } else {
            if (!success || !message.empty()) {
                 emit showMessage(QString::fromStdString(message));
            }
        }
    } else if (type == "alert_triggered") {
        // Use new message construction logic
        QString qMsg = constructAlertMessage(j);
        
        std::string symbol = j.value("symbol", "Unknown");
        double price = j.value("current_price", 0.0);
        
        QString logDetail = QString("[%1] %2 (Price: %3)").arg("ALERT", qMsg).arg(price);

        emit showMessage(qMsg);
        emit logReceived(QTime::currentTime().toString("HH:mm:ss"), "ALERT", logDetail);
    } else if (type == "login_response") {
        // Legacy support
        bool success = j.value("success", false);
        std::string message = j.value("message", "");
        authManager_->handleResponse("login", success, message, j);
    } else if (type == "register_response") {
        bool success = j.value("success", false);
        std::string message = j.value("message", "");
        authManager_->handleResponse("register", success, message, j);
    }
}

void Backend::testTriggerAlert(const QString &symbol) {
#ifdef GLOBAL_DEBUG_MODE
    if (client_) {
        std::string code = symbol.isEmpty() ? "IF2310" : symbol.toStdString();
        QString detailMsg = QString("Alert Triggered: %1 Price >= 9999.0").arg(QString::fromStdString(code));
        client_->simulate_alert_trigger(code, 9999.0, detailMsg.toStdString());
    }
#endif
}
