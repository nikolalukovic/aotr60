#pragma once
#include <cstddef>

// Name -> address of every DLL stub and variable that tools/sites.json refers to through <rel32:NAME> / <abs32:NAME>.
struct Symbol {
    const char* name;
    const void* address;
};

const void* FindSymbol(const char* name); // nullptr if unknown
const Symbol* AllSymbols(size_t* count);
