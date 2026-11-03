#pragma once

#include <optional>
#include <string>
#include <vector>

#include "product.h"

bool addProduct(
    std::vector<Product>& products,
    const Product& product,
    std::string& error
);

std::optional<Product> findProductById(
    const std::vector<Product>& products,
    int productId
);

std::vector<Product> searchProductsByName(
    const std::vector<Product>& products,
    const std::string& name
);

bool updateProduct(
    std::vector<Product>& products,
    int productId,
    const Product& updatedProduct,
    std::string& error
);

bool deleteProduct(
    std::vector<Product>& products,
    int productId,
    std::string& error
);

bool stockIn(
    std::vector<Product>& products,
    int productId,
    int quantity,
    std::string& error
);

bool stockOut(
    std::vector<Product>& products,
    int productId,
    int quantity,
    std::string& error
);