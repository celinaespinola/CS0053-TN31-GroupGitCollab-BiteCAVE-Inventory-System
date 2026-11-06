#include "validation.h"
#include "display.h"

#include <charconv>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

std::string trimWhitespace(const std::string& text)
{
    const std::string whitespace = " \t\r\n\f\v";
    const auto first = text.find_first_not_of(whitespace);

    if (first == std::string::npos) {
        return "";
    }

    const auto last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

std::optional<std::string> readTrimmedLine(const std::string& prompt)
{
    std::cout << prompt << std::flush;

    std::string line;
    if (!std::getline(std::cin, line)) {
        return std::nullopt;
    }

    return trimWhitespace(line);
}

bool hasValidNumberFormat(const std::string& text, bool allowDecimal)
{
    if (text.empty()) {
        return false;
    }

    std::size_t position = 0;
    if (text.front() == '+' || text.front() == '-') {
        position = 1;
    }

    bool hasDigit = false;
    bool hasDecimalPoint = false;

    for (; position < text.size(); ++position) {
        const char character = text[position];

        if (character >= '0' && character <= '9') {
            hasDigit = true;
        } else if (allowDecimal && character == '.' && !hasDecimalPoint) {
            hasDecimalPoint = true;
        } else {
            return false;
        }
    }

    return hasDigit;
}

char lowerEnglishLetter(char character)
{
    if (character >= 'A' && character <= 'Z') {
        return static_cast<char>(character + ('a' - 'A'));
    }

    return character;
}

} // namespace

std::optional<int> readInt(
    const std::string& prompt,
    int minimum,
    int maximum
)
{
    if (minimum > maximum) {
        throw std::invalid_argument("readInt: minimum exceeds maximum.");
    }

    while (true) {
        const auto input = readTrimmedLine(prompt);
        if (!input.has_value()) {
            return std::nullopt;
        }

        if (hasValidNumberFormat(*input, false)) {
            const char* first = input->data();
            const char* last = first + input->size();

            // from_chars does not accept a leading plus sign.
            if (*first == '+') {
                ++first;
            }

            int value = 0;
            const auto result = std::from_chars(first, last, value);

            if (result.ec == std::errc{} && result.ptr == last &&
                value >= minimum && value <= maximum) {
                return value;
            }
        }

        displayError(
            "Enter a whole number from " + std::to_string(minimum) +
            " to " + std::to_string(maximum) + "."
        );
    }
}

std::optional<double> readDouble(
    const std::string& prompt,
    double minimum
)
{
    if (!std::isfinite(minimum)) {
        throw std::invalid_argument("readDouble: minimum must be finite.");
    }

    while (true) {
        const auto input = readTrimmedLine(prompt);
        if (!input.has_value()) {
            return std::nullopt;
        }

        if (hasValidNumberFormat(*input, true)) {
            const char* first = input->data();
            const char* last = first + input->size();

            if (*first == '+') {
                ++first;
            }

            double value = 0.0;
            const auto result = std::from_chars(
                first, last, value, std::chars_format::fixed
            );

            if (result.ec == std::errc{} && result.ptr == last &&
                std::isfinite(value) && value >= minimum) {
                return value;
            }
        }

        displayError(
            "Enter a finite decimal number at or above the stated minimum. "
            "Use a decimal point, without commas, exponents, or extra text."
        );
    }
}

std::optional<std::string> readNonEmptyString(const std::string& prompt)
{
    while (true) {
        const auto input = readTrimmedLine(prompt);
        if (!input.has_value()) {
            return std::nullopt;
        }

        if (!input->empty()) {
            return input;
        }

        displayError("Enter a value containing at least one non-space character.");
    }
}

std::optional<bool> confirmAction(const std::string& prompt)
{
    while (true) {
        const auto input = readTrimmedLine(prompt);
        if (!input.has_value()) {
            return std::nullopt;
        }

        if (equalsIgnoreCase(*input, "y") || equalsIgnoreCase(*input, "yes")) {
            return true;
        }

        if (equalsIgnoreCase(*input, "n") || equalsIgnoreCase(*input, "no")) {
            return false;
        }

        displayError("Enter Y or Yes to confirm, or N or No to decline.");
    }
}

bool equalsIgnoreCase(const std::string& first, const std::string& second)
{
    if (first.size() != second.size()) {
        return false;
    }

    for (std::size_t index = 0; index < first.size(); ++index) {
        if (lowerEnglishLetter(first[index]) != lowerEnglishLetter(second[index])) {
            return false;
        }
    }

    return true;
}