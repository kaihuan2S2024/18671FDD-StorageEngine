/*
 * result_code.cc
 *
 * Implements the functions declared in result_code.h.
 * The functions are based on this example:
 * https://github.com/abseil/abseil-cpp/blob/master/absl/status/status.cc
 */

#include "sql_rc.h"

// Converts a ResultCode to a string.
std::string ToString(const ResultCode &code) {
  switch (code) {
	// Primary Result Code
	case ResultCode::kOk: return "OK";
	case ResultCode::kError: return "ERROR";
	case ResultCode::kInternal: return "INTERNAL";
	case ResultCode::kPerm: return "PERM";
	case ResultCode::kAbort: return "ABORT";
	case ResultCode::kBusy: return "BUSY";
	case ResultCode::kLocked: return "LOCKED";
	case ResultCode::kNoMem: return "NO_MEM";
	case ResultCode::kReadOnly: return "READ_ONLY";
	case ResultCode::kInterrupt: return "INTERRUPT";
	case ResultCode::kIOError: return "IO_ERROR";
	case ResultCode::kCorrupt: return "CORRUPT";
	case ResultCode::kNotFound: return "NOTFOUND";
	case ResultCode::kFull: return "FULL";
	case ResultCode::kCantOpen: return "CANT_OPEN";
	case ResultCode::kProtocol: return "PROTOCOL";
	case ResultCode::kEmpty: return "EMPTY";
	case ResultCode::kSchema: return "SCHEMA";
	case ResultCode::kTooBig: return "TOO_BIG";
	case ResultCode::kConstraint: return "CONSTRAINT";
	case ResultCode::kMismatch: return "MISMATCH";
	case ResultCode::kMisuse: return "MISUSE";
	case ResultCode::kNoLFS: return "NO_LFS";
	case ResultCode::kAuth: return "AUTH";
	case ResultCode::kFormat: return "FORMAT";
	case ResultCode::kRange: return "RANGE";
	case ResultCode::kNotADB: return "NOT_ADB";
	case ResultCode::kNotice: return "NOTICE";
	case ResultCode::kWarning: return "WARNING";
	case ResultCode::kRow: return "ROW";
	case ResultCode::kDone: return "DONE";

	  // Extended Result Code: Error
	case ResultCode::kErrorMissingCollSeq: return "ERROR_MISSING_COLL_SEQ";
	case ResultCode::kErrorRetry: return "ERROR_RETRY";
	case ResultCode::kErrorSnapshot: return "ERROR_SNAPSHOT";

	  // Extended Result Code: IO Error
	case ResultCode::kIOErrorRead: return "IO_ERROR_READ";
	case ResultCode::kIOErrorShortRead: return "IO_ERROR_SHORT_READ";
	case ResultCode::kIOErrorWrite: return "IO_ERROR_WRITE";
	case ResultCode::kIOErrorFsync: return "IO_ERROR_FSYNC";
	case ResultCode::kIOErrorDirFSync: return "IO_ERROR_DIR_FSYNC";
	case ResultCode::kIOErrorTruncate: return "IO_ERROR_TRUNCATE";
	case ResultCode::kIOErrorFStat: return "IO_ERROR_FSTAT";
	case ResultCode::kIOErrorUnlock: return "IO_ERROR_UNLOCK";
	case ResultCode::kIOErrorRDLock: return "IO_ERROR_RD_LOCK";
	case ResultCode::kIOErrorDelete: return "IO_ERROR_DELETE";
	case ResultCode::kIOErrorBlocked: return "IO_ERROR_BLOCKED";
	case ResultCode::kIOErrorNoMem: return "IO_ERROR_NOMEM";
	case ResultCode::kIOErrorAccess: return "IO_ERROR_ACCESS";
	case ResultCode::kIOErrorCheckReservedLock: return "IO_ERROR_CHECK_RESERVED_LOCK";
	case ResultCode::kIOErrorLock: return "IO_ERROR_LOCK";
	case ResultCode::kIOErrorClose: return "IO_ERROR_CLOSE";
	case ResultCode::kIOErrorDirClose: return "IO_ERROR_DIR_CLOSE";
	case ResultCode::kIOErrorSHMOpen: return "IO_ERROR_SHM_OPEN";
	case ResultCode::kIOErrorSHMSize: return "IO_ERROR_SHM_SIZE";
	case ResultCode::kIOErrorSHMLock: return "IO_ERROR_SHM_LOCK";
	case ResultCode::kIOErrorSHMMap: return "IO_ERROR_SHM_MAP";
	case ResultCode::kIOErrorSeek: return "IO_ERROR_SEEK";
	case ResultCode::kIOErrorDeleteNoEnt: return "IO_ERROR_DELETE_NO_ENT";
	case ResultCode::kIOErrorMMap: return "IO_ERROR_MMAP";
	case ResultCode::kIOErrorGetTempPath: return "IO_ERROR_GET_TEMP_PATH";
	case ResultCode::kIOErrorConvPath: return "IO_ERROR_CON_V_PATH";
	case ResultCode::kIOErrorVNode: return "IO_ERROR_V_NODE";
	case ResultCode::kIOErrorAuth: return "IO_ERROR_AUTH";
	case ResultCode::kIOErrorBeginAtomic: return "IO_ERROR_BEGIN_ATOMIC";
	case ResultCode::kIOErrorCommitAtomic: return "IO_ERROR_COMMIT_ATOMIC";
	case ResultCode::kIOErrorRollbackAtomic: return "IO_ERROR_ROLLBACK_ATOMIC";
	case ResultCode::kIOErrorData: return "IO_ERROR_DATA";
	case ResultCode::kIOErrorCorruptFS: return "IO_ERROR_CORRUPT_FS";

	  // Extended Result Code: Locked
	case ResultCode::kLockedSharedCache: return "LOCKED_SHARED_CACHE";
	case ResultCode::kLockedVTab: return "LOCKED_VTAB";

	  // Extended Result Code: Busy
	case ResultCode::kBusyRecovery: return "BUSY_RECOVERY";
	case ResultCode::kBusySnapshot: return "BUSY_SNAPSHOT";
	case ResultCode::kBusyTimeout: return "BUSY_TIMEOUT";

	  // Extended Result Code: CantOpen
	case ResultCode::kCantOpenNoTempDir: return "CANT_OPEN_NO_TEMP_DIR";
	case ResultCode::kCantOpenIsDir: return "CANT_OPEN_ISDIR";
	case ResultCode::kCantOpenFullPath: return "CANT_OPEN_FULL_PATH";
	case ResultCode::kCantOpenConvPath: return "CANT_OPEN_CONV_PATH";
	case ResultCode::kCantOpenDirtyWAL: return "CANT_OPEN_DIRTY_WAL";
	case ResultCode::kCantOpenSymlink: return "CANT_OPEN_SYMLINK";

	  // Extended Result Code: Corrupt
	case ResultCode::kCorruptVTab: return "CORRUPT_VTAB";
	case ResultCode::kCorruptSequence: return "CORRUPT_SEQUENCE";
	case ResultCode::kCorruptIndex: return "CORRUPT_INDEX";

	  // Extended Result Code: ReadOnly
	case ResultCode::kReadOnlyRecovery: return "READONLY_RECOVERY";
	case ResultCode::kReadOnlyCantLock: return "READONLY_CANT_LOCK";
	case ResultCode::kReadOnlyRollback: return "READONLY_ROLLBACK";
	case ResultCode::kReadOnlyDbMoved: return "READONLY_DB_MOVED";
	case ResultCode::kReadOnlyCantInit: return "READONLY_CANT_INIT";
	case ResultCode::kReadOnlyDirectory: return "READONLY_DIRECTORY";

	  // Extended Result Code: Abort
	case ResultCode::kAbortRollback: return "ABORT_ROLLBACK";

	  // Extended Result Code: Constraint
	case ResultCode::kConstraintCheck: return "CONSTRAINT_CHECK";
	case ResultCode::kConstraintCommitHook: return "CONSTRAINT_COMMIT_HOOK";
	case ResultCode::kConstraintForeignKey: return "CONSTRAINT_FOREIGN_KEY";
	case ResultCode::kConstraintFunction: return "CONSTRAINT_FUNCTION";
	case ResultCode::kConstraintNotNull: return "CONSTRAINT_NOTNULL";
	case ResultCode::kConstraintPrimaryKey: return "CONSTRAINT_PRIMARY_KEY";
	case ResultCode::kConstraintTrigger: return "CONSTRAINT_TRIGGER";
	case ResultCode::kConstraintUnique: return "CONSTRAINT_UNIQUE";
	case ResultCode::kConstraintVTab: return "CONSTRAINT_VTAB";
	case ResultCode::kConstraintRowID: return "CONSTRAINT_ROWID";
	case ResultCode::kConstraintPinned: return "CONSTRAINT_PINNED";
	case ResultCode::kConstraintDataType: return "CONSTRAINT_DATA_TYPE";

	  // Extended Result Code: Notice
	case ResultCode::kNoticeRecoverWal: return "NOTICE_RECOVER_WAL";
	case ResultCode::kNoticeRecoverRollback: return "NOTICE_RECOVER_ROLLBACK";
	case ResultCode::kNoticeRBU: return "NOTICE_RBU";

	  // Extended Result Code: WarningC
	case ResultCode::kWarningAutoIndex: return "WARNING_AUTO_INDEX";

	  // Extended Result Codes: Auth
	case ResultCode::kAuthUser: return "AUTH_USER";

	  // Extended Result Codes: Ok
	case ResultCode::kOkLoadPermanently: return "OK_LOAD_PERMANENTLY";
	case ResultCode::kOkSymlink: return "OK_SYMLINK";

	default: return "";
  }
}

// Converts a ResultCode to a string and writes it to the given stream.
std::ostream &operator<<(std::ostream &os, const ResultCode &code) {
  return os << ToString(code);
}

// Converts an ResultCode to a ResultCode.
ResultCode GetPrimaryResultCode(const ResultCode &code) {
  // Get the last 8 bits of the extended result code and cast it to a ResultCode
  return static_cast<ResultCode>( u32(code) & 0xFF);
}
