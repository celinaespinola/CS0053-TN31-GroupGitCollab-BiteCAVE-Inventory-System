#include "authentication.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <conio.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr int kIterations = 600000;
constexpr std::size_t kSaltSize = 16;
constexpr std::size_t kHashSize = 32;

using Salt = std::array<unsigned char, kSaltSize>;
using Hash = std::array<unsigned char, kHashSize>;

// Convert binary data to hexadecimal text.
std::string toHex(const unsigned char* data, std::size_t length)
{
    constexpr char digits[] = "0123456789abcdef";

    std::string result;
    result.reserve(length * 2);

    for (std::size_t i = 0; i < length; ++i) {
        result += digits[data[i] >> 4];
        result += digits[data[i] & 0x0F];
    }

    return result;
}

// Convert hexadecimal text back to binary data.
bool fromHex(
    const std::string& text,
    unsigned char* output,
    std::size_t length
)
{
    if (text.size() != length * 2) {
        return false;
    }

    auto hexValue = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    for (std::size_t i = 0; i < length; ++i) {
        const int high = hexValue(text[i * 2]);
        const int low = hexValue(text[i * 2 + 1]);

        if (high < 0 || low < 0) {
            return false;
        }

        output[i] = static_cast<unsigned char>((high << 4) | low);
    }

    return true;
}

// Derive a password hash using PBKDF2-HMAC-SHA256.
bool deriveHash(
    const std::string& password,
    const Salt& salt,
    Hash& hash
)
{
    if (password.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    return PKCS5_PBKDF2_HMAC(
        password.data(),
        static_cast<int>(password.size()),
        salt.data(),
        static_cast<int>(salt.size()),
        kIterations,
        EVP_sha256(),
        static_cast<int>(hash.size()),
        hash.data()
    ) == 1;
}

// Read a password without displaying typed characters.
bool readHiddenPassword(
    const std::string& prompt,
    std::string& password
)
{
    password.clear();
    std::cout << prompt << std::flush;

    while (true) {
        const int key = _getch();

        if (key == '\r') {
            std::cout << '\n';
            return true;
        }

        // Cancel on Ctrl+C or Ctrl+Z.
        if (key == 3 || key == 26) {
            std::cout << '\n';
            password.clear();
            return false;
        }

        // Ignore extended keyboard keys.
        if (key == 0 || key == 224) {
            (void)_getch();
            continue;
        }

        // Handle Backspace.
        if (key == '\b') {
            if (!password.empty()) {
                password.pop_back();
            }
            continue;
        }

        // Accept printable ASCII characters.
        if (key >= 32 && key <= 126) {
            password.push_back(static_cast<char>(key));
        }
    }
}

// Write all bytes to an open file.
bool writeAll(
    FILE* file,
    const std::string& contents
)
{
    return std::fwrite(
        contents.data(),
        1,
        contents.size(),
        file
    ) == contents.size();
}

} // namespace

bool initializeAdministrator(
    const std::string& filename,
    std::string& error
)
{
    error.clear();

    try {
        if (fs::exists(filename)) {
            error = "Administrator account file already exists.";
            return false;
        }

        std::string username;
        std::string password;
        std::string confirmation;

        std::cout << "=== Administrator Account Setup ===\n";
        std::cout << "Username: ";

        if (!std::getline(std::cin, username) ||
            username.empty() ||
            username.find_first_of("\r\n") != std::string::npos) {
            error = "A valid username is required.";
            return false;
        }

        if (!readHiddenPassword("Password: ", password) ||
            password.empty()) {
            error = "A valid password is required.";
            return false;
        }

        if (!readHiddenPassword("Confirm password: ", confirmation) ||
            password != confirmation) {
            error = "Passwords do not match.";
            return false;
        }

        Salt salt{};
        Hash hash{};

        if (RAND_bytes(
                salt.data(),
                static_cast<int>(salt.size())
            ) != 1) {
            error = "Failed to generate a secure password salt.";
            return false;
        }

        if (!deriveHash(password, salt, hash)) {
            error = "Failed to hash administrator password.";
            return false;
        }

        const fs::path path(filename);

        if (!path.parent_path().empty()) {
            fs::create_directories(path.parent_path());
        }

        // Exclusive creation: fail if the file already exists.
        FILE* rawFile = nullptr;

        if (fopen_s(
                &rawFile,
                filename.c_str(),
                "wbx"
            ) != 0 || rawFile == nullptr) {
            error = "Unable to create administrator account file.";
            return false;
        }

        const std::string contents =
            "1\n" +
            username + "\n" +
            toHex(salt.data(), salt.size()) + "\n" +
            toHex(hash.data(), hash.size()) + "\n" +
            "Administrator\n";

        const bool written = writeAll(rawFile, contents);
        const bool flushed = written && std::fflush(rawFile) == 0;
        const bool closed = std::fclose(rawFile) == 0;

        if (!written || !flushed || !closed) {
            error = "Failed to save administrator account.";

            // Remove a partially created account file.
            if (std::remove(filename.c_str()) != 0) {
                error += " Incomplete account file could not be removed.";
            }

            return false;
        }

        return true;
    }
    catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool loadUsers(
    std::vector<User>& users,
    const std::string& filename,
    std::string& error
)
{
    error.clear();

    std::ifstream file(filename, std::ios::binary);

    if (!file) {
        error = "Unable to open administrator account file.";
        return false;
    }

    User administrator{};
    std::string id;

    if (!std::getline(file, id) ||
        !std::getline(file, administrator.username) ||
        !std::getline(file, administrator.passwordSalt) ||
        !std::getline(file, administrator.passwordHash) ||
        !std::getline(file, administrator.accountRole)) {
        error = "Administrator account file is incomplete.";
        return false;
    }

    if (id != "1" ||
        administrator.username.empty() ||
        administrator.accountRole != "Administrator") {
        error = "Administrator account contains invalid information.";
        return false;
    }

    Salt salt{};
    Hash hash{};

    if (!fromHex(
            administrator.passwordSalt,
            salt.data(),
            salt.size()
        ) ||
        !fromHex(
            administrator.passwordHash,
            hash.data(),
            hash.size()
        )) {
        error = "Administrator account contains invalid credential data.";
        return false;
    }

    // Reject unexpected additional records.
    std::string extraLine;

    if (std::getline(file, extraLine)) {
        error = "Administrator account file contains unexpected data.";
        return false;
    }

    if (file.bad()) {
        error = "Failed while reading administrator account file.";
        return false;
    }

    administrator.userId = 1;

    // Replace the collection only after successful validation.
    users.clear();
    users.push_back(administrator);

    return true;
}

bool authenticateUser(
    const std::vector<User>& users,
    const std::string& username,
    const std::string& password
)
{
    for (const auto& user : users) {
        if (user.username != username) {
            continue;
        }

        Salt salt{};
        Hash expectedHash{};
        Hash actualHash{};

        if (!fromHex(
                user.passwordSalt,
                salt.data(),
                salt.size()
            ) ||
            !fromHex(
                user.passwordHash,
                expectedHash.data(),
                expectedHash.size()
            )) {
            return false;
        }

        if (!deriveHash(password, salt, actualHash)) {
            return false;
        }

        return CRYPTO_memcmp(
            expectedHash.data(),
            actualHash.data(),
            expectedHash.size()
        ) == 0;
    }

    return false;
}