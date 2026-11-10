#include "transactions.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace
{

bool hasInformation(const std::string& value)
{
    return std::any_of(value.begin(), value.end(),
                       [](unsigned char ch) { return !std::isspace(ch); });
}

int numberAt(const std::string& value, std::size_t position, std::size_t length)
{
    int number = 0;
    for (std::size_t i = position; i < position + length; ++i)
    {
        if (value[i] < '0' || value[i] > '9')
        {
            return -1;
        }
        number = number * 10 + (value[i] - '0');
    }
    return number;
}

bool validTimestamp(const std::string& value)
{
    if (value.size() != 19 || value[4] != '-' || value[7] != '-' || value[10] != ' ' ||
        value[13] != ':' || value[16] != ':')
    {
        return false;
    }

    const int year = numberAt(value, 0, 4);
    const int month = numberAt(value, 5, 2);
    const int day = numberAt(value, 8, 2);
    const int hour = numberAt(value, 11, 2);
    const int minute = numberAt(value, 14, 2);
    const int second = numberAt(value, 17, 2);
    if (year < 1 || month < 1 || month > 12 || hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
        second < 0 || second > 59)
    {
        return false;
    }

    static constexpr int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leapYear = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    const int maxDay = daysInMonth[month - 1] + (month == 2 && leapYear ? 1 : 0);
    return day >= 1 && day <= maxDay;
}

} // namespace

int generateTransactionId(const std::vector<Transaction>& transactions)
{
    int highestId = 0;
    for (const auto& transaction : transactions)
    {
        highestId = std::max(highestId, transaction.transactionId);
    }
    return highestId == std::numeric_limits<int>::max() ? -1 : highestId + 1;
}

bool addTransaction(std::vector<Transaction>& transactions, const Transaction& transaction,
                    std::string& error)
{
    if (transaction.transactionId <= 0)
    {
        error = "Transaction ID must be positive.";
        return false;
    }
    if (std::any_of(transactions.begin(), transactions.end(), [&](const Transaction& existing)
                    { return existing.transactionId == transaction.transactionId; }))
    {
        error = "Transaction ID already exists.";
        return false;
    }
    if (transaction.productId <= 0 || !hasInformation(transaction.productName))
    {
        error = "Product ID and name are required.";
        return false;
    }
    if (transaction.transactionType != "STOCK_IN" && transaction.transactionType != "STOCK_OUT")
    {
        error = "Transaction type must be STOCK_IN or STOCK_OUT.";
        return false;
    }
    if (!validTimestamp(transaction.dateTime))
    {
        error = "Timestamp must be a valid YYYY-MM-DD HH:MM:SS date and time.";
        return false;
    }
    if (transaction.quantityChanged <= 0 || transaction.previousQuantity < 0 ||
        transaction.updatedQuantity < 0)
    {
        error = "Transaction quantities must be valid and nonnegative.";
        return false;
    }
    const long long expectedQuantity =
        transaction.transactionType == "STOCK_IN"
            ? static_cast<long long>(transaction.previousQuantity) + transaction.quantityChanged
            : static_cast<long long>(transaction.previousQuantity) - transaction.quantityChanged;
    if (expectedQuantity != transaction.updatedQuantity)
    {
        error = "Updated quantity does not match the stock movement.";
        return false;
    }
    if (!std::isfinite(transaction.transactionAmount) ||
        (transaction.transactionType == "STOCK_IN" && transaction.transactionAmount != 0.0) ||
        (transaction.transactionType == "STOCK_OUT" && transaction.transactionAmount < 0.0))
    {
        error = "Transaction amount is invalid for the stock movement.";
        return false;
    }

    transactions.push_back(transaction);
    error.clear();
    return true;
}

std::vector<Transaction> getTransactionHistory(const std::vector<Transaction>& transactions)
{
    auto history = transactions;
    std::sort(history.begin(), history.end(),
              [](const Transaction& left, const Transaction& right)
              {
                  if (left.dateTime != right.dateTime)
                  {
                      return left.dateTime < right.dateTime;
                  }
                  return left.transactionId < right.transactionId;
              });
    return history;
}
