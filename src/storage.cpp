#include "storage.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <system_error>
#include <unordered_set>

namespace {

namespace fs = std::filesystem;

constexpr const char* PRODUCT_HEADER =
    "productId,productName,category,unitPrice,quantity,lowStockThreshold,supplierId";
constexpr const char* SUPPLIER_HEADER =
    "supplierId,supplierName,contactNumber,address";
constexpr const char* TRANSACTION_HEADER =
    "transactionId,productId,productName,transactionType,dateTime,quantityChanged,previousQuantity,updatedQuantity,transactionAmount";

bool hasText(const std::string& value)
{
    return std::any_of(value.begin(), value.end(), [](unsigned char ch) {
        return ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n';
    });
}

bool parseInt(const std::string& text, int& result)
{
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool parseMoney(const std::string& text, double& result)
{
    if (text.empty()) return false;
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    input >> std::noskipws >> result;
    return input.eof() && !input.fail() && std::isfinite(result);
}

bool validDateTime(const std::string& value)
{
    if (value.size() != 19 || value[4] != '-' || value[7] != '-' ||
        value[10] != ' ' || value[13] != ':' || value[16] != ':') return false;
    const int digits[] = {0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18};
    for (int position : digits) {
        if (value[position] < '0' || value[position] > '9') return false;
    }
    const int year = std::stoi(value.substr(0, 4));
    const int month = std::stoi(value.substr(5, 2));
    const int day = std::stoi(value.substr(8, 2));
    const int hour = std::stoi(value.substr(11, 2));
    const int minute = std::stoi(value.substr(14, 2));
    const int second = std::stoi(value.substr(17, 2));
    if (year < 1 || month < 1 || month > 12 || hour > 23 || minute > 59 || second > 59)
        return false;
    constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    return day >= 1 && day <= days[month - 1] + (month == 2 && leap ? 1 : 0);
}

bool valid(const Product& product)
{
    return product.productId > 0 && hasText(product.productName) &&
           hasText(product.category) && std::isfinite(product.unitPrice) &&
           product.unitPrice >= 0.0 && product.quantity >= 0 &&
           product.lowStockThreshold >= 0 && product.supplierId > 0;
}

bool valid(const Supplier& supplier)
{
    return supplier.supplierId > 0 && hasText(supplier.supplierName) &&
           hasText(supplier.contactNumber) && hasText(supplier.address);
}

bool valid(const Transaction& transaction)
{
    if (transaction.transactionId <= 0 || transaction.productId <= 0 ||
        !hasText(transaction.productName) || !validDateTime(transaction.dateTime) ||
        transaction.quantityChanged <= 0 || transaction.previousQuantity < 0 ||
        transaction.updatedQuantity < 0 || !std::isfinite(transaction.transactionAmount))
        return false;
    const long long previous = transaction.previousQuantity;
    const long long changed = transaction.quantityChanged;
    if (transaction.transactionType == "STOCK_IN")
        return previous + changed == transaction.updatedQuantity &&
               transaction.transactionAmount == 0.0;
    if (transaction.transactionType == "STOCK_OUT")
        return previous - changed == transaction.updatedQuantity &&
               transaction.transactionAmount >= 0.0;
    return false;
}

// Returns false for any malformed CSV; quoted fields may span physical lines.
bool readRow(std::istream& input, std::vector<std::string>& fields, bool& found)
{
    fields.clear();
    std::string field;
    enum class State { Start, Plain, Quoted, Closed } state = State::Start;
    found = false;
    for (;;) {
        const int next = input.get();
        if (next == EOF) {
            if (input.bad() || (input.fail() && !input.eof()) || state == State::Quoted)
                return false;
            if (!found) return true;
            fields.push_back(field);
            return true;
        }
        found = true;
        const char ch = static_cast<char>(next);
        if (state == State::Quoted) {
            if (ch == '"') state = State::Closed;
            else field += ch;
            continue;
        }
        if (state == State::Closed && ch == '"') {
            field += '"';
            state = State::Quoted;
            continue;
        }
        if (ch == ',' || ch == '\n' || ch == '\r') {
            fields.push_back(field);
            field.clear();
            state = State::Start;
            if (ch == ',') continue;
            if (ch == '\r' && input.peek() == '\n') input.get();
            return true;
        }
        if (state == State::Closed) return false;
        if (ch == '"') {
            if (state != State::Start) return false;
            state = State::Quoted;
        } else {
            field += ch;
            state = State::Plain;
        }
    }
}

std::string escape(const std::string& value)
{
    if (value.find_first_of(",\"\r\n") == std::string::npos) return value;
    std::string escaped = "\"";
    for (char ch : value) {
        if (ch == '"') escaped += '"';
        escaped += ch;
    }
    return escaped + '"';
}

std::string money(double value)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(2) << value;
    return output.str();
}

bool decode(const std::vector<std::string>& fields, Product& product)
{
    if (fields.size() != 7 || !parseInt(fields[0], product.productId) ||
        !parseMoney(fields[3], product.unitPrice) ||
        !parseInt(fields[4], product.quantity) ||
        !parseInt(fields[5], product.lowStockThreshold) ||
        !parseInt(fields[6], product.supplierId)) return false;
    product.productName = fields[1];
    product.category = fields[2];
    return valid(product);
}

bool decode(const std::vector<std::string>& fields, Supplier& supplier)
{
    if (fields.size() != 4 || !parseInt(fields[0], supplier.supplierId)) return false;
    supplier.supplierName = fields[1];
    supplier.contactNumber = fields[2];
    supplier.address = fields[3];
    return valid(supplier);
}

bool decode(const std::vector<std::string>& fields, Transaction& transaction)
{
    if (fields.size() != 9 || !parseInt(fields[0], transaction.transactionId) ||
        !parseInt(fields[1], transaction.productId) ||
        !parseInt(fields[5], transaction.quantityChanged) ||
        !parseInt(fields[6], transaction.previousQuantity) ||
        !parseInt(fields[7], transaction.updatedQuantity) ||
        !parseMoney(fields[8], transaction.transactionAmount)) return false;
    transaction.productName = fields[2];
    transaction.transactionType = fields[3];
    transaction.dateTime = fields[4];
    return valid(transaction);
}

int idOf(const Product& value) { return value.productId; }
int idOf(const Supplier& value) { return value.supplierId; }
int idOf(const Transaction& value) { return value.transactionId; }

template <typename Record>
LoadStatus load(std::vector<Record>& destination, const std::string& filename,
                const char* header, std::string& error)
{
    const fs::path path(filename);
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        if (ec) {
            error = "Cannot inspect file '" + filename + "': " + ec.message();
            return LoadStatus::Error;
        }
        error.clear();
        return LoadStatus::FileNotFound;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "Cannot read file '" + filename + "'.";
        return LoadStatus::Error;
    }
    std::vector<std::string> fields;
    bool found = false;
    std::istringstream expectedHeader(header);
    std::vector<std::string> expectedFields;
    bool expectedFound = false;
    readRow(expectedHeader, expectedFields, expectedFound);
    if (!readRow(input, fields, found) || !found || fields != expectedFields) {
        error = "Invalid CSV header in '" + filename + "'.";
        return LoadStatus::Error;
    }
    std::vector<Record> parsed;
    std::unordered_set<int> ids;
    while (true) {
        if (!readRow(input, fields, found)) {
            error = "Malformed CSV record in '" + filename + "'.";
            return LoadStatus::Error;
        }
        if (!found) break;
        Record record{};
        if (!decode(fields, record) || !ids.insert(idOf(record)).second) {
            error = "Invalid or duplicate CSV record in '" + filename + "'.";
            return LoadStatus::Error;
        }
        parsed.push_back(std::move(record));
    }
    destination.swap(parsed);
    error.clear();
    return LoadStatus::Success;
}

void write(std::ostream& output, const Product& value)
{
    output << value.productId << ',' << escape(value.productName) << ','
           << escape(value.category) << ',' << money(value.unitPrice) << ','
           << value.quantity << ',' << value.lowStockThreshold << ','
           << value.supplierId << '\n';
}

void write(std::ostream& output, const Supplier& value)
{
    output << value.supplierId << ',' << escape(value.supplierName) << ','
           << escape(value.contactNumber) << ',' << escape(value.address) << '\n';
}

void write(std::ostream& output, const Transaction& value)
{
    output << value.transactionId << ',' << value.productId << ','
           << escape(value.productName) << ',' << value.transactionType << ','
           << value.dateTime << ',' << value.quantityChanged << ','
           << value.previousQuantity << ',' << value.updatedQuantity << ','
           << money(value.transactionAmount) << '\n';
}

fs::path reserveSaveDirectory(const fs::path& destination,
                              std::error_code& ec)
{
    for (unsigned int index = 0; index < 10000; ++index) {
        fs::path candidate = destination;
        candidate += ".save." + std::to_string(index);
        if (fs::create_directory(candidate, ec)) return candidate;
        if (ec) return {};
    }
    return {};
}

template <typename Record>
bool save(const std::vector<Record>& records, const std::string& filename,
          const char* header, std::string& error)
{
    const fs::path destination(filename);
    if (filename.empty() || destination.filename().empty()) {
        error = "Invalid destination filename.";
        return false;
    }
    std::unordered_set<int> ids;
    for (const auto& record : records) {
        if (!valid(record) || !ids.insert(idOf(record)).second) {
            error = "Cannot save invalid or duplicate records.";
            return false;
        }
    }
    std::error_code ec;
    const fs::path parent = destination.parent_path();
    if (!parent.empty()) fs::create_directories(parent, ec);
    if (ec) {
        error = "Cannot prepare destination '" + filename + "': " + ec.message();
        return false;
    }
    const bool destinationExists = fs::exists(destination, ec);
    if (ec) {
        error = "Cannot inspect destination '" + filename + "': " + ec.message();
        return false;
    }
    if (destinationExists) {
        const bool regularFile = fs::is_regular_file(destination, ec);
        if (ec) {
            error = "Cannot inspect destination type '" + filename + "': " + ec.message();
            return false;
        }
        if (!regularFile) {
            error = "Destination is not a regular file: '" + filename + "'.";
            return false;
        }
    }
    const fs::path saveDirectory = reserveSaveDirectory(destination, ec);
    if (ec || saveDirectory.empty()) {
        error = "Cannot reserve a save directory for '" + filename +
                "': " + (ec ? ec.message() : "no available name");
        return false;
    }
    const fs::path temporary = saveDirectory / "new.csv";
    const fs::path backup = saveDirectory / "original.csv";
    const auto cleanTemporary = [&]() {
        std::error_code cleanup;
        fs::remove(temporary, cleanup);
        fs::remove(saveDirectory, cleanup);
        if (cleanup) error += " Temporary file cleanup failed: " + cleanup.message();
    };
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            error = "Cannot open temporary file for '" + filename + "'.";
            cleanTemporary();
            return false;
        }
        output << header << '\n';
        for (const auto& record : records) write(output, record);
        output.flush();
        if (!output) {
            output.close();
            error = "Failed writing temporary file for '" + filename + "'.";
            cleanTemporary();
            return false;
        }
        output.close();
        if (!output) {
            error = "Failed closing temporary file for '" + filename + "'.";
            cleanTemporary();
            return false;
        }
    }
    const bool existing = fs::exists(destination, ec);
    if (ec) {
        error = "Cannot inspect destination '" + filename + "'.";
        cleanTemporary();
        return false;
    }
    if (existing) {
        fs::rename(destination, backup, ec);
        if (ec) {
            error = "Cannot back up existing file '" + filename + "'.";
            cleanTemporary();
            return false;
        }
    }
    fs::rename(temporary, destination, ec);
    if (ec) {
        const std::string reason = ec.message();
        error = "Save failed for '" + filename + "': " + reason;
        std::error_code cleanup;
        fs::remove(temporary, cleanup);
        if (cleanup) error += " Temporary file cleanup failed: " + cleanup.message();
        if (existing) {
            fs::rename(backup, destination, cleanup);
            if (cleanup) {
                error += " Restoration failed; original data remains at '" +
                         backup.string() + "': " + cleanup.message();
            } else {
                fs::remove(saveDirectory, cleanup);
                if (cleanup) error += " Save directory cleanup failed: " + cleanup.message();
            }
        } else {
            fs::remove(saveDirectory, cleanup);
            if (cleanup) error += " Save directory cleanup failed: " + cleanup.message();
        }
        return false;
    }
    if (existing) {
        fs::remove(backup, ec);
        if (ec) {
            error = "Saved new records to '" + filename +
                    "', but could not remove backup at '" + backup.string() +
                    "': " + ec.message();
            return true;
        }
    }
    fs::remove(saveDirectory, ec);
    error = ec ? "Saved new records to '" + filename +
                     "', but could not remove save directory '" + saveDirectory.string() +
                     "': " + ec.message()
               : "";
    return true;
}

} // namespace

LoadStatus loadProducts(std::vector<Product>& products, const std::string& filename,
                        std::string& error)
{
    return load(products, filename, PRODUCT_HEADER, error);
}

bool saveProducts(const std::vector<Product>& products, const std::string& filename,
                  std::string& error)
{
    return save(products, filename, PRODUCT_HEADER, error);
}

LoadStatus loadSuppliers(std::vector<Supplier>& suppliers, const std::string& filename,
                         std::string& error)
{
    return load(suppliers, filename, SUPPLIER_HEADER, error);
}

bool saveSuppliers(const std::vector<Supplier>& suppliers, const std::string& filename,
                   std::string& error)
{
    return save(suppliers, filename, SUPPLIER_HEADER, error);
}

LoadStatus loadTransactions(std::vector<Transaction>& transactions, const std::string& filename,
                            std::string& error)
{
    return load(transactions, filename, TRANSACTION_HEADER, error);
}

bool saveTransactions(const std::vector<Transaction>& transactions, const std::string& filename,
                      std::string& error)
{
    return save(transactions, filename, TRANSACTION_HEADER, error);
}
