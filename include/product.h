#pragma once

#include <string>

struct Product {
    int productId;
    std::string productName;
    std::string category;
    double unitPrice;
    int quantity;
    int lowStockThreshold;
    int supplierId;
};