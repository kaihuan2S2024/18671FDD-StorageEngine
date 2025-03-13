#include <fcntl.h>
#include <sys/stat.h>

#include <cassert>
#include <iostream>

#include "os.h"

// Initialize the static member variables
bool OsFile::in_mutex_ = false;

// FindLockInfo and ReleaseLockInfo are only used in UNIX
// to implement file locking of different threads in the same process.
#if OS_UNIX
void OsFile::FindLockInfo() {
  int rc_int;
  struct stat stat_buf{};
  rc_int = fstat(fd_, &stat_buf);
  if (rc_int != 0) {
    lock_info_ptr_ = std::weak_ptr<LockInfo>{};
    return;  // Return early since there's no valid LockInfo
  }
  InodeKey key{};
  key.dev = stat_buf.st_dev;
  key.ino = stat_buf.st_ino;
  auto it = lock_info_map_.find(key);
  if (it == lock_info_map_.end()) {
    // Create a new lock info and insert its shared pointer into the map
    std::shared_ptr<LockInfo> info_ptr = std::make_shared<LockInfo>();
    info_ptr->key = key;
    info_ptr->cnt = 0;
    info_ptr->num_ref = 0;
    it = lock_info_map_.insert({key, info_ptr}).first;
  }
  lock_info_ptr_ = it->second;
  lock_info_ptr_.lock()->num_ref++;
}

void OsFile::ReleaseLockInfo() {
  if (lock_info_ptr_.expired()) return;
  lock_info_ptr_.lock()->num_ref--;
  if (lock_info_ptr_.lock()->num_ref == 0) {
    lock_info_map_.erase(lock_info_ptr_.lock()->key);
  }
}
#endif

// Acquires a read lock on the file
ResultCode OsFile::OsReadLock() {
#if OS_UNIX
  if (lock_info_ptr_.expired()) {
    return ResultCode::kError;
  }
  ResultCode rc;

  OsEnterMutex();
  if (lock_info_ptr_.lock()->cnt > 0) {
    lock_info_ptr_.lock()->cnt++;
    rc = ResultCode::kOk;
  } else if (lock_info_ptr_.lock()->cnt == 0) {
    struct flock lock{};
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = lock.l_len = 0L;
    if (fcntl(fd_, F_SETLK, &lock) != 0) {
      rc = ResultCode::kBusy;
    } else {
      rc = ResultCode::kOk;
      lock_info_ptr_.lock()->cnt = 1;
      locked_ = true;
    }
  } else {
    rc = ResultCode::kBusy;
  }
  OsLeaveMutex();
  return rc;
#endif

#if OS_WIN
  ResultCode rc;
  if (locked_) {
    rc = ResultCode::kOk;
  } else if (LockFile(h_, 0, 0, 1024, 0)) {
    rc = ResultCode::kOk;
    locked_ = true;
  } else {
    rc = ResultCode::kBusy;
  }
  return rc;
#endif
}

// Acquires a write lock on the file
ResultCode OsFile::OsWriteLock() {
#if OS_UNIX
  if (lock_info_ptr_.expired()) {
    return ResultCode::kError;
  }
  ResultCode rc;

  OsEnterMutex();
  if (lock_info_ptr_.lock()->cnt == 0 || lock_info_ptr_.lock()->cnt >= 1) {
    struct flock lock{};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = lock.l_len = 0L;
    if (fcntl(fd_, F_SETLK, &lock) != 0) {
      rc = ResultCode::kBusy;
    } else {
      rc = ResultCode::kOk;
      lock_info_ptr_.lock()->cnt = -1;
      locked_ = true;
    }
  } else {
    rc = ResultCode::kBusy;
  }

  OsLeaveMutex();
  return rc;
#endif

#if OS_WIN
  ResultCode rc;
  if (locked_) {
    rc = ResultCode::kOk;
  } else if (LockFile(h_, 0, 0, 1024, 0)) {
    rc = ResultCode::kOk;
    locked_ = true;
  } else {
    rc = ResultCode::kBusy;
  }
  return rc;
#endif
}

// Releases the lock held on the file
ResultCode OsFile::OsUnlock() {
#if OS_UNIX
  if (lock_info_ptr_.expired() || (lock_info_ptr_.lock()->cnt == 0)) {
    return ResultCode::kError;
  }
  ResultCode rc;

  OsEnterMutex();
  if (lock_info_ptr_.lock()->cnt > 0) {
    lock_info_ptr_.lock()->cnt--;
    rc = ResultCode::kOk;
  } else {
    struct flock lock{};
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = lock.l_len = 0L;
    if (fcntl(fd_, F_SETLK, &lock) != 0) {
      rc = ResultCode::kBusy;
    } else {
      rc = ResultCode::kOk;
      lock_info_ptr_.lock()->cnt = 0;
    }
  }
  OsLeaveMutex();
  locked_ = false;
  return rc;
#endif

#if OS_WIN
  ResultCode rc;
  if (!locked_) {
    rc = ResultCode::kOk;
  } else if (UnlockFile(h_, 0, 0, 1024, 0)) {
    rc = ResultCode::kOk;
    locked_ = false;
  } else {
    rc = ResultCode::kBusy;
  }
  return rc;
#endif
}

/*
 * Macros that determine the use of threads.
 * If SQLITE_UNIX_THREADS is defined, we are using posix threads.
 * If SQLITE_WIN_THREADS is defined, we are using Win32 threads.
 */
#if OS_UNIX && defined(THREADSAFE) && THREADSAFE
#define SQLITE_UNIX_THREADS 1
#endif

#if OS_WIN && defined(THREADSAFE) && THREADSAFE
#define SQLITE_WIN32_THREADS 1
#endif

#if SQLITE_UNIX_THREADS
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

#if SQLITE_WIN32_THREADS
static CRITICAL_SECTION cs;
#endif

// Enters a mutex to provide thread safety
void OsFile::OsEnterMutex() {
#if SQLITE_UNIX_THREADS
  pthread_mutex_lock(&mutex);
#endif

#if SQLITE_WIN32_THREADS
  static bool is_init = false;
  while (!is_init) {
    static long lock = 0;
    if (InterlockedIncrement(&lock) == 1) {
      InitializeCriticalSection(&cs);
      is_init = true;
    } else {
      Sleep(1);
    }
  }
  EnterCriticalSection(&cs);
#endif
  assert(!in_mutex_);
  in_mutex_ = true;
}

// Leaves a mutex, releasing thread safety
void OsFile::OsLeaveMutex() {
  assert(in_mutex_);
  in_mutex_ = false;
#if SQLITE_UNIX_THREADS
  pthread_mutex_unlock(&mutex);
#endif

#if SQLITE_WIN32_THREADS
  LeaveCriticalSection(&cs);
#endif
}
