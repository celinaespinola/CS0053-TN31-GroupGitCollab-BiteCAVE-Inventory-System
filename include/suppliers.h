#pragma once

#include <string>
#include <vector>
#include "product.h"
#include "supplier.h"

bool addSupplier(
    std::vector<Supplier>& suppliers,
    const Supplier& supplier,
    std::string& error
);

std::vector<Supplier> searchSuppliersByName(
    const std::vector<Supplier>& suppliers,
    const std::string& name
);

bool updateSupplier(
    std::vector<Supplier>& suppliers,
    int supplierId,
    const Supplier& updatedSupplier,
    std::string& error
);

bool deleteSupplier(
    std::vector<Supplier>& suppliers,
    const std::vector<Product>& products,
    int supplierId,
    std::string& error
);