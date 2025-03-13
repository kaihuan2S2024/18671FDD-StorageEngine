/*
 * This file contains the definitions of various integer types used
 */

#pragma once

#include <string>
#include "sql_enum.h"
#include <unordered_map>

/*
 * sql_int.h
 *
 * This file contains the definitions of various integer types used in the database
 */

// Unsigned integers
typedef unsigned long long int u64;
typedef unsigned int u32;
typedef unsigned short int u16;
typedef unsigned char u8;


// Pointers

typedef int ptr;
typedef unsigned int u_ptr;


// Commonly used types

typedef u32 PageNumber;
typedef u16 ImageIndex;

// utility functions
void sqliteDequote(std::string &str);
void sqliteCompressSpaces(std::string &str);
