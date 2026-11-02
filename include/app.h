#pragma once

#include <vector>
#include "product.h"
#include "supplier.h"
#include "transaction.h"

struct AppState {
    std::vector<Product> products;
    std::vector<Supplier> suppliers;
    std::vector<Transaction> transactions;
};

void runApplication();