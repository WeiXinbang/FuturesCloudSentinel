#include "WarningManager.h"
#include "ContractData.h"

WarningManager::WarningManager(QObject *parent) : QObject(parent) {}

void WarningManager::setClient(FuturesClient* client) {
    client_ = client;
}

void WarningManager::setCurrentUsername(const QString& username) {
    currentUsername_ = username;
}

void WarningManager::addPriceWarning(const QString &symbol, double maxPrice, double minPrice) {
    if (!client_) {
        emit operationResult(false, "Not connected to server");
        return;
    }
    if (currentUsername_.isEmpty()) {
        emit operationResult(false, "Please login first");
        return;
    }
    // Note: symbol here is expected to be the code, not "Name (Code)"
    // The caller (Backend) should have already extracted the code.
    client_->add_price_warning(currentUsername_.toStdString(), symbol.toStdString(), maxPrice, minPrice);
}

void WarningManager::addTimeWarning(const QString &symbol, const QString &timeStr) {
    if (!client_) {
        emit operationResult(false, "Not connected to server");
        return;
    }
    if (currentUsername_.isEmpty()) {
        emit operationResult(false, "Please login first");
        return;
    }
    client_->add_time_warning(currentUsername_.toStdString(), symbol.toStdString(), timeStr.toStdString());
}

void WarningManager::modifyPriceWarning(const QString &orderId, double maxPrice, double minPrice) {
    if (client_) {
        client_->modify_price_warning(orderId.toStdString(), maxPrice, minPrice);
    }
}

void WarningManager::modifyTimeWarning(const QString &orderId, const QString &timeStr) {
    if (client_) {
        client_->modify_time_warning(orderId.toStdString(), timeStr.toStdString());
    }
}

void WarningManager::deleteWarning(const QString &orderId) {
    if (client_) {
        client_->delete_warning(orderId.toStdString());
    }
}

void WarningManager::queryWarnings(const QString &statusFilter) {
    if (client_ && !currentUsername_.isEmpty()) {
        client_->query_warnings(currentUsername_.toStdString(), statusFilter.toStdString());
    }
}

void WarningManager::handleResponse(const std::string& type, bool success, const std::string& message, const nlohmann::json& j) {
    if (type == "query_warnings") {
        if (success) {
            if (j.contains("data") && j["data"].is_array()) {
                warningList_.clear();
                QStringList symbolsToSubscribe;
                for (const auto& item : j["data"]) {
                    QVariantMap map;
                    map.insert("order_id", QString::fromStdString(item.value("order_id", "")));
                    map.insert("symbol", QString::fromStdString(item.value("symbol", "")));
                    
                    std::string wType = item.value("type", "");
                    if (wType.empty()) wType = item.value("warning_type", "");
                    map.insert("type", QString::fromStdString(wType));

                    // Add Contract Name
                    std::string symbol = item.value("symbol", "");
                    QString qSymbol = QString::fromStdString(symbol);
                    if (!symbolsToSubscribe.contains(qSymbol)) {
                        symbolsToSubscribe.append(qSymbol);
                    }
                    QString contractName = qSymbol; // Default to code
                    
                    for (const auto& entry : RAW_CONTRACT_DATA) {
                        if (entry.code == symbol) {
                            contractName = QString::fromStdString(entry.name);
                            break;
                        }
                    }
                    map.insert("contract_name", contractName);

                    if (item.contains("max_price")) map.insert("max_price", item["max_price"].get<double>());
                    if (item.contains("min_price")) map.insert("min_price", item["min_price"].get<double>());
                    if (item.contains("trigger_time")) map.insert("trigger_time", QString::fromStdString(item["trigger_time"].get<std::string>()));
                    warningList_.append(QVariant(map));
                }
                emit warningListChanged();
                emit subscribeRequest(symbolsToSubscribe);
            }
        } else {
            emit operationResult(false, "Query failed: " + QString::fromStdString(message));
        }
    } else if (type == "add_warning") {
        if (success) {
            emit operationResult(true, "Warning added successfully");
            queryWarnings(); 
        } else {
            emit operationResult(false, "Failed to add warning: " + QString::fromStdString(message));
        }
    } else if (type == "modify_warning") {
        if (success) {
            emit operationResult(true, "Warning modified successfully");
            queryWarnings();
        } else {
            emit operationResult(false, "Failed to modify warning: " + QString::fromStdString(message));
        }
    } else if (type == "delete_warning") {
        if (success) {
            emit operationResult(true, "Warning deleted successfully");
            queryWarnings();
        } else {
            emit operationResult(false, "Failed to delete warning: " + QString::fromStdString(message));
        }
    }
}
