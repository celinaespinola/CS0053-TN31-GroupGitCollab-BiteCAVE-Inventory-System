#include "reports.h"
#include "validation.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace {

namespace fs = std::filesystem;

std::string lowerEnglish(std::string text)
{
    for (char& character : text) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character + ('a' - 'A'));
        }
    }
    return text;
}

void validateProduct(const Product& product)
{
    if (product.quantity < 0 || product.lowStockThreshold < 0 ||
        !std::isfinite(product.unitPrice) || product.unitPrice < 0.0) {
        throw std::invalid_argument("Product " + std::to_string(product.productId) +
                                    " has invalid price, quantity or threshold.");
    }
}

std::string statusOf(const Product& product)
{
    if (product.quantity == 0) {
        return "Out of Stock";
    }
    return product.quantity <= product.lowStockThreshold ? "Low Stock" : "In Stock";
}

std::string csvText(const std::string& text)
{
    if (text.find_first_of(",\"\r\n") == std::string::npos) {
        return text;
    }
    std::string escaped = "\"";
    for (char character : text) {
        if (character == '"') {
            escaped += '"';
        }
        escaped += character;
    }
    return escaped + '"';
}

std::string decimal(double value)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(2) << value;
    return output.str();
}

void writeChecked(const fs::path& path, const std::string& contents)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot create temporary report file.");
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.flush();
    if (!output) {
        throw std::runtime_error("Failed while writing a temporary report file.");
    }
    output.close();
    if (!output) {
        throw std::runtime_error("Failed while closing a temporary report file.");
    }
}

fs::path reserveWorkDirectory(const fs::path& parent)
{
    static std::atomic<unsigned long> sequence{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto candidate = parent / (".bitecave-export-" + std::to_string(stamp) +
                                        "-" + std::to_string(sequence++));
        std::error_code error;
        if (fs::create_directory(candidate, error)) {
            return candidate;
        }
        if (error && error != std::errc::file_exists) {
            throw fs::filesystem_error("Cannot create temporary report directory", candidate, error);
        }
    }
    throw std::runtime_error("Cannot reserve a temporary report directory.");
}

void checkDestination(const fs::path& path)
{
    const auto status = fs::symlink_status(path);
    if (fs::exists(status) && !fs::is_regular_file(status)) {
        throw std::runtime_error("Report destination must be a regular file: " + path.u8string());
    }
    for (const auto& component : path.parent_path()) {
        if (lowerEnglish(component.u8string()) == "data") {
            throw std::invalid_argument("Export reports outside persistent data directories.");
        }
    }
}

} // namespace

std::vector<Product> getLowStockProducts(const std::vector<Product>& products)
{
    std::vector<Product> result;
    for (const auto& product : products) {
        validateProduct(product);
        if (product.quantity == 0 || product.quantity <= product.lowStockThreshold) {
            result.push_back(product);
        }
    }
    return result;
}

InventorySummary generateInventorySummary(const std::vector<Product>& products)
{
    if (products.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error("Product count exceeds the summary integer range.");
    }
    InventorySummary result{};
    result.totalProducts = static_cast<int>(products.size());
    long double value = 0.0L;
    for (const auto& product : products) {
        validateProduct(product);
        if (product.quantity > std::numeric_limits<int>::max() - result.totalUnits) {
            throw std::overflow_error("Total units exceed the summary integer range.");
        }
        result.totalUnits += product.quantity;
        value += static_cast<long double>(product.unitPrice) * product.quantity;
        if (!std::isfinite(value) || value > std::numeric_limits<double>::max()) {
            throw std::overflow_error("Inventory value exceeds the summary numeric range.");
        }
        if (product.quantity == 0) {
            ++result.outOfStockCount;
        } else if (product.quantity <= product.lowStockThreshold) {
            ++result.lowStockCount;
        }
    }
    result.inventoryValue = static_cast<double>(value);
    return result;
}

std::vector<Product> sortProducts(
    const std::vector<Product>& products,
    const std::string& criterion,
    bool ascending
)
{
    if (criterion != "name" && criterion != "price" && criterion != "quantity") {
        throw std::invalid_argument("Sorting criterion must be name, price or quantity.");
    }
    for (const auto& product : products) {
        validateProduct(product);
    }
    auto result = products;
    std::stable_sort(result.begin(), result.end(), [&](const Product& first, const Product& second) {
        if (criterion == "name") {
            const auto a = lowerEnglish(first.productName);
            const auto b = lowerEnglish(second.productName);
            return ascending ? a < b : a > b;
        }
        if (criterion == "price") {
            return ascending ? first.unitPrice < second.unitPrice : first.unitPrice > second.unitPrice;
        }
        return ascending ? first.quantity < second.quantity : first.quantity > second.quantity;
    });
    return result;
}

std::vector<Product> filterProductsByCategory(
    const std::vector<Product>& products,
    const std::string& category
)
{
    std::vector<Product> result;
    for (const auto& product : products) {
        if (equalsIgnoreCase(product.category, category)) {
            result.push_back(product);
        }
    }
    return result;
}

bool exportInventoryReport(
    const std::vector<Product>& products,
    const std::vector<Supplier>& suppliers,
    const std::string& filename,
    std::string& error
)
{
    error.clear();
    fs::path work;
    fs::path targets[2];
    bool backedUp[2] = {false, false};
    bool installed[2] = {false, false};
    const auto stagedName = [](int index) {
        return index == 0 ? "products.tmp" : "summary.tmp";
    };
    const auto backupName = [](int index) {
        return index == 0 ? "products.backup" : "summary.backup";
    };

    try {
        if (filename.empty()) {
            throw std::invalid_argument("Specify a product report CSV filename.");
        }
        auto requested = fs::u8path(filename);
        if (lowerEnglish(requested.extension().u8string()) != ".csv") {
            throw std::invalid_argument("Product report filename must end in .csv.");
        }
        targets[0] = fs::weakly_canonical(fs::absolute(requested));
        const auto summaryName = targets[0].filename() == "inventory_report.csv"
            ? fs::path("inventory_summary.csv")
            : fs::u8path(targets[0].stem().u8string() + "_summary.csv");
        targets[1] = targets[0].parent_path() / summaryName;
        for (const auto& target : targets) {
            checkDestination(target);
        }
        if (fs::exists(targets[0]) && fs::exists(targets[1]) &&
            fs::equivalent(targets[0], targets[1])) {
            throw std::invalid_argument("Product and summary reports must be different files.");
        }

        const auto summary = generateInventorySummary(products);
        std::unordered_map<int, const Supplier*> supplierById;
        for (const auto& supplier : suppliers) {
            if (!supplierById.emplace(supplier.supplierId, &supplier).second) {
                throw std::invalid_argument("Duplicate supplier ID: " + std::to_string(supplier.supplierId));
            }
        }
        std::ostringstream productCsv;
        productCsv.imbue(std::locale::classic());
        productCsv << "productId,productName,category,unitPrice,quantity,lowStockThreshold,stockStatus,"
                      "supplierId,supplierName,contactNumber,address\r\n";
        for (const auto& product : products) {
            const auto found = supplierById.find(product.supplierId);
            if (found == supplierById.end()) {
                throw std::invalid_argument("Missing supplier " + std::to_string(product.supplierId) +
                                            " for product " + std::to_string(product.productId) + ".");
            }
            const auto& supplier = *found->second;
            productCsv << product.productId << ',' << csvText(product.productName) << ','
                       << csvText(product.category) << ',' << decimal(product.unitPrice) << ','
                       << product.quantity << ',' << product.lowStockThreshold << ','
                       << statusOf(product) << ',' << product.supplierId << ','
                       << csvText(supplier.supplierName) << ',' << csvText(supplier.contactNumber)
                       << ',' << csvText(supplier.address) << "\r\n";
        }
        std::ostringstream summaryCsv;
        summaryCsv.imbue(std::locale::classic());
        summaryCsv << "totalProducts,totalUnits,inventoryValue,lowStockCount,outOfStockCount\r\n"
                   << summary.totalProducts << ',' << summary.totalUnits << ','
                   << decimal(summary.inventoryValue) << ',' << summary.lowStockCount << ','
                   << summary.outOfStockCount << "\r\n";

        fs::create_directories(targets[0].parent_path());
        work = reserveWorkDirectory(targets[0].parent_path());
        writeChecked(work / stagedName(0), productCsv.str());
        writeChecked(work / stagedName(1), summaryCsv.str());
        // Checked replacement with recovery on ordinary errors, not a crash-atomic transaction.
        for (int index = 0; index < 2; ++index) {
            checkDestination(targets[index]);
            if (fs::exists(targets[index])) {
                fs::rename(targets[index], work / backupName(index));
                backedUp[index] = true;
            }
            fs::rename(work / stagedName(index), targets[index]);
            installed[index] = true;
        }
    } catch (const std::exception& failure) {
        error = std::string("Report export failed: ") + failure.what();
        bool recoveryFailed = false;
        for (int index = 1; index >= 0; --index) {
            std::error_code problem;
            if (installed[index]) {
                fs::remove(targets[index], problem);
                if (problem) {
                    recoveryFailed = true;
                    error += " Could not remove incomplete output: " + targets[index].u8string() + ".";
                    continue;
                }
            }
            if (backedUp[index]) {
                fs::rename(work / backupName(index), targets[index], problem);
                if (problem) {
                    recoveryFailed = true;
                    error += " Backup retained at: " + (work / backupName(index)).u8string() + ".";
                }
            }
        }
        if (!work.empty() && !recoveryFailed) {
            std::error_code cleanup;
            fs::remove_all(work, cleanup);
            if (cleanup) {
                error += " Temporary files remain at: " + work.u8string() + ".";
            }
        }
        return false;
    }
    std::error_code cleanup;
    fs::remove_all(work, cleanup);
    // Both output files are installed. Leftover backups do not negate report success.
    return true;
}
