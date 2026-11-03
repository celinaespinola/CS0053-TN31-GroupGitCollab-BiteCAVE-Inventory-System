#pragma once

#include <string>
#include <vector>
#include "product.h"
#include "supplier.h"
#include "transaction.h"

enum class LoadStatus {
    Success,
    FileNotFound,
    Error
};

LoadStatus loadProducts(
    std::vector<Product>& products,
    const std::string& filename,
    std::string& error
);

bool saveProducts(
    const std::vector<Product>& products,
    const std::string& filename,
    std::string& error
);

LoadStatus loadSuppliers(
    std::vector<Supplier>& suppliers,
    const std::string& filename,
    std::string& error
);

bool saveSuppliers(
    const std::vector<Supplier>& suppliers,
    const std::string& filename,
    std::string& error
);

LoadStatus loadTransactions(
    std::vector<Transaction>& transactions,
    const std::string& filename,
    std::string& error
);

bool saveTransactions(
    const std::vector<Transaction>& transactions,
    const std::string& filename,
    std::string& error
);