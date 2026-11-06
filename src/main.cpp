#include "app.h"

#include <exception>
#include <iostream>

int main()
{
    try {
        runApplication();
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Fatal application error: "
                  << error.what() << '\n';
        return 1;
    }
    catch (...) {
        std::cerr << "Fatal application error: Unknown exception.\n";
        return 1;
    }
}