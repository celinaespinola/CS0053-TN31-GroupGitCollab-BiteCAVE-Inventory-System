#include "suppliers.h"

#include <algorithm>
#include <cctype>

namespace
{

bool hasInformation(const std::string& value)
{
    return std::any_of(value.begin(), value.end(),
                       [](unsigned char character) { return !std::isspace(character); });
}

char lowerCharacter(unsigned char character)
{
    return static_cast<char>(std::tolower(character));
}

bool containsIgnoreCase(const std::string& text, const std::string& query)
{
    return std::search(text.begin(), text.end(), query.begin(), query.end(),
                       [](unsigned char left, unsigned char right)
                       { return lowerCharacter(left) == lowerCharacter(right); }) != text.end();
}

bool hasRequiredInformation(const Supplier& supplier)
{
    return hasInformation(supplier.supplierName) && hasInformation(supplier.contactNumber) &&
           hasInformation(supplier.address);
}

} // namespace

bool addSupplier(std::vector<Supplier>& suppliers, const Supplier& supplier, std::string& error)
{
    if (supplier.supplierId <= 0)
    {
        error = "Supplier ID must be positive.";
        return false;
    }
    if (std::any_of(suppliers.begin(), suppliers.end(), [&](const Supplier& existing)
                    { return existing.supplierId == supplier.supplierId; }))
    {
        error = "Supplier ID already exists.";
        return false;
    }
    if (!hasRequiredInformation(supplier))
    {
        error = "Supplier name, contact number, and address are required.";
        return false;
    }

    suppliers.push_back(supplier);
    error.clear();
    return true;
}

std::vector<Supplier> searchSuppliersByName(const std::vector<Supplier>& suppliers,
                                            const std::string& name)
{
    std::vector<Supplier> matches;
    for (const Supplier& supplier : suppliers)
    {
        if (containsIgnoreCase(supplier.supplierName, name))
        {
            matches.push_back(supplier);
        }
    }
    return matches;
}

bool updateSupplier(std::vector<Supplier>& suppliers, int supplierId,
                    const Supplier& updatedSupplier, std::string& error)
{
    auto existing =
        std::find_if(suppliers.begin(), suppliers.end(), [supplierId](const Supplier& supplier)
                     { return supplier.supplierId == supplierId; });
    if (existing == suppliers.end())
    {
        error = "Supplier not found.";
        return false;
    }
    if (updatedSupplier.supplierId != supplierId)
    {
        error = "Supplier ID cannot be changed.";
        return false;
    }
    if (!hasRequiredInformation(updatedSupplier))
    {
        error = "Supplier name, contact number, and address are required.";
        return false;
    }

    *existing = updatedSupplier;
    error.clear();
    return true;
}

bool deleteSupplier(std::vector<Supplier>& suppliers, const std::vector<Product>& products,
                    int supplierId, std::string& error)
{
    auto existing =
        std::find_if(suppliers.begin(), suppliers.end(), [supplierId](const Supplier& supplier)
                     { return supplier.supplierId == supplierId; });
    if (existing == suppliers.end())
    {
        error = "Supplier not found.";
        return false;
    }
    if (std::any_of(products.begin(), products.end(), [supplierId](const Product& product)
                    { return product.supplierId == supplierId; }))
    {
        error = "Supplier is referenced by an active product.";
        return false;
    }

    suppliers.erase(existing);
    error.clear();
    return true;
}
