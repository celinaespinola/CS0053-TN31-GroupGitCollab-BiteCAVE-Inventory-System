#include "inventory.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>


namespace {

std::string toLower(const std::string& text) {
    std::string result = text;

    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        }
    );

    return result;
}

bool hasNonWhitespace(const std::string& text) {
    return std::any_of(
        text.begin(),
        text.end(),
        [](unsigned char character) {
            return !std::isspace(character);
        }
    );
}

bool validateProductData(const Product& product, std::string& error) {
    if (product.productId <= 0) {
        error = "Product ID must be a positive integer.";
        return false;
    }

    if (!hasNonWhitespace(product.productName)) {
        error = "Product name cannot be empty.";
        return false;
    }

    if (!hasNonWhitespace(product.category)) {
        error = "Product category cannot be empty.";
        return false;
    }

    if (!std::isfinite(product.unitPrice) || product.unitPrice < 0.0) {
        error = "Unit price must be a nonnegative finite value.";
        return false;
    }

    if (product.quantity < 0) {
        error = "Quantity cannot be negative.";
        return false;
    }

    if (product.lowStockThreshold < 0) {
        error = "Low-stock threshold cannot be negative.";
        return false;
    }

    if (product.supplierId <= 0) {
        error = "Supplier ID must be a positive integer.";
        return false;
    }

    return true;
}

} // namespace

bool addProduct(
    std::vector<Product>& products,
    const Product& product,
    std::string& error
) {
    if (!validateProductData(product, error)) {
        return false;
    }

    if (findProductById(products, product.productId).has_value()) {
        error = "A product with this ID already exists.";
        return false;
    }

    products.push_back(product);
    return true;
}

std::optional<Product> findProductById(
    const std::vector<Product>& products,
    int productId
) {
    const auto product = std::find_if(
        products.begin(),
        products.end(),
        [productId](const Product& currentProduct) {
            return currentProduct.productId == productId;
        }
    );

    if (product == products.end()) {
        return std::nullopt;
    }

    return *product;
}

std::vector<Product> searchProductsByName(
    const std::vector<Product>& products,
    const std::string& name
) {
    std::vector<Product> matches;
    const std::string searchName = toLower(name);

    for (const Product& product : products) {
        if (toLower(product.productName).find(searchName) != std::string::npos) {
            matches.push_back(product);
        }
    }

    return matches;
}

bool updateProduct(
    std::vector<Product>& products,
    int productId,
    const Product& updatedProduct,
    std::string& error
) {
    if (updatedProduct.productId != productId) {
        error = "Product ID cannot be changed.";
        return false;
    }

    if (!validateProductData(updatedProduct, error)) {
        return false;
    }

    const auto product = std::find_if(
        products.begin(),
        products.end(),
        [productId](const Product& currentProduct) {
            return currentProduct.productId == productId;
        }
    );

    if (product == products.end()) {
        error = "Product not found.";
        return false;
    }

    *product = updatedProduct;
    return true;
}

bool deleteProduct(
    std::vector<Product>& products,
    int productId,
    std::string& error
) {
    const auto product = std::find_if(
        products.begin(),
        products.end(),
        [productId](const Product& currentProduct) {
            return currentProduct.productId == productId;
        }
    );

    if (product == products.end()) {
        error = "Product not found.";
        return false;
    }

    products.erase(product);
    return true;
}

bool stockIn(
    std::vector<Product>& products,
    int productId,
    int quantity,
    std::string& error
) {
    if (quantity <= 0) {
        error = "Restock quantity must be a positive integer.";
        return false;
    }

    const auto product = std::find_if(
        products.begin(),
        products.end(),
        [productId](const Product& currentProduct) {
            return currentProduct.productId == productId;
        }
    );

    if (product == products.end()) {
        error = "Product not found.";
        return false;
    }

    if (quantity > std::numeric_limits<int>::max() - product->quantity) {
        error = "Restock quantity would exceed the allowed limit.";
        return false;
    }

    product->quantity += quantity;
    return true;
}

bool stockOut(
    std::vector<Product>& products,
    int productId,
    int quantity,
    std::string& error
) {
    if (quantity <= 0) {
        error = "Stock-out quantity must be a positive integer.";
        return false;
    }

    const auto product = std::find_if(
        products.begin(),
        products.end(),
        [productId](const Product& currentProduct) {
            return currentProduct.productId == productId;
        }
    );

    if (product == products.end()) {
        error = "Product not found.";
        return false;
    }

    if (quantity > product->quantity) {
        error = "Insufficient stock available.";
        return false;
    }

    product->quantity -= quantity;
    return true;
}