#pragma once

#include <optional>
#include <string>

std::optional<int> readInt(
    const std::string& prompt,
    int minimum,
    int maximum
);

std::optional<double> readDouble(
    const std::string& prompt,
    double minimum
);

std::optional<std::string> readNonEmptyString(
    const std::string& prompt
);

std::optional<bool> confirmAction(
    const std::string& prompt
);

bool equalsIgnoreCase(
    const std::string& first,
    const std::string& second
);