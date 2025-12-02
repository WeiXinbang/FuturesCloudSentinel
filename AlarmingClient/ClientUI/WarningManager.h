#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include <nlohmann/json.hpp>
#include "../base/FuturesClient.h"

class WarningManager : public QObject {
    Q_OBJECT
public:
    explicit WarningManager(QObject *parent = nullptr);
    void setClient(FuturesClient* client);
    void setCurrentUsername(const QString& username);

    void addPriceWarning(const QString &symbol, double maxPrice, double minPrice);
    void addTimeWarning(const QString &symbol, const QString &timeStr);
    void modifyPriceWarning(const QString &orderId, double maxPrice, double minPrice);
    void modifyTimeWarning(const QString &orderId, const QString &timeStr);
    void deleteWarning(const QString &orderId);
    void queryWarnings(const QString &statusFilter = "all");

    QVariantList warningList() const { return warningList_; }

signals:
    void warningListChanged();
    void operationResult(bool success, const QString& message);
    void subscribeRequest(const QStringList& symbols);

public:
    void handleResponse(const std::string& type, bool success, const std::string& message, const nlohmann::json& j);

private:
    FuturesClient* client_ = nullptr;
    QString currentUsername_;
    QVariantList warningList_;
};
