# BiteCAVE Inventory System

**CS0053 – Programming Tools and Techniques**  
**Section:** TN31  
**Group:** GitCollab

A console-based C++ grocery inventory management system developed by GitCollab for CS0053 Programming Tools and Techniques. BiteCAVE helps small grocery businesses manage products, monitor stock levels, maintain supplier records, track inventory transactions, and generate reports through a menu-driven command-line interface.

## 1. Project Overview

BiteCAVE Inventory System provides a centralized solution for managing grocery inventory records. It reduces reliance on manual inventory tracking by allowing users to maintain product information, monitor stock movements, and access inventory reports.

The system uses local CSV files for persistent data storage and provides administrator authentication to restrict access to inventory management functions.

## 2. System Features

| Feature | Description |
|---|---|
| F01 – Add New Product | Register products with an ID, name, category, unit price, quantity, low-stock threshold, and supplier ID. |
| F02 – View All Products | Display product records in a formatted inventory table. |
| F03 – Search Product | Search products by ID or case-insensitive partial name. |
| F04 – Update Product Details | Modify selected product information while preserving unchanged fields. |
| F05 – Delete Product | Remove an existing product after confirmation. |
| F06 – Stock In / Restock | Increase product quantities and record stock-in transactions. |
| F07 – Stock Out / Record Sale | Deduct available stock and record stock-out transactions. |
| F08 – Low-Stock Monitoring | Identify products whose quantities are at or below their low-stock thresholds. |
| F09 – Inventory Summary Report | Display total products, total units, inventory value, low-stock count, and out-of-stock count. |
| F10 – Sort Products | Sort products by name, price, or quantity in ascending or descending order. |
| F11 – Filter Products by Category | Display products belonging to a selected category. |
| F12 – Transaction History | View recorded stock-in and stock-out transactions. |
| F13 – Export Inventory Report | Export inventory details and summary information to CSV files. |
| F14 – Supplier Management | Add, view, search, update, and delete supplier records. |
| F15 – User Authentication | Create an administrator account, verify login credentials, enforce a three-attempt login limit, and support logout. |

The main menu contains options 1–14 for inventory functions, option 15 for logout, and option 16 for Save and Exit.

## 3. Technologies Used

| Technology / Tool | Purpose |
|---|---|
| C++17 | Primary programming language |
| MSYS2 UCRT64 G++ 16.2.0 | Compilation and linking of the C++17 application |
| OpenSSL | PBKDF2-HMAC-SHA256 password hashing |
| C++ Standard Library | Collections, file operations, input/output, and data processing |
| CSV | Persistent inventory, supplier, and transaction storage |
| Visual Studio Code | Development environment |
| clang-format 19.1.7 | Standardizing C++ code formatting |
| clang-tidy | Static analysis and identification of potential code-quality issues |
| GDB 17.2 | Debugging using breakpoints and variable inspection |
| Git | Version control |
| GitHub | Repository hosting, collaboration, pull requests, and code review |

## 4. Project Structure

```text
CS0053-TN31-GroupGitCollab-BiteCAVE-Inventory-System/
├── include/
│   ├── app.h
│   ├── authentication.h
│   ├── display.h
│   ├── inventory.h
│   ├── product.h
│   ├── reports.h
│   ├── storage.h
│   ├── supplier.h
│   ├── suppliers.h
│   ├── transaction.h
│   ├── transactions.h
│   ├── user.h
│   └── validation.h
├── src/
│   ├── app.cpp
│   ├── authentication.cpp
│   ├── display.cpp
│   ├── inventory.cpp
│   ├── main.cpp
│   ├── reports.cpp
│   ├── storage.cpp
│   ├── suppliers.cpp
│   ├── transactions.cpp
│   └── validation.cpp
├── data/
├── reports/
├── .clang-format
├── .gitignore
└── README.md
```

The `data/` directory stores application records, while the `reports/` directory contains exported CSV reports. These directories may initially be empty.

The `.clang-format` file contains the shared C++ formatting configuration used to maintain consistent code style across the project.

## 5. Prerequisites

The current application implementation is designed for Windows.

Before compiling, ensure the following are available:

- A C++17-compatible G++ compiler, such as the MSYS2 UCRT64 toolchain.
- OpenSSL development headers and libraries compatible with the compiler.
- A terminal such as PowerShell or the VS Code integrated terminal.
- Git, for cloning the repository.

The application uses Windows-specific functions for hidden password entry and local time handling.

## 6. Installation and Execution

### 6.1 Clone the Repository

Open a terminal and run:

```powershell
git clone https://github.com/celinaespinola/CS0053-TN31-GroupGitCollab-BiteCAVE-Inventory-System.git
```

Navigate to the project directory:

```powershell
cd CS0053-TN31-GroupGitCollab-BiteCAVE-Inventory-System
```

### 6.2 Compile the Application

From the repository's root directory, run:

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic -I include src/*.cpp -o bitecave.exe -lcrypto
```

This command compiles and links the application modules using C++17 and OpenSSL.

Successful compilation normally produces no output and generates `bitecave.exe`.

### 6.3 Run the Application

Execute:

```powershell
.\bitecave.exe
```

The application will start in the terminal.

If the compiler cannot find the OpenSSL headers or library, verify the OpenSSL installation and compiler environment before rebuilding.

## 7. First-Time Administrator Setup

When BiteCAVE launches for the first time, it checks whether an administrator account file exists.

If no account file is found, the application prompts the user to create an administrator account.

The administrator must enter a username, password, and password confirmation.

The system uses PBKDF2-HMAC-SHA256 with 600,000 iterations, a randomly generated 16-byte salt, and a 32-byte derived password hash.

Administrator information is stored in `data/users.dat`. The original password is not saved in plaintext.

On subsequent launches, the application loads the existing administrator account and displays the login screen.

The system allows a maximum of three failed login attempts per login session. After the third failed attempt, the application terminates.

## 8. Data Storage

BiteCAVE uses local files to preserve information between application sessions.

| File | Purpose |
|---|---|
| `data/inventory.csv` | Product information, prices, quantities, thresholds, and supplier references |
| `data/suppliers.csv` | Supplier information and contact details |
| `data/transactions.csv` | Stock-in and stock-out transaction history |
| `data/users.dat` | Administrator account information and password verification data |
| `reports/inventory_report.csv` | Exported inventory details |
| `reports/inventory_summary.csv` | Exported inventory summary |

Inventory, supplier, and transaction records are automatically saved after successful modifying operations. Option 16 performs a final save before exiting.

Application data and administrator credentials should not be committed to the public repository.

## 9. Sample Input and Output

The following example demonstrates adding a supplier, registering a product, and recording stock movements.

### 9.1 Add Supplier

```text
Select an option (1-16): 14

Supplier Management
1. Add Supplier
2. View Suppliers
3. Search Supplier
4. Update Supplier
5. Delete Supplier
6. Back

Select option: 1
Supplier ID: 1
Supplier name: Test Grocery Supplier
Contact number: 09123456789
Address: Quezon City

Supplier changes saved successfully.
```

### 9.2 Add New Product

```text
Select an option (1-16): 1

Product ID: 101
Product name: Paper Bag
Category: Packaging
Unit price: 0.00
Initial quantity: 50
Low-stock threshold: 10
Supplier ID: 1

Inventory changes saved successfully.
```

### 9.3 Stock In

```text
Select an option (1-16): 6

Product ID: 101
Quantity: 20

Stock movement recorded and saved.
```

The product quantity increases from 50 to 70 units.

### 9.4 Stock Out

```text
Select an option (1-16): 7

Product ID: 101
Quantity: 15

Stock movement recorded and saved.
```

The product quantity decreases from 70 to 55 units. The transaction is recorded in the transaction history.

### 9.5 Save and Exit

```text
Select an option (1-16): 16

Inventory saved. Goodbye!
```

The application saves the current records and terminates.

The examples above show representative prompts and results. Actual terminal output also includes formatted screen headers and tables.

## 10. Development and Collaboration

The project was developed collaboratively using Git and GitHub.

The team followed an agreed feature-branch structure, with individual members responsible for specific application modules.

Development changes were committed to assigned feature branches, submitted through pull requests, reviewed by teammates, and merged into `main` after approval.

The team also conducted module-level verification, integration testing, and code improvement activities.

## 11. Team Members and Responsibilities

| Team Member | Responsibilities |
|---|---|
| **Espinola, Celina Mae R.** | Project leadership, repository coordination, administrator authentication (F15), main application controller, and program integration |
| **Andaya, Mark Jastin M.** | Inventory reporting (F08–F11, F13), CLI display framework, and reusable input validation |
| **Celis, Syril Marie C.** | Product management (F01–F05), stock-in (F06), and stock-out (F07) |
| **Villorente, Lady Antonette E.** | File persistence, transaction history (F12), supplier management (F14), and QA coordination |

## 12. Project Limitations

- The application is designed for a Windows console environment.
- Data is stored locally rather than in a remote database.
- The current authentication system supports one administrator account.
- The three-attempt login restriction applies to the current login session.
- Separate CSV file saves do not provide crash-atomic transactions across multiple files.
- The application is intended for academic demonstration and small-scale inventory management.

## 13. Academic Information

**Course:** CS0053 – Programming Tools and Techniques  
**Section:** TN31  
**Project:** BiteCAVE Inventory System  
**Group:** GitCollab

Developed as a collaborative final project demonstrating C++ programming, modular development, version control, GitHub pull requests, code review, testing, and application integration.
