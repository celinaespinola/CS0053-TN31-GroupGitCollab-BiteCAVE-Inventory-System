#pragma once

#include <string>
#include <vector>
#include "product.h"
#include "supplier.h"

struct InventorySummary {
    int totalProducts;
    int totalUnits;
    double inventoryValue;
    int lowStockCount;
    int outOfStockCount;
};

std::vector<Product> getLowStockProducts(
    const std::vector<Product>& products
);

InventorySummary generateInventorySummary(
    const std::vector<Product>& products
);

std::vector<Product> sortProducts(
    const std::vector<Product>& products,
    const std::string& criterion,
    bool ascending
);

std::vector<Product> filterProductsByCategory(
    const std::vector<Product>& products,
    const std::string& category
);

bool exportInventoryReport(
    const std::vector<Product>& products,
    const std::vector<Supplier>& suppliers,
    const std::string& filename,
    std::string& error
);