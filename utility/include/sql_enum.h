#pragma once

/*
 * sql_enum.h
 *
 * This file contains the definitions of various enums used in the database
 */

// The SQL_ prefix is needed to avoid namespace conflict when importing
enum SQL_TYPE {
  SQL_INTEGER = 0,
  SQL_TEXT = 1,
  SQL_NONE = 2,
  SQL_REAL = 3,
  SQL_NUMERIC = 4
};
