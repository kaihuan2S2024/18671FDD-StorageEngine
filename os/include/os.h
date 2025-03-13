#pragma once

/*
 * os.h
 *
 * This file is the header file containing the OsFile class definition.
 *
 * The OsFile class represents a file operated on by the database system.
 * All the operations on the file are represented by member functions of the
 * class.
 *
 * The majority of the I/O operations are implemented using the native OS file
 * system.
 *
 * In UNIX, files are opened using the open() system call, and are closed using
 * the close() system call. File descriptors (the fd_ private variable in the
 * OsFile class) are used to refer to the files. File descriptors and fcntl are
 * also used to handle locking and unlocking of files. The additional usage of a
 * hashmap to maintain locks is to compensate for complications with POSIX file
 * locking. It is implied inANSI STD 1003.1 (1996) section 6.5.2.2 that any when
 * a process sets or clears a lock, that operation overrides any prior locks set
 * by the same process. Therefore, we need to maintain a hashmap of locks to
 * ensure that the correct lock is released.
 *
 * In Windows, files are opened using the CreateFile() function, and are closed
 * using the CloseHandle() function. The issue mentioned above with POSIX file
 * locking does not exist in Windows, so the additional hashmap is not needed.
 *
 *
 */

#include <string>
#include <unordered_map>

#include "sql_rc.h"

#ifndef OS_UNIX
#ifndef OS_WIN
#if defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || \
    defined(__MINGW32__) || defined(__BORLANDC__)
#define OS_WIN 1
#define OS_UNIX 0
#else
#define OS_WIN 0
#define OS_UNIX 1
#endif
#else
#define OS_UNIX 0
#endif
#endif
#ifndef OS_WIN
#define OS_WIN 0
#endif

#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

#include "sql_int.h"
#include "sql_limit.h"

// In the following OS_UNIX specific classes are defined to work around POSIX
// file locking. POSIX locks will work fine to synchronize access for threads in
// separate processes, but not threads within the same process. Therefore,
// additional locking logic is needed for UNIX systems.
#if OS_UNIX
struct InodeKey {
  dev_t dev; /* Device number, typedef int */
  ino_t ino; /* Inode number, typedef unsigned long long */
  struct InodeKeyHashFunction {
    std::size_t operator()(const InodeKey &key) const;
  };

  struct InodeKeyEqualFunction {
    bool operator()(const InodeKey &lhs, const InodeKey &rhs) const {
      return lhs.dev == rhs.dev && lhs.ino == rhs.ino;
    }
  };
};

class LockInfo {
  friend class OsFile;

 private:
  InodeKey key; /* The lookup key */
  int cnt;      /* 0: unlocked.  -1: write lock.  1: read lock. */
  int num_ref;  /* Number of pointers to this structure */
};
#endif

#if OS_WIN
#include "winbase.h"
#include "windows.h"
#endif

static constexpr unsigned int kRandomSeedBufferSize = 256;

class OsFile {
 private:
  bool locked_;          /* True if this user holds the lock */
  std::string filename_; /* Name of the file */

#if OS_UNIX
  std::weak_ptr<LockInfo> lock_info_ptr_;
  int fd_; /* The file descriptor */

  static std::unordered_map<InodeKey, std::shared_ptr<LockInfo>,
                            InodeKey::InodeKeyHashFunction,
                            InodeKey::InodeKeyEqualFunction>
      lock_info_map_;
#endif

#if OS_WIN
  HANDLE h_; /* Handle for accessing the file */
#endif

  static bool in_mutex_;

#if OS_UNIX
  void FindLockInfo();

  void ReleaseLockInfo();
#endif

 public:
  OsFile();                                // Constructor 1
  explicit OsFile(std::string &filename);  // Constructor 2

  // File operations
  ResultCode OsDelete();

  ResultCode OsFileExists();

  ResultCode OsOpenReadOnly(const std::string &filename);

  ResultCode OsOpenExclusive(int);

  ResultCode OsOpenReadWrite(const std::string &filename, bool &read_only);

  ResultCode OsRead(std::vector<std::byte> &data);

  ResultCode OsRead(std::vector<std::byte> &data, u32 amount);

  ResultCode OsWrite(const std::vector<std::byte> &data);

  ResultCode OsWrite(const std::vector<std::byte> &data, u32 amount);

  ResultCode OsWrite(const std::array<std::byte, kPageSize> &data);

  ResultCode OsDisplay();

  ResultCode OsClose();

  ResultCode OsSeek(u32 offset);

  unsigned long GetCurrentPosition();

  ResultCode OsSync();

  ResultCode OsTruncate(u32 size);

  ResultCode OsFileSize(u32 &size);

  ResultCode OsReadLock();

  ResultCode OsWriteLock();

  ResultCode OsUnlock();

  ResultCode OsRandomSeed(
      std::array<std::byte, kRandomSeedBufferSize> &random_seed);

  ResultCode OsSleep(int ms);

  static void OsEnterMutex();

  static void OsLeaveMutex();
};
