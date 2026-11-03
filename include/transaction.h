#pragma once

#include <string>

struct Transaction {
    int transactionId;
    int productId;
    std::string productName;
    std::string transactionType;
    std::string dateTime;
    int quantityChanged;
    int previousQuantity;
    int updatedQuantity;
    double transactionAmount;
};