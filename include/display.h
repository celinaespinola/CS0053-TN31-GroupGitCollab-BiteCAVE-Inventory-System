#pragma once

#include <string>
#include <vector>
#include "product.h"
#include "supplier.h"
#include "transaction.h"

inline constexpr int defaultDisplayWidth = 96;

void beginScreen(
    const std::string& title,
    int width = defaultDisplayWidth
);

void displayCenteredText(
    const std::string& text
);

// Labels have no embedded numbers. This prints aligned selectors and a footer.
// Two columns read downward on the left, then downward on the right.
void displayMenuOptions(
    const std::vector<std::string>& options,
    int columns = 1
);

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
