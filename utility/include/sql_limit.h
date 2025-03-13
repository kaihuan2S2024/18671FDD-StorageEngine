#pragma once

#include "sql_int.h"

/*
 * sql_limit.h
 *
 * This file contains the definitions of various limits used in the database
 *
 * The limits are based on sqlite's own limits:
 * https://github.com/sqlite/sqlite/blob/master/src/sqliteLimit.h
 */

// Maximum number of columns in a table
constexpr u32 kMaxColumn = 2000;

// Maximum number of pages in a database file
constexpr PageNumber kMaxPageCount = 1073741823;

// Default page size in bytes
constexpr u32 kDefaultPageSize = 4096;

// Default cache size (number of pages in the cache)
constexpr int kDefaultCacheSize = 10;

// Maximum page size in bytes
constexpr u32 kMaxPageSize = 65536;

// The maximum length of a TEXT or BLOB in bytes.
constexpr u32 kMaxSqlLength = 1000000000;

constexpr u32 kPageSize = 1024;



