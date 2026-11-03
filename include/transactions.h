#pragma once

#include <string>
#include <vector>
#include "transaction.h"

int generateTransactionId(
    const std::vector<Transaction>& transactions
);

bool addTransaction(
    std::vector<Transaction>& transactions,
    const Transaction& transaction,
    std::string& error
);

std::vector<Transaction> getTransactionHistory(
    const std::vector<Transaction>& transactions
);