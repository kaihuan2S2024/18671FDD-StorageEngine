/*
 * sql_rc.h
 *
 * This file contains result code for SQL_Storage_Engine
 */

#pragma once

#include <ostream>
#include <string>

#include "sql_int.h"

/*
 * ResultCode
 *
 * An enumerated type indicating the result code of
 * function calls in SQLite_Storage_Engine.
 *
 * This enum class contains every code in sqlite's primary result code list:
 * https://www.sqlite.org/rescode.html
 *
 * Its naming scheme is based on Google's Coding Guideline on Enumerate Names:
 * https://google.github.io/styleguide/cppguide.html#Enumerator_Names
 *
 * An example of Google declares an enum type could be found here:
 * https://github.com/abseil/abseil-cpp/blob/master/absl/status/status.h
 *
 *
 * Note that in the Sqlite's original implementation, the result code is a
 * 32-bit integer The last 8 bits are the primary result code, and the first 24
 * bits used to extended result code. SQLITE_ERROR_SNAPSHOT is SQLITE_ERROR |
 * (3<<8), where SQLITE_ERROR is the value 1. This allows the primary result
 * code to be contained within the full result code.
 *
 * Many of Sqlite's functions return a result code
 * By returning result codes, we can avoid throwing exceptions
 * Google's C++ Style Guide recommends using exceptions only for exceptional
 * cases https://google.github.io/styleguide/cppguide.html#Exceptions
 */

enum class ResultCode : u32 {
  // Primary Result Code
  kInit = 18671,
  kOk = 0,           // Successful result
  kError = 1,        // Generic error
  kInternal = 2,     // Internal logic error in SQLite
  kPerm = 3,         // Access permission denied
  kAbort = 4,        // Callback routine requested an abort
  kBusy = 5,         // The database file is locked_
  kLocked = 6,       // A table in the database is locked_
  kNoMem = 7,        // A malloc() failed
  kReadOnly = 8,     // Attempt to write a readonly database
  kInterrupt = 9,    // Operation terminated by sqlite3_interrupt()
  kIOError = 10,     // Some kind of disk I/O error occurred
  kCorrupt = 11,     // The database disk image is malformed
  kNotFound = 12,    // Unknown opcode in sqlite3_file_control()
  kFull = 13,        // Insertion failed because database is full
  kCantOpen = 14,    // Unable to open the database file
  kProtocol = 15,    // Database lock protocol error
  kEmpty = 16,       // Internal use only
  kSchema = 17,      // The database schema changed
  kTooBig = 18,      // String or BLOB exceeds size limit
  kConstraint = 19,  // Abort due to constraint violation
  kMismatch = 20,    // Data type mismatch
  kMisuse = 21,      // Library used incorrectly
  kNoLFS = 22,       // Uses OS features not supported on host
  kAuth = 23,        // Authorization denied
  kFormat = 24,      // Not used
  kRange = 25,       // 2nd parameter to sqlite3_bind out of range
  kNotADB = 26,      // File opened that is not a database file
  kNotice = 27,      // Notifications from sqlite3_log()
  kWarning = 28,     // Warnings from sqlite3_log()
  kRow = 100,        // sqlite3_step() has another row ready
  kDone = 101,       // sqlite3_step() has finished executing

  // Extended Result Code: Error
  kErrorMissingCollSeq = ResultCode::kError | (1 << 8),  // 257
  kErrorRetry = ResultCode::kError | (2 << 8),           // 513
  kErrorSnapshot = ResultCode::kError | (3 << 8),        // 769

  // Extended Result Code: IOError
  kIOErrorRead = ResultCode::kIOError | (1 << 8),                // 266
  kIOErrorShortRead = ResultCode::kIOError | (2 << 8),           // 522
  kIOErrorWrite = ResultCode::kIOError | (3 << 8),               // 778
  kIOErrorFsync = ResultCode::kIOError | (4 << 8),               // 1034
  kIOErrorDirFSync = ResultCode::kIOError | (5 << 8),            // 1290
  kIOErrorTruncate = ResultCode::kIOError | (6 << 8),            // 1546
  kIOErrorFStat = ResultCode::kIOError | (7 << 8),               // 1802
  kIOErrorUnlock = ResultCode::kIOError | (8 << 8),              // 2058
  kIOErrorRDLock = ResultCode::kIOError | (9 << 8),              // 2314
  kIOErrorDelete = ResultCode::kIOError | (10 << 8),             // 2570
  kIOErrorBlocked = ResultCode::kIOError | (11 << 8),            // 2826
  kIOErrorNoMem = ResultCode::kIOError | (12 << 8),              // 3082
  kIOErrorAccess = ResultCode::kIOError | (13 << 8),             // 3338
  kIOErrorCheckReservedLock = ResultCode::kIOError | (14 << 8),  // 3594
  kIOErrorLock = ResultCode::kIOError | (15 << 8),               // 3850
  kIOErrorClose = ResultCode::kIOError | (16 << 8),              // 4106
  kIOErrorDirClose = ResultCode::kIOError | (17 << 8),           // 4362
  kIOErrorSHMOpen = ResultCode::kIOError | (18 << 8),            // 4618
  kIOErrorSHMSize = ResultCode::kIOError | (19 << 8),            // 4874
  kIOErrorSHMLock = ResultCode::kIOError | (20 << 8),            // 5130
  kIOErrorSHMMap = ResultCode::kIOError | (21 << 8),             // 5386
  kIOErrorSeek = ResultCode::kIOError | (22 << 8),               // 5642
  kIOErrorDeleteNoEnt = ResultCode::kIOError | (23 << 8),        // 5898
  kIOErrorMMap = ResultCode::kIOError | (24 << 8),               // 6154
  kIOErrorGetTempPath = ResultCode::kIOError | (25 << 8),        // 6410
  kIOErrorConvPath = ResultCode::kIOError | (26 << 8),           // 6666
  kIOErrorVNode = ResultCode::kIOError | (27 << 8),              // 6922
  kIOErrorAuth = ResultCode::kIOError | (28 << 8),               // 7178
  kIOErrorBeginAtomic = ResultCode::kIOError | (29 << 8),        // 7434
  kIOErrorCommitAtomic = ResultCode::kIOError | (30 << 8),       // 7690
  kIOErrorRollbackAtomic = ResultCode::kIOError | (31 << 8),     // 7946
  kIOErrorData = ResultCode::kIOError | (32 << 8),               // 8202
  kIOErrorCorruptFS = ResultCode::kIOError | (33 << 8),          // 8458

  // Extended Result Code: Locked
  kLockedSharedCache = ResultCode::kLocked | (1 << 8),  // 262
  kLockedVTab = ResultCode::kLocked | (2 << 8),         // 518

  // Extended Result Code: Busy
  kBusyRecovery = ResultCode::kBusy | (1 << 8),  // 261
  kBusySnapshot = ResultCode::kBusy | (2 << 8),  // 517
  kBusyTimeout = ResultCode::kBusy | (3 << 8),   // 773

  // Extended Result Code: CantOpen
  kCantOpenNoTempDir = ResultCode::kCantOpen | (1 << 8),  // 270
  kCantOpenIsDir = ResultCode::kCantOpen | (2 << 8),      // 526
  kCantOpenFullPath = ResultCode::kCantOpen | (3 << 8),   // 782
  kCantOpenConvPath = ResultCode::kCantOpen | (4 << 8),   // 1038
  kCantOpenDirtyWAL = ResultCode::kCantOpen | (5 << 8),   // 1294
  kCantOpenSymlink = ResultCode::kCantOpen | (6 << 8),    // 1550

  // Extended Result Code: Corrupt
  kCorruptVTab = ResultCode::kCorrupt | (1 << 8),      // 267
  kCorruptSequence = ResultCode::kCorrupt | (2 << 8),  // 523
  kCorruptIndex = ResultCode::kCorrupt | (3 << 8),     // 779

  // Extended Result Code: ReadOnly
  kReadOnlyRecovery = ResultCode::kReadOnly | (1 << 8),   // 264
  kReadOnlyCantLock = ResultCode::kReadOnly | (2 << 8),   // 520
  kReadOnlyRollback = ResultCode::kReadOnly | (3 << 8),   // 776
  kReadOnlyDbMoved = ResultCode::kReadOnly | (4 << 8),    // 1032
  kReadOnlyCantInit = ResultCode::kReadOnly | (5 << 8),   // 1288
  kReadOnlyDirectory = ResultCode::kReadOnly | (6 << 8),  // 1544

  // Extended Result Code: Abort
  kAbortRollback = ResultCode::kAbort | (2 << 8),  // 516

  // Extended Result Code: Constraint
  kConstraintCheck = ResultCode::kConstraint | (1 << 8),       // 275
  kConstraintCommitHook = ResultCode::kConstraint | (2 << 8),  // 531
  kConstraintForeignKey = ResultCode::kConstraint | (3 << 8),  // 787
  kConstraintFunction = ResultCode::kConstraint | (4 << 8),    // 1034
  kConstraintNotNull = ResultCode::kConstraint | (5 << 8),     // 1299
  kConstraintPrimaryKey = ResultCode::kConstraint | (6 << 8),  // 1555
  kConstraintTrigger = ResultCode::kConstraint | (7 << 8),     // 1811
  kConstraintUnique = ResultCode::kConstraint | (8 << 8),      // 2067
  kConstraintVTab = ResultCode::kConstraint | (9 << 8),        //  2323
  kConstraintRowID = ResultCode::kConstraint | (10 << 8),      // 2579
  kConstraintPinned = ResultCode::kConstraint | (11 << 8),     // 2835
  kConstraintDataType = ResultCode::kConstraint | (12 << 8),   // 3091

  // Extended Result Code: Notice
  kNoticeRecoverWal = ResultCode::kNotice | (1 << 8),       // 283
  kNoticeRecoverRollback = ResultCode::kNotice | (2 << 8),  // 539
  kNoticeRBU = ResultCode::kNotice | (3 << 8),              // 795

  // Extended Result Code: Warning
  kWarningAutoIndex = ResultCode::kWarning | (1 << 8),  // 284

  // Extended Result Code: Auth
  kAuthUser = ResultCode::kAuth | (1 << 8),  // 279

  // Extended Result Code: Ok
  kOkLoadPermanently = ResultCode::kOk | (1 << 8),  // 256
  kOkSymlink = ResultCode::kOk | (2 << 8)           // 512
};

/*
 * ResultCodeToString(const ResultCode &code)
 * Returns the name for the primary result code, or "" if it is an unknown
 * value.
 */
std::string ToString(const ResultCode &code);

/*
 * operator<<
 * Streams ResultCodeToString(ResultCode code); to `os`.
 */
std::ostream &operator<<(std::ostream &os, const ResultCode &code);

/*
 * GetPrimaryResultCode(const ExtendedResultCode &code)
 * Returns the primary result code for the extended result code.
 */
ResultCode GetPrimaryResultCode(const ResultCode &code);

class SqliteException : public std::exception {
 public:
  SqliteException(ResultCode code) : code_(code) {}

  ResultCode code() const { return code_; }

 private:
  ResultCode code_;
};
