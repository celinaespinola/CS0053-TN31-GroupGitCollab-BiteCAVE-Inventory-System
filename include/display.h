#pragma once

#include <string>
#include <vector>
#include "product.h"
#include "supplier.h"
#include "transaction.h"

void displayHeader(
    const std::string& title
);

void displayMainMenu();

void displayProducts(
    const std::vector<Product>& products
);

void displaySuppliers(
    const std::vector<Supplier>& suppliers
);

void displayTransactions(
    const std::vector<Transaction>& transactions
);

void displayMessage(
    const std::string& message
);

void displayError(
    const std::string& message
);