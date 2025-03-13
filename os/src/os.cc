/*
 * os.cc
 */

#include "os.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctime>
#include <fstream>
#include <ostream>
#include <iostream>
#include <filesystem>
#include <array>
#include <cstddef>
#include <cstring>
#include "sql_limit.h"

/*
** Macros for performance tracing.  Normally turned off
*/
#if 0
static int last_page = 0;
#define SEEK(X)     last_page=(X)
#define TRACE1(X)   fprintf(stderr,X)
#define TRACE2(X,Y) fprintf(stderr,X,Y)
#else
#define SEEK(X)
#define TRACE1(X)
#define TRACE2(X, Y)
#endif

/*
** If we compile with the SQLITE_TEST macro set, then the following block
** of code will give us the ability to simulate a disk I/O error.  This
** is used for testing the I/O recovery logic.
*/
#ifdef SQLITE_TEST
int sqlite_io_error_pending = 0;
#define SimulateIOError(A)  \
   if( sqlite_io_error_pending ) \
     if( sqlite_io_error_pending-- == 1 ){ local_ioerr(); return A; }
static void local_ioerr(){
  sqlite_io_error_pending = 0;  /* Really just a place to set a breakpoint */
}
#else
#define SimulateIOError(A)
#endif

#if OS_UNIX
// Lock info is only used in Unix
std::unordered_map<InodeKey,
                   std::shared_ptr<LockInfo>,
                   InodeKey::InodeKeyHashFunction,
                   InodeKey::InodeKeyEqualFunction>
    OsFile::lock_info_map_{};

std::size_t InodeKey::InodeKeyHashFunction::operator()(const InodeKey &key) const {
  size_t h1 = std::hash<dev_t>{}(key.dev);
  size_t h2 = std::hash<ino_t>{}(key.ino);
  return h1 ^ h2;
}
#endif

// Default constructor for OsFile (Unix version)
#if OS_UNIX
OsFile::OsFile() {

  fd_ = -1;
  locked_ = false;
}
#endif

// Default constructor for OsFile (Windows version)
#if OS_WIN
OsFile::OsFile() {

  h_ = INVALID_HANDLE_VALUE;
  locked_ = false;
}
#endif

// Call OsFile() and set the filename
OsFile::OsFile(std::string &filename) : OsFile() {
  filename_ = filename;
}

ResultCode OsFile::OsDelete() {
#if OS_UNIX
  unlink(filename_.c_str());
#endif

#if OS_WIN
  DeleteFile(filename_.c_str());
#endif

  return ResultCode::kOk;
}

// Checks if the file associated with this OsFile object exists
ResultCode OsFile::OsFileExists() {
#if OS_UNIX
  return access(filename_.c_str(), 0) == 0 ? ResultCode::kOk : ResultCode::kError;
#endif

#if OS_WIN
  return GetFileAttributes(filename_.c_str()) != 0xFFFFFFFF ? ResultCode::kOk : ResultCode::kError;
#endif
}

// Opens a file exclusively, creating it if it doesn't exist
ResultCode OsFile::OsOpenExclusive(int del_flag) {
#if OS_UNIX
  // The filename was originally passed into the arguments
  // However, since we could pass the filename in the constructor,
  // we could simply access the member variable filename_.
  if (access(filename_.c_str(), 0) == 0) {
    return ResultCode::kCantOpen;
  }
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
  fd_ = open(filename_.c_str(), O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (fd_ < 0) {
    return ResultCode::kCantOpen;
  }
  // Find the lock info
  OsEnterMutex();
  FindLockInfo();
  OsLeaveMutex();
  if (lock_info_ptr_.expired()) {
    close(fd_);
    return ResultCode::kNoMem;
  }
  locked_ = false;
  if (del_flag) {
    unlink(filename_.c_str());
  }
  return ResultCode::kOk;
#endif
#if OS_WIN

  HANDLE h;
  int file_flags;
  if (del_flag) {
    file_flags = FILE_ATTRIBUTE_TEMPORARY |
        FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_DELETE_ON_CLOSE;
  } else {
    file_flags = FILE_FLAG_RANDOM_ACCESS;
  }
  h = CreateFile(
      filename_.c_str(), GENERIC_READ | GENERIC_WRITE,
      0,
      nullptr,
      CREATE_ALWAYS,
      file_flags,
      nullptr
  );
  if (h == INVALID_HANDLE_VALUE) {
    return ResultCode::kCantOpen;
  }
  h_ = h;
  locked_ = false;
  return ResultCode::kOk;
#endif
}

// Opens a file in read-only mode
ResultCode OsFile::OsOpenReadOnly(const std::string &filename) {
#if OS_UNIX
  filename_ = filename;
  fd_ = open(filename_.c_str(), O_RDONLY);
  if (fd_ < 0) {
    return ResultCode::kCantOpen;
  }
  // Find the lock info
  OsEnterMutex();
  FindLockInfo();
  OsLeaveMutex();
  if (lock_info_ptr_.expired()) {
    close(fd_);
    return ResultCode::kNoMem;
  }
  locked_ = false;
  return ResultCode::kOk;
#endif
#if OS_WIN
  HANDLE h;
  filename_ = filename;
  h = CreateFile(
      filename_.c_str(),
      GENERIC_READ,
      0,
      nullptr,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
      nullptr
  );
  if (h == INVALID_HANDLE_VALUE) {
    return ResultCode::kCantOpen;
  }
  h_ = h;
  locked_ = false;
  return ResultCode::kOk;
#endif
}

// Opens a file in read-write mode, setting a flag if it's opened read-only
ResultCode OsFile::OsOpenReadWrite(const std::string &filename, bool &read_only) {
#if OS_UNIX
  filename_ = filename;
  // The O_CREAT flag is used to create the file if it does not exist.
  fd_ = open(filename_.c_str(), O_RDWR | O_CREAT, 0644);
  if (fd_ < 0) {
    fd_ = open(filename.c_str(), O_RDONLY);
    if (fd_ < 0) {
      return ResultCode::kCantOpen;
    }
    read_only = true;
  } else {
    read_only = false;
  }
  // Find the lock info
  OsEnterMutex();
  FindLockInfo();
  OsLeaveMutex();
  if (lock_info_ptr_.expired()) {
    close(fd_);
    return ResultCode::kNoMem;
  }
  locked_ = false;
  return ResultCode::kOk;
#endif

#if OS_WIN
  filename_ = filename;
  HANDLE h = CreateFile(
      filename_.c_str(),
      GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      nullptr,
      OPEN_ALWAYS,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
      nullptr
  );
  if (h == INVALID_HANDLE_VALUE) {
    h = CreateFile(filename.c_str(),
                   GENERIC_READ,
                   FILE_SHARE_READ,
                   nullptr,
                   OPEN_ALWAYS,
                   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
                   nullptr);
    if (h == INVALID_HANDLE_VALUE) {
      return ResultCode::kCantOpen;
    }
    read_only = true;
  } else {
    read_only = false;
  }
  h_ = h;
  locked_ = false;
  return ResultCode::kOk;
#endif
}

// Reads data from the file into a vector
ResultCode OsFile::OsRead(std::vector<std::byte> &data) {
  return OsRead(data, data.size());
}

// Reads a specified amount of data from the file into a vector
ResultCode OsFile::OsRead(std::vector<std::byte> &data, u32 amount) {

  if (data.size() < amount) {
    data.resize(amount);
  }

#if OS_UNIX
  int got;
  got = read(fd_, data.data(), amount);
  if (got < 0) {
    return ResultCode::kIOError;
  }
  if (got < 0) {
    got = 0;
  }
  return (u32) got == amount ? ResultCode::kOk : ResultCode::kIOError;
#endif

#if OS_WIN
  DWORD got;
  if (!ReadFile(h_, data.data(), amount, &got, nullptr)) {
    got = 0;
  }
  return (u32) got == amount ? ResultCode::kOk : ResultCode::kIOError;
#endif
}

// Writes data from a vector to the file
ResultCode OsFile::OsWrite(const std::vector<std::byte> &data) {
  return OsWrite(data, data.size());
}

/*
 * Writes an array of kPageSize to a file
 *
 * This function was added to such that p_image could be directly written
 * into the file without having to convert it into a vector of bytes.
 *
 */
ResultCode OsFile::OsWrite(const std::array<std::byte, kPageSize> &data) {
#if OS_UNIX
  int wrote;
  wrote = write(fd_, data.data(), kPageSize);
  if (wrote < 0) {
    return ResultCode::kFull;
  }
  return ResultCode::kOk;
#endif

#if OS_WIN
  DWORD wrote;
  if (!WriteFile(h_, data.data(), kPageSize, &wrote, nullptr)) {
    return ResultCode::kFull;
  }
  return ResultCode::kOk;
#endif

}

// Writes a specified amount of data from a vector to the file
ResultCode OsFile::OsWrite(const std::vector<std::byte> &data, u32 amount) {

#if OS_UNIX
  int wrote;
  wrote = write(fd_, data.data(), amount);
  if (wrote < 0) {
    return ResultCode::kFull;
  }
  return (u32) wrote == amount ? ResultCode::kOk : ResultCode::kIOError;
#endif

#if OS_WIN
  DWORD wrote;
  if (!WriteFile(h_, data.data(), amount, &wrote, nullptr)) {
    return ResultCode::kFull;
  }
  return (u32) wrote == amount ? ResultCode::kOk : ResultCode::kIOError;
#endif

}

// Displays the file contents to the standard output
ResultCode OsFile::OsDisplay() {

#if OS_UNIX
  int got;
  char buf[1024];
  while ((got = read(fd_, buf, sizeof(buf))) > 0) {
    fwrite(buf, 1, got, stdout);
  }
  return ResultCode::kOk;

#endif

#if OS_WIN
  DWORD got;
  char buf[1024];
  while (ReadFile(h_, buf, sizeof(buf), &got, nullptr) && got > 0) {
    fwrite(buf, 1, got, stdout);
  }
  return ResultCode::kOk;
#endif
}

// Closes the file associated with this OsFile object
ResultCode OsFile::OsClose() {

#if OS_UNIX
  close(fd_);
  OsEnterMutex();
  ReleaseLockInfo();
  OsLeaveMutex();
#endif

#if OS_WIN
  CloseHandle(h_);
#endif
  return ResultCode::kOk;
}

// Seeks to a specific offset in the file
ResultCode OsFile::OsSeek(u32 offset) {
  // SEEK(offset / 1024 + 1);

#if OS_UNIX
  lseek(fd_, offset, SEEK_SET);
#endif

#if OS_WIN
  SetFilePointer(h_, offset, nullptr, FILE_BEGIN);
#endif
  return ResultCode::kOk;
}

// Returns how many bytes from the beginning of the file
unsigned long OsFile::GetCurrentPosition() {

#if OS_UNIX
  unsigned long pos;
  pos = lseek(fd_, 0, SEEK_CUR);
  return pos;
#endif

#if OS_WIN
  DWORD pos = SetFilePointer(h_, 0, nullptr, FILE_CURRENT);
  return (unsigned long) pos;
#endif

}

// Synchronizes the file's in-memory state with the storage device
ResultCode OsFile::OsSync() {
#if OS_UNIX
  return fsync(fd_) == 0 ? ResultCode::kOk : ResultCode::kIOError;
#endif

#if OS_WIN
  return FlushFileBuffers(h_) ? ResultCode::kOk : ResultCode::kIOError;
#endif
}

// Truncates the file to a specified size
ResultCode OsFile::OsTruncate(u32 size) {
#if OS_UNIX
  return ftruncate(fd_, size) == 0 ? ResultCode::kOk : ResultCode::kIOError;
#endif

#if OS_WIN
  SetFilePointer(h_, (long) size, nullptr, FILE_BEGIN);
  SetEndOfFile(h_);
  return ResultCode::kOk;
#endif
}

// Gets the size of the file
ResultCode OsFile::OsFileSize(u32 &size) {
#if OS_UNIX
  struct stat buf{};
  if (fstat(fd_, &buf) != 0) {
    return ResultCode::kIOError;
  }
  size = buf.st_size;
  return ResultCode::kOk;

#endif

#if OS_WIN
  size = GetFileSize(h_, nullptr);
  return ResultCode::kOk;
#endif
}

// Puts the calling thread to sleep for a specified duration
ResultCode OsFile::OsSleep(int millis) {
  usleep(millis * 1000);
  return ResultCode::kOk;
}

// Generates a random seed for use in other operations
ResultCode OsRandomSeed(std::array<std::byte, kRandomSeedBufferSize> &random_seed) {
  static bool have_not_set_seed = true;
#if OS_UNIX
  int pid;
  time((time_t *) random_seed.begin());
  pid = getpid();
  memcpy(&random_seed[sizeof(time_t)], &pid, sizeof(pid));
#endif
#if OS_WIN
  GetSystemTime((LPSYSTEMTIME) random_seed.begin());
#endif
  if (have_not_set_seed) {
    int seed;
    std::memcpy(&seed, random_seed.begin(), sizeof(seed));
    srand(seed);
    have_not_set_seed = false;
  }
  return ResultCode::kOk;
}
