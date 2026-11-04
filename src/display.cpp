#include "display.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace {

int requestedWidth = defaultDisplayWidth;

int screenWidth()
{
    int width = requestedWidth;
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        const int columns = info.srWindow.Right - info.srWindow.Left + 1;
        width = std::min(width, std::max(5, columns - 1));
    }
#else
    winsize size{};
    if (isatty(STDOUT_FILENO) &&
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0) {
        width = std::min(width, std::max(5, static_cast<int>(size.ws_col) - 1));
    }
#endif
    return width;
}

void clearScreen()
{
    std::cout.flush();
#ifdef _WIN32
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD originalMode = 0;
    if (!GetConsoleMode(output, &originalMode)) {
        return; // Keep redirected files and automated test logs as plain text.
    }

    // Clear both the terminal display and old screen history, then home again.
    // Write directly to the console instead of the C++ output stream.
    if (SetConsoleMode(output, originalMode | ENABLE_PROCESSED_OUTPUT |
                                      ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        const wchar_t sequence[] = L"\x1b[H\x1b[2J\x1b[3J\x1b[H";
        const DWORD length = static_cast<DWORD>(sizeof(sequence) / sizeof(sequence[0]) - 1);
        DWORD written = 0;
        const BOOL cleared = WriteConsoleW(output, sequence, length, &written, nullptr);
        SetConsoleMode(output, originalMode);
        if (cleared && written == length) {
            return;
        }
    }

    // Classic-console fallback: clear the whole buffer, not only srWindow.
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(output, &info)) {
        const COORD home{0, 0};
        const DWORD cells = static_cast<DWORD>(info.dwSize.X) *
                            static_cast<DWORD>(info.dwSize.Y);
        DWORD written = 0;
        const BOOL textCleared = FillConsoleOutputCharacterW(
            output, L' ', cells, home, &written
        );
        const bool completeText = textCleared && written == cells;
        const BOOL attributesCleared = FillConsoleOutputAttribute(
            output, info.wAttributes, cells, home, &written
        );
        if (completeText && attributesCleared && written == cells &&
            SetConsoleCursorPosition(output, home)) {
            return;
        }
    }
    std::cerr << "[Display] Screen clearing failed. Windows error: "
              << GetLastError() << '\n';
#else
    if (isatty(STDOUT_FILENO)) {
        std::cout << "\x1b[H\x1b[2J\x1b[3J\x1b[H" << std::flush;
    }
#endif
}

std::string printableText(const std::string& text)
{
    std::string result;
    for (unsigned char character : text) {
        // Prevent embedded line breaks, tabs and escape codes breaking frames.
        result += (character < 32 || character == 127)
            ? ' ' : static_cast<char>(character);
    }
    return result;
}

std::vector<std::string> wrapText(const std::string& text, int width)
{
    std::string remaining = printableText(text);
    std::vector<std::string> lines;
    const auto limit = static_cast<std::size_t>(std::max(1, width));
    while (remaining.size() > limit) {
        auto cut = remaining.rfind(' ', limit);
        if (cut == std::string::npos || cut == 0) {
            cut = limit;
        }
        lines.push_back(remaining.substr(0, cut));
        remaining.erase(0, cut);
        if (!remaining.empty() && remaining.front() == ' ') {
            remaining.erase(0, 1);
        }
    }
    if (!remaining.empty() || lines.empty()) {
        lines.push_back(remaining);
    }
    return lines;
}

void border(int width, char character = '-', bool header = false)
{
    const char corner = header ? '+' : '|';
    std::cout << corner << std::string(static_cast<std::size_t>(width - 2), character)
              << corner << '\n';
}

void textRow(const std::string& text, int width, bool centered = false)
{
    const int inside = width - 4;
    for (const auto& line : wrapText(text, inside)) {
        const auto remaining = static_cast<std::size_t>(inside) - line.size();
        const auto left = centered ? remaining / 2 : 0;
        std::cout << "| " << std::string(left, ' ') << line
                  << std::string(remaining - left, ' ') << " |\n";
    }
}

// Presentation only: preserve the beginning and final four characters.
std::string productNameCell(const std::string& name, int width)
{
    const std::string text = printableText(name);
    const auto capacity = static_cast<std::size_t>(width);
    if (text.size() <= capacity) {
        return text;
    }
    // Keep UTF-8 character boundaries intact while respecting the byte-sized cells.
    std::vector<std::size_t> boundaries;
    for (std::size_t index = 0; index < text.size(); ++index) {
        if ((static_cast<unsigned char>(text[index]) & 0xC0) != 0x80) {
            boundaries.push_back(index);
        }
    }
    const auto suffixStart = boundaries.size() >= 4
        ? boundaries[boundaries.size() - 4] : std::size_t{0};
    const auto suffix = text.substr(suffixStart);
    if (capacity < suffix.size() + 3) {
        return text; // Product-table name cells reserve at least 19 bytes.
    }
    const auto prefixBudget = capacity - suffix.size() - 3;
    std::size_t prefixEnd = 0;
    for (const auto boundary : boundaries) {
        if (boundary > prefixBudget) break;
        prefixEnd = boundary;
    }
    return text.substr(0, prefixEnd) + "..." + suffix;
}

std::string money(double value)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "PHP " << std::fixed << std::setprecision(2) << value;
    return output.str();
}

void tableRow(
    const std::vector<std::string>& cells,
    const std::vector<int>& widths,
    const std::vector<bool>& rightAligned = {}
)
{
    std::vector<std::vector<std::string>> wrapped;
    std::size_t height = 1;
    for (std::size_t column = 0; column < cells.size(); ++column) {
        wrapped.push_back(wrapText(cells[column], widths[column]));
        height = std::max(height, wrapped.back().size());
    }
    for (std::size_t row = 0; row < height; ++row) {
        std::cout << '|';
        for (std::size_t column = 0; column < cells.size(); ++column) {
            const std::string value = row < wrapped[column].size()
                ? wrapped[column][row] : "";
            const std::string padding(
                static_cast<std::size_t>(widths[column]) - value.size(), ' '
            );
            const bool right = column < rightAligned.size() && rightAligned[column];
            std::cout << ' ' << (right ? padding + value : value + padding) << " |";
        }
        std::cout << '\n';
    }
}

std::string stockStatus(const Product& product)
{
    if (product.quantity == 0) {
        return "Out of Stock";
    }
    return product.quantity <= product.lowStockThreshold ? "Low Stock" : "In Stock";
}

} // namespace

void beginScreen(const std::string& title, int width)
{
    requestedWidth = std::clamp(width, 20, 240);
    clearScreen();
    displayHeader(title);
}

void displayCenteredText(const std::string& text)
{
    textRow(text, screenWidth(), true);
}

void displayHeader(const std::string& title)
{
    const int width = screenWidth();
    border(width, '=', true);
    textRow("BiteCAVE Inventory System", width, true);
    textRow(title, width, true);
    border(width, '=', true);
}

void displayMenuOptions(const std::vector<std::string>& options, int columns)
{
    const int width = screenWidth();
    const int inside = width - 4;
    columns = columns >= 2 && width >= 80 ? 2 : 1;
    const int gap = columns == 2 ? 4 : 0;
    const int cellWidth = (inside - gap) / columns;
    const std::size_t rows = (options.size() + columns - 1) / columns;
    textRow("", width);
    for (std::size_t row = 0; row < rows; ++row) {
        std::vector<std::vector<std::string>> cells;
        std::size_t height = 1;
        for (int column = 0; column < columns; ++column) {
            const auto index = row + static_cast<std::size_t>(column) * rows;
            std::ostringstream label;
            if (index < options.size()) {
                label << "[ " << std::setw(2) << index + 1 << " ] " << options[index];
            }
            cells.push_back(wrapText(label.str(), cellWidth));
            height = std::max(height, cells.back().size());
        }
        for (std::size_t line = 0; line < height; ++line) {
            std::string combined;
            for (int column = 0; column < columns; ++column) {
                const auto& cell = cells[static_cast<std::size_t>(column)];
                std::string value = line < cell.size() ? cell[line] : "";
                value.append(static_cast<std::size_t>(cellWidth) - value.size(), ' ');
                if (column > 0) combined.append(static_cast<std::size_t>(gap), ' ');
                combined += value;
            }
            combined.append(static_cast<std::size_t>(inside) - combined.size(), ' ');
            // Preserve intentional column padding; do not normalize this row.
            std::cout << "| " << combined << " |\n";
        }
    }
    textRow("", width);
    border(width, '=');
}

void displayMainMenu()
{
    const std::vector<std::string> choices = {
        "Add New Product", "View All Products", "Search Product",
        "Update Product Details", "Delete Product", "Stock In / Restock",
        "Stock Out / Record Sale", "View Low-Stock Products",
        "Inventory Summary Report", "Sort Products", "Filter Products by Category",
        "View Transaction History", "Export Inventory Report",
        "Supplier Management", "User Account / Logout", "Save and Exit"
    };
    displayMenuOptions(choices, 2);
}

void displayProducts(const std::vector<Product>& products)
{
    const int width = screenWidth();
    if (products.empty()) {
        textRow("No products to display.", width, true);
        border(width, '=');
        return;
    }

    // Eight columns: keep every product's numeric fields on its own data row.
    std::vector<int> columns = {6, 25, 12, 12, 8, 9, 11, 12};
    for (const auto& product : products) {
        columns[0] = std::max(columns[0], static_cast<int>(std::to_string(product.productId).size()));
        columns[3] = std::max(columns[3], static_cast<int>(money(product.unitPrice).size()));
        columns[4] = std::max(columns[4], static_cast<int>(std::to_string(product.quantity).size()));
        columns[5] = std::max(columns[5], static_cast<int>(std::to_string(product.lowStockThreshold).size()));
        columns[6] = std::max(columns[6], static_cast<int>(std::to_string(product.supplierId).size()));
    }
    const int fixed = columns[0] + columns[3] + columns[4] + columns[5] + columns[6] + columns[7];
    constexpr int separators = 25;
    constexpr int minimumName = 19;
    constexpr int minimumCategory = 8;
    const int required = fixed + minimumName + minimumCategory + separators;
    if (width < required) {
        textRow("A one-row product table needs at least " + std::to_string(required) +
                " columns. Use a wider product screen to see all fields without shortening numbers.", width);
        border(width, '=');
        return;
    }
    columns[2] = std::min(12, width - fixed - minimumName - separators);
    columns[1] = width - fixed - columns[2] - separators;
    tableRow({"ID", "Product name", "Category", "Unit price", "Qty", "Min stock", "Supplier ID", "Status"}, columns);
    border(width, '=');
    for (const auto& product : products) {
        std::string category = printableText(product.category);
        if (category.size() > static_cast<std::size_t>(columns[2])) {
            std::size_t end = static_cast<std::size_t>(columns[2] - 3);
            while (end > 0 && (static_cast<unsigned char>(category[end]) & 0xC0) == 0x80) {
                --end;
            }
            category = category.substr(0, end) + "...";
        }
        tableRow({std::to_string(product.productId), productNameCell(product.productName, columns[1]),
                  category, money(product.unitPrice), std::to_string(product.quantity),
                  std::to_string(product.lowStockThreshold), std::to_string(product.supplierId),
                  stockStatus(product)}, columns,
                 {true, false, false, true, true, true, true, false});
    }
    border(width, '=');
}

void displaySuppliers(const std::vector<Supplier>& suppliers)
{
    const int width = screenWidth();
    if (suppliers.empty()) {
        textRow("No suppliers to display.", width, true);
        border(width, '=');
        return;
    }
    const int nameWidth = (width - 38) / 2;
    const std::vector<int> columns = {10, nameWidth, 15, width - 38 - nameWidth};
    if (width >= 100) {
        tableRow({"ID", "Supplier name", "Contact number", "Address"}, columns);
        border(width, '=');
    }
    for (const auto& supplier : suppliers) {
        if (width >= 100) {
            tableRow({std::to_string(supplier.supplierId), supplier.supplierName,
                      supplier.contactNumber, supplier.address}, columns,
                     {true, false, false, false});
        } else {
            textRow("Supplier " + std::to_string(supplier.supplierId) + ": " + supplier.supplierName, width);
            textRow("Contact: " + supplier.contactNumber, width);
            textRow("Address: " + supplier.address, width);
        }
        border(width);
    }
    border(width, '=');
}

void displayTransactions(const std::vector<Transaction>& transactions)
{
    const int width = screenWidth();
    if (transactions.empty()) {
        textRow("No transactions to display.", width, true);
        border(width, '=');
        return;
    }
    const std::vector<int> columns = {8, 19, 9, 10, 10, 10, 10, width - 101};
    if (width >= 116) {
        tableRow({"Txn ID", "Date and time", "Type", "Product ID", "Qty change",
                  "Previous", "Updated", "Amount"}, columns);
        border(width, '=');
    }
    for (const auto& transaction : transactions) {
        if (width >= 116) {
            tableRow({std::to_string(transaction.transactionId), transaction.dateTime,
                      transaction.transactionType, std::to_string(transaction.productId),
                      std::to_string(transaction.quantityChanged),
                      std::to_string(transaction.previousQuantity),
                      std::to_string(transaction.updatedQuantity),
                      money(transaction.transactionAmount)}, columns,
                     {true, false, false, true, true, true, true, true});
            textRow("Product: " + transaction.productName, width);
        } else {
            textRow("Transaction " + std::to_string(transaction.transactionId) +
                    " | " + transaction.dateTime + " | " + transaction.transactionType, width);
            textRow("Product " + std::to_string(transaction.productId) + ": " + transaction.productName, width);
            textRow("Quantity change: " + std::to_string(transaction.quantityChanged) +
                    " | Previous: " + std::to_string(transaction.previousQuantity) +
                    " | Updated: " + std::to_string(transaction.updatedQuantity), width);
            textRow("Amount: " + money(transaction.transactionAmount), width);
        }
        border(width);
    }
    border(width, '=');
}

void displayMessage(const std::string& message)
{
    const int width = screenWidth();
    textRow(message, width);
}

void displayError(const std::string& message)
{
    const int width = screenWidth();
    textRow("[ERROR] " + message, width);
}
