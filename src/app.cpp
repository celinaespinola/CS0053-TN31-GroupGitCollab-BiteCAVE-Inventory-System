#include "app.h"

#include "authentication.h"
#include "display.h"
#include "inventory.h"
#include "reports.h"
#include "storage.h"
#include "suppliers.h"
#include "transactions.h"
#include "validation.h"

#include <conio.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr const char* USERS_FILE = "data/users.dat";
constexpr const char* PRODUCTS_FILE = "data/inventory.csv";
constexpr const char* SUPPLIERS_FILE = "data/suppliers.csv";
constexpr const char* TRANSACTIONS_FILE = "data/transactions.csv";
constexpr const char* REPORT_FILE = "reports/inventory_report.csv";

void message(const std::string& text)
{
    displayMessage(text);
}

void errorMessage(const std::string& text)
{
    displayError(text);
}

void waitForEnter()
{
    std::cout << "\nPress Enter to continue..." << std::flush;
    std::string ignored;
    std::getline(std::cin, ignored);
}

std::optional<int> inputId(const std::string& prompt)
{
    return readInt(prompt, 1, std::numeric_limits<int>::max());
}

std::optional<int> inputQuantity(const std::string& prompt)
{
    return readInt(prompt, 0, std::numeric_limits<int>::max());
}

bool readPassword(std::string& password)
{
    password.clear();
    std::cout << "Password: " << std::flush;

    while (true) {
        const int key = _getch();

        if (key == '\r') {
            std::cout << '\n';
            return true;
        }

        if (key == 3 || key == 26) {
            password.clear();
            std::cout << '\n';
            return false;
        }

        if (key == 0 || key == 224) {
            (void)_getch();
            continue;
        }

        if (key == '\b') {
            if (!password.empty()) {
                password.pop_back();
            }
            continue;
        }

        if (key >= 32 && key <= 126) {
            password.push_back(static_cast<char>(key));
        }
    }
}

bool loadData(AppState& state)
{
    std::string error;

    const auto productStatus =
        loadProducts(state.products, PRODUCTS_FILE, error);

    if (productStatus == LoadStatus::Error) {
        errorMessage(error);
        return false;
    }

    const auto supplierStatus =
        loadSuppliers(state.suppliers, SUPPLIERS_FILE, error);

    if (supplierStatus == LoadStatus::Error) {
        errorMessage(error);
        return false;
    }

    const auto transactionStatus =
        loadTransactions(state.transactions, TRANSACTIONS_FILE, error);

    if (transactionStatus == LoadStatus::Error) {
        errorMessage(error);
        return false;
    }

    return true;
}

bool prepareAdministrator(std::vector<User>& users)
{
    std::string error;

    if (!fs::exists(USERS_FILE)) {
        beginScreen("First-Time Administrator Setup");

        if (!initializeAdministrator(USERS_FILE, error)) {
            errorMessage(error);
            return false;
        }
    }

    if (!loadUsers(users, USERS_FILE, error)) {
        errorMessage(error);
        return false;
    }

    return true;
}

bool login(const std::vector<User>& users)
{
    for (int attempt = 1; attempt <= 3; ++attempt) {
        beginScreen("Administrator Login");

        const auto username = readNonEmptyString("Username: ");

        if (!username) {
            return false;
        }

        std::string password;

        if (!readPassword(password)) {
            return false;
        }

        const bool isAuthenticated = authenticateUser(
            users, *username, password
        );

        std::fill(password.begin(), password.end(), '\0');
        password.clear();

        if (isAuthenticated) {
            message("Login successful.");
            return true;
        }

        errorMessage(
            "Invalid credentials. Attempt " +
            std::to_string(attempt) + " of 3."
        );

        waitForEnter();
    }

    errorMessage("Maximum login attempts reached.");
    return false;
}

bool supplierExists(const AppState& state, int id)
{
    return std::any_of(
        state.suppliers.begin(),
        state.suppliers.end(),
        [id](const Supplier& supplier) {
            return supplier.supplierId == id;
        }
    );
}

bool saveProductChanges(
    AppState& state,
    std::vector<Product>& updatedProducts
)
{
    std::string error;

    if (!saveProducts(updatedProducts, PRODUCTS_FILE, error)) {
        errorMessage(error);
        return false;
    }

    state.products = std::move(updatedProducts);
    message("Inventory changes saved successfully.");
    return true;
}

bool saveSupplierChanges(
    AppState& state,
    std::vector<Supplier>& updatedSuppliers
)
{
    std::string error;

    if (!saveSuppliers(updatedSuppliers, SUPPLIERS_FILE, error)) {
        errorMessage(error);
        return false;
    }

    state.suppliers = std::move(updatedSuppliers);
    message("Supplier changes saved successfully.");
    return true;
}

std::string currentDateTime()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time =
        std::chrono::system_clock::to_time_t(now);

    std::tm local{};

    if (localtime_s(&local, &time) != 0) {
        return "";
    }

    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

// F01 - Add New Product
void addProductScreen(AppState& state)
{
    beginScreen("Add New Product");

    const auto id = inputId("Product ID: ");
    if (!id) return;

    const auto name = readNonEmptyString("Product name: ");
    if (!name) return;

    const auto category = readNonEmptyString("Category: ");
    if (!category) return;

    const auto price = readDouble("Unit price: ", 0.0);
    if (!price) return;

    const auto quantity = inputQuantity("Initial quantity: ");
    if (!quantity) return;

    const auto threshold = inputQuantity("Low-stock threshold: ");
    if (!threshold) return;

    displaySuppliers(state.suppliers);

    const auto supplierId = inputId("Supplier ID: ");
    if (!supplierId) return;

    if (!supplierExists(state, *supplierId)) {
        errorMessage("Supplier does not exist.");
        return;
    }

    const Product product{
        *id, *name, *category, *price,
        *quantity, *threshold, *supplierId
    };

    auto updated = state.products;
    std::string error;

    if (!addProduct(updated, product, error)) {
        errorMessage(error);
        return;
    }

    saveProductChanges(state, updated);
}

// F02 - View All Products
void viewProductsScreen(const AppState& state)
{
    beginScreen("View All Products", 120);
    displayProducts(state.products);
}

// F03 - Search Product
void searchProductScreen(const AppState& state)
{
    beginScreen("Search Product", 120);

    displayMenuOptions({
        "Search by Product ID",
        "Search by Product Name",
        "Back"
    });

    const auto choice = readInt("Select option: ", 1, 3);

    if (!choice || *choice == 3) {
        return;
    }

    if (*choice == 1) {
        const auto id = inputId("Product ID: ");
        if (!id) return;

        const auto result = findProductById(
            state.products, *id
        );

        if (result) {
            displayProducts({*result});
        } else {
            message("Product not found.");
        }
    } else {
        const auto name = readNonEmptyString("Product name: ");
        if (!name) return;

        const auto results = searchProductsByName(
            state.products, *name
        );

        displayProducts(results);
    }
}

// F04 - Update Product Details
void updateProductScreen(AppState& state)
{
    beginScreen("Update Product", 120);

    const auto id = inputId("Product ID: ");
    if (!id) return;

    const auto existing = findProductById(
        state.products, *id
    );

    if (!existing) {
        errorMessage("Product not found.");
        return;
    }

    displayProducts({*existing});

    displayMenuOptions({
        "Product Name",
        "Category",
        "Unit Price",
        "Low-Stock Threshold",
        "Supplier",
        "Back"
    });

    const auto choice = readInt(
        "Select field to update: ", 1, 6
    );

    if (!choice || *choice == 6) {
        return;
    }

    Product updatedProduct = *existing;

    switch (*choice) {
        case 1: {
            const auto value = readNonEmptyString(
                "New product name: "
            );
            if (!value) return;

            updatedProduct.productName = *value;
            break;
        }

        case 2: {
            const auto value = readNonEmptyString(
                "New category: "
            );
            if (!value) return;

            updatedProduct.category = *value;
            break;
        }

        case 3: {
            const auto value = readDouble(
                "New unit price: ", 0.0
            );
            if (!value) return;

            updatedProduct.unitPrice = *value;
            break;
        }

        case 4: {
            const auto value = inputQuantity(
                "New low-stock threshold: "
            );
            if (!value) return;

            updatedProduct.lowStockThreshold = *value;
            break;
        }

        case 5: {
            displaySuppliers(state.suppliers);

            const auto value = inputId(
                "New supplier ID: "
            );
            if (!value) return;

            if (!supplierExists(state, *value)) {
                errorMessage("Supplier does not exist.");
                return;
            }

            updatedProduct.supplierId = *value;
            break;
        }

        default:
            return;
    }

    auto updated = state.products;
    std::string error;

    if (!updateProduct(
            updated, *id, updatedProduct, error
        )) {
        errorMessage(error);
        return;
    }

    saveProductChanges(state, updated);
}

// F05 - Delete Product
void deleteProductScreen(AppState& state)
{
    beginScreen("Delete Product", 120);

    const auto id = inputId("Product ID: ");
    if (!id) return;

    const auto existing = findProductById(
        state.products, *id
    );

    if (!existing) {
        errorMessage("Product not found.");
        return;
    }

    displayProducts({*existing});

    const auto confirmed = confirmAction(
        "Delete this product? (Y/N): "
    );

    if (!confirmed || !*confirmed) {
        message("Deletion cancelled.");
        return;
    }

    auto updated = state.products;
    std::string error;

    if (!deleteProduct(updated, *id, error)) {
        errorMessage(error);
        return;
    }

    saveProductChanges(state, updated);
}

// F06 and F07 - Stock In / Stock Out
void stockMovementScreen(AppState& state, bool incoming)
{
    beginScreen(incoming ? "Stock In" : "Stock Out", 120);

    const auto id = inputId("Product ID: ");
    if (!id) return;

    const auto existing = findProductById(
        state.products, *id
    );

    if (!existing) {
        errorMessage("Product not found.");
        return;
    }

    displayProducts({*existing});

    const auto quantity = readInt(
        "Quantity: ",
        1,
        std::numeric_limits<int>::max()
    );

    if (!quantity) return;

    auto updatedProducts = state.products;
    auto updatedTransactions = state.transactions;

    std::string error;

    const bool changed = incoming
        ? stockIn(updatedProducts, *id, *quantity, error)
        : stockOut(updatedProducts, *id, *quantity, error);

    if (!changed) {
        errorMessage(error);
        return;
    }

    const auto updatedProduct = findProductById(
        updatedProducts, *id
    );

    if (!updatedProduct) {
        errorMessage("Updated product could not be found.");
        return;
    }

    const int transactionId = generateTransactionId(
        updatedTransactions
    );

    if (transactionId <= 0) {
        errorMessage("Unable to generate transaction ID.");
        return;
    }

    const double amount = incoming
        ? 0.0
        : existing->unitPrice * *quantity;

    if (!std::isfinite(amount)) {
        errorMessage("Transaction amount exceeds numeric limits.");
        return;
    }

    const std::string timestamp = currentDateTime();

    if (timestamp.empty()) {
        errorMessage("Unable to generate transaction timestamp.");
        return;
    }

    const Transaction transaction{
        transactionId,
        *id,
        existing->productName,
        incoming ? "STOCK_IN" : "STOCK_OUT",
        timestamp,
        *quantity,
        existing->quantity,
        updatedProduct->quantity,
        amount
    };

    if (!addTransaction(
            updatedTransactions, transaction, error
        )) {
        errorMessage(error);
        return;
    }

    // Separate CSV files do not provide crash-atomic transactions.
    // Save transaction history first, then inventory.
    if (!saveTransactions(
            updatedTransactions,
            TRANSACTIONS_FILE,
            error
        )) {
        errorMessage(error);
        return;
    }

    if (!saveProducts(
            updatedProducts,
            PRODUCTS_FILE,
            error
        )) {
        errorMessage(
            "Inventory save failed: " + error
        );

        std::string rollbackError;

        if (!saveTransactions(
                state.transactions,
                TRANSACTIONS_FILE,
                rollbackError
            )) {
            errorMessage(
                "Transaction restoration failed: " +
                rollbackError
            );

            throw std::runtime_error(
                "Stock files may be inconsistent. "
                "Stop operations and inspect saved data."
            );
        }

        return;
    }

    state.products = std::move(updatedProducts);
    state.transactions = std::move(updatedTransactions);

    message("Stock movement recorded and saved.");
}

// F08 - Low-Stock Monitoring
void lowStockScreen(const AppState& state)
{
    beginScreen("Low-Stock Monitoring", 120);

    displayProducts(
        getLowStockProducts(state.products)
    );
}

// F09 - Inventory Summary Report
void summaryScreen(const AppState& state)
{
    beginScreen("Inventory Summary");

    const auto summary = generateInventorySummary(
        state.products
    );

    message(
        "Total products: " +
        std::to_string(summary.totalProducts)
    );

    message(
        "Total units: " +
        std::to_string(summary.totalUnits)
    );

    std::ostringstream value;

    value << std::fixed
          << std::setprecision(2)
          << summary.inventoryValue;

    message("Inventory value: PHP " + value.str());

    message(
        "Low-stock products: " +
        std::to_string(summary.lowStockCount)
    );

    message(
        "Out-of-stock products: " +
        std::to_string(summary.outOfStockCount)
    );
}

// F10 - Sort Products
void sortProductsScreen(const AppState& state)
{
    beginScreen("Sort Products");

    displayMenuOptions({
        "Name",
        "Price",
        "Quantity",
        "Back"
    });

    const auto choice = readInt(
        "Sort by: ", 1, 4
    );

    if (!choice || *choice == 4) {
        return;
    }

    displayMenuOptions({
        "Ascending",
        "Descending"
    });

    const auto direction = readInt(
        "Order: ", 1, 2
    );

    if (!direction) return;

    const std::vector<std::string> criteria{
        "name", "price", "quantity"
    };

    const auto sorted = sortProducts(
        state.products,
        criteria[static_cast<std::size_t>(*choice - 1)],
        *direction == 1
    );

    beginScreen("Sorted Products", 120);
    displayProducts(sorted);
}

// F11 - Filter Products by Category
void filterCategoryScreen(const AppState& state)
{
    beginScreen("Filter Products by Category");

    std::vector<std::string> categories;

    for (const auto& product : state.products) {
        const bool found = std::any_of(
            categories.begin(),
            categories.end(),
            [&](const std::string& category) {
                return equalsIgnoreCase(
                    category, product.category
                );
            }
        );

        if (!found) {
            categories.push_back(product.category);
        }
    }

    if (categories.empty()) {
        message("No categories available.");
        return;
    }

    displayMenuOptions(categories);

    const auto choice = readInt(
        "Select category: ",
        1,
        static_cast<int>(categories.size())
    );

    if (!choice) return;

    const auto filtered = filterProductsByCategory(
        state.products,
        categories[static_cast<std::size_t>(*choice - 1)]
    );

    beginScreen("Filtered Products", 120);
    displayProducts(filtered);
}

// F12 - Transaction History
void transactionHistoryScreen(const AppState& state)
{
    beginScreen("Transaction History", 120);

    displayTransactions(
        getTransactionHistory(state.transactions)
    );
}

// F13 - Export Inventory Report
void exportReportScreen(const AppState& state)
{
    beginScreen("Export Inventory Report");

    std::string error;

    if (exportInventoryReport(
            state.products,
            state.suppliers,
            REPORT_FILE,
            error
        )) {
        message("Inventory report exported successfully.");
        message(REPORT_FILE);
        message("reports/inventory_summary.csv");
    } else {
        errorMessage(error);
    }
}

// F14 - Supplier Management
void supplierMenu(AppState& state)
{
    while (true) {
        beginScreen("Supplier Management");

        displayMenuOptions({
            "Add Supplier",
            "View Suppliers",
            "Search Supplier",
            "Update Supplier",
            "Delete Supplier",
            "Back"
        });

        const auto choice = readInt(
            "Select option: ", 1, 6
        );

        if (!choice || *choice == 6) {
            return;
        }

        // View Suppliers
        if (*choice == 2) {
            displaySuppliers(state.suppliers);
            waitForEnter();
            continue;
        }

        // Search Supplier by ID or Name
        if (*choice == 3) {
            beginScreen("Search Supplier");

            displayMenuOptions({
                "Search by Supplier ID",
                "Search by Supplier Name",
                "Back"
            });

            const auto searchChoice = readInt(
                "Select option: ", 1, 3
            );

            if (!searchChoice || *searchChoice == 3) {
                continue;
            }

            if (*searchChoice == 1) {
                const auto id = inputId("Supplier ID: ");
                if (!id) return;

                const auto found = std::find_if(
                    state.suppliers.begin(),
                    state.suppliers.end(),
                    [&](const Supplier& supplier) {
                        return supplier.supplierId == *id;
                    }
                );

                if (found == state.suppliers.end()) {
                    message("Supplier not found.");
                } else {
                    displaySuppliers({*found});
                }
            } else {
                const auto name = readNonEmptyString(
                    "Supplier name: "
                );

                if (!name) return;

                displaySuppliers(
                    searchSuppliersByName(
                        state.suppliers, *name
                    )
                );
            }

            waitForEnter();
            continue;
        }

        auto updated = state.suppliers;
        std::string error;

        // Add Supplier
        if (*choice == 1) {
            const auto id = inputId("Supplier ID: ");
            if (!id) return;

            const auto name = readNonEmptyString(
                "Supplier name: "
            );
            if (!name) return;

            const auto contact = readNonEmptyString(
                "Contact number: "
            );
            if (!contact) return;

            const auto address = readNonEmptyString(
                "Address: "
            );
            if (!address) return;

            const Supplier supplier{
                *id, *name, *contact, *address
            };

            if (!addSupplier(updated, supplier, error)) {
                errorMessage(error);
            } else {
                saveSupplierChanges(state, updated);
            }
        }

        // Update Supplier
        if (*choice == 4) {
            const auto id = inputId("Supplier ID: ");
            if (!id) return;

            const auto found = std::find_if(
                updated.begin(),
                updated.end(),
                [&](const Supplier& supplier) {
                    return supplier.supplierId == *id;
                }
            );

            if (found == updated.end()) {
                errorMessage("Supplier not found.");
                waitForEnter();
                continue;
            }

            const auto name = readNonEmptyString(
                "New supplier name: "
            );
            if (!name) return;

            const auto contact = readNonEmptyString(
                "New contact: "
            );
            if (!contact) return;

            const auto address = readNonEmptyString(
                "New address: "
            );
            if (!address) return;

            const Supplier replacement{
                *id, *name, *contact, *address
            };

            if (!updateSupplier(
                    updated, *id, replacement, error
                )) {
                errorMessage(error);
            } else {
                saveSupplierChanges(state, updated);
            }
        }

        // Delete Supplier
        if (*choice == 5) {
            const auto id = inputId("Supplier ID: ");
            if (!id) return;

            const auto confirmed = confirmAction(
                "Delete supplier? (Y/N): "
            );

            if (!confirmed || !*confirmed) {
                message("Deletion cancelled.");
                waitForEnter();
                continue;
            }

            if (!deleteSupplier(
                    updated,
                    state.products,
                    *id,
                    error
                )) {
                errorMessage(error);
            } else {
                saveSupplierChanges(state, updated);
            }
        }

        waitForEnter();
    }
}

// Main Feature Dispatcher
void handleFeatureSelection(int choice, AppState& state)
{
    switch (choice) {
        case 1:
            addProductScreen(state);
            break;

        case 2:
            viewProductsScreen(state);
            break;

        case 3:
            searchProductScreen(state);
            break;

        case 4:
            updateProductScreen(state);
            break;

        case 5:
            deleteProductScreen(state);
            break;

        case 6:
            stockMovementScreen(state, true);
            break;

        case 7:
            stockMovementScreen(state, false);
            break;

        case 8:
            lowStockScreen(state);
            break;

        case 9:
            summaryScreen(state);
            break;

        case 10:
            sortProductsScreen(state);
            break;

        case 11:
            filterCategoryScreen(state);
            break;

        case 12:
            transactionHistoryScreen(state);
            break;

        case 13:
            exportReportScreen(state);
            break;

        case 14:
            supplierMenu(state);
            break;

        default:
            errorMessage("Invalid feature selection.");
            break;
    }
}

// Final Save Check
bool finalSave(const AppState& state)
{
    std::string error;

    if (!saveProducts(
            state.products, PRODUCTS_FILE, error
        )) {
        errorMessage(error);
        return false;
    }

    if (!saveSuppliers(
            state.suppliers, SUPPLIERS_FILE, error
        )) {
        errorMessage(error);
        return false;
    }

    if (!saveTransactions(
            state.transactions,
            TRANSACTIONS_FILE,
            error
        )) {
        errorMessage(error);
        return false;
    }

    return true;
}

} // namespace

// Main Application Controller
void runApplication()
{
    AppState state;
    std::vector<User> users;

    if (!loadData(state)) {
        throw std::runtime_error(
            "Application data could not be loaded."
        );
    }

    if (!prepareAdministrator(users)) {
        throw std::runtime_error(
            "Administrator setup failed."
        );
    }

    while (true) {
        if (!login(users)) {
            return;
        }

        while (true) {
            beginScreen("Main Menu");
            displayMainMenu();

            const auto choice = readInt(
                "Select an option (1-16): ", 1, 16
            );

            if (!choice) return;

            if (*choice == 15) {
                message("Logged out successfully.");
                break;
            }

            if (*choice == 16) {
                if (!finalSave(state)) {
                    errorMessage(
                        "Final save failed. Exit cancelled."
                    );

                    waitForEnter();
                    continue;
                }

                message("Inventory saved. Goodbye!");
                return;
            }

            try {
                handleFeatureSelection(*choice, state);
            }
            catch (const std::exception& exception) {
                errorMessage(exception.what());

                // A stock persistence failure may leave CSV files
                // inconsistent. Stop rather than continuing blindly.
                throw;
            }

            waitForEnter();
        }
    }
}