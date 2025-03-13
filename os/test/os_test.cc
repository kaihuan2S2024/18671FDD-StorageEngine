#include "os.h"

#include <ostream>
#include <fstream>

#include "gtest/gtest.h"

// Tests opening a single file for read/write access
TEST(OpenFile, SingleOpen) {
  std::string filename = "test_SingleOpen.db";
  std::remove(filename.c_str());
  OsFile file;
  bool read_only = false;
  ResultCode rc = file.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);
}

/* Tests opening a file in read-only mode
 * First ensures that opening a non-existent file fails
 * Then checks that writing to a read-only file fails */
TEST(OpenFile, OpenReadOnly) {
  std::string filename = "test_OpenReadOnly.db";
  std::remove(filename.c_str());
  OsFile read_only_file;

  // Calling OsReadOnly on a file that doesn't exist should return kCantOpen
  // Its implementation should not allow non-existent files to be opened
  ResultCode rc = read_only_file.OsOpenReadOnly(filename);
  EXPECT_EQ(ResultCode::kCantOpen, rc);

  OsFile read_write_file;
  bool read_only = false;
  rc = read_write_file.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);

  rc = read_write_file.OsClose();
  EXPECT_EQ(ResultCode::kOk, rc);

  rc = read_only_file.OsOpenReadOnly(filename);
  EXPECT_EQ(ResultCode::kOk, rc);

  EXPECT_EQ(ResultCode::kOk, rc);
  std::string exam = "This is a test";
  std::vector<std::byte> exam_bytes;
  for (char c : exam) {
    exam_bytes.push_back(static_cast<std::byte>(c));
  }

  // OsWrite should fail on a read-only file
  rc = read_only_file.OsWrite(exam_bytes);
  EXPECT_EQ(ResultCode::kFull, rc);
}

// Tests exclusively opening a non-existent file, expecting success
TEST(OpenFile, OpenExclusive) {
  std::string filename = "test_OpenExclusive.db";
  std::remove(filename.c_str());
  OsFile exclusive_file(filename);
  ResultCode rc;
  // Since the file does not exist, OsOpenExclusive should return kOk
  rc = exclusive_file.OsOpenExclusive(0);
  EXPECT_EQ(ResultCode::kOk, rc);
}

// Tests successfully closing an open file
TEST(CloseFile, SuccessfulClose) {
  std::string filename = "test_SuccessfulClose.db";
  std::remove(filename.c_str());
  OsFile file;
  bool read_only = false;
  ResultCode rc = file.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);

  rc = file.OsClose();
  EXPECT_EQ(ResultCode::kOk, rc);
}

// Tests successful deletion of an open file
TEST(DeleteFile, SuccessfulDelete) {
  std::string filename = "test_SuccessfulDelete.db";
  std::remove(filename.c_str());
  OsFile file;
  bool read_only = false;
  ResultCode rc = file.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);

  rc = file.OsDelete();
  EXPECT_EQ(ResultCode::kOk, rc);
}

// Tests the file existence check
// First checks non-existence, then confirms existence after creation
TEST(FileExists, SuccessfulFileExists) {
  std::string filename = "test_SuccessfulFileExists.db";
  std::remove(filename.c_str());
  OsFile file;
  bool read_only = false;

  ResultCode rc = file.OsFileExists();
  EXPECT_EQ(ResultCode::kError, rc);

  rc = file.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);

  rc = file.OsFileExists();
  EXPECT_EQ(ResultCode::kOk, rc);
}

// Tests successful seeking to the beginning of a file
TEST(Seek, SuccessfulSeek) {
  std::string filename = "test_SuccessfulSeek.db";
  std::remove(filename.c_str());
  OsFile file;
  bool read_only = false;
  ResultCode rc = file.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);

  rc = file.OsSeek(0);
  EXPECT_EQ(ResultCode::kOk, rc);
}

// Tests successful synchronization (flushing) of a file
TEST(Sync, SuccessfulSync) {
  std::string filename = "test_SuccessfulSync.db";
  std::remove(filename.c_str());
  OsFile file;
  bool read_only = false;
  ResultCode rc = file.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);

  rc = file.OsSync();
  EXPECT_EQ(ResultCode::kOk, rc);
}

// Tests successful truncation of a file to zero size
TEST(Truncate, SuccessfulTruncate) {
  std::string filename = "test_SuccessfulTruncate.db";
  std::remove(filename.c_str());
  OsFile file;
  bool read_only = false;
  ResultCode rc = file.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);

  rc = file.OsTruncate(0);
  EXPECT_EQ(ResultCode::kOk, rc);
}

// Tests successful retrieval of file size. Includes writing to the file and then checking the updated sizet
TEST(FileSize, SuccessfulGetFileSize) {
  std::string filename = "test_SuccessfulGetFileSize.db";
  std::remove(filename.c_str());
  OsFile file;
  bool read_only = false;
  ResultCode rc = file.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);

  u32 size;
  rc = file.OsFileSize(size);
  EXPECT_EQ(ResultCode::kOk, rc);
  EXPECT_EQ(0, size);

  std::string exam = "This is a test";
  std::vector<std::byte> exam_bytes;
  for (char c : exam) {
    exam_bytes.push_back(static_cast<std::byte>(c));
  }

  file.OsWrite(exam_bytes);
  EXPECT_EQ(ResultCode::kOk, rc);
  rc = file.OsFileSize(size);
  EXPECT_EQ(ResultCode::kOk, rc);
  EXPECT_EQ(14, size);
}

// Tests successful writing of string data and byte array to a file
TEST(WriteFile, SuccessfulWrite) {
  std::string filename = "test_SuccessfulWrite.db";
  std::remove(filename.c_str());
  OsFile file;
  bool read_only = false;
  ResultCode rc = file.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);

  std::string exam = "This is a test2";
  std::vector<std::byte> examBytes;
  for (char c : exam) {
    examBytes.push_back(static_cast<std::byte>(c));
  }

  rc = file.OsWrite(examBytes);
  EXPECT_EQ(ResultCode::kOk, rc);

  std::vector<std::byte> bytesData = {std::byte(0x41), std::byte(0x42), std::byte(0x43)};
  file.OsWrite(bytesData);
  EXPECT_EQ(ResultCode::kOk, rc);

  rc = file.OsDisplay();
  EXPECT_EQ(ResultCode::kOk, rc);
}


// Tests successful reading from a file
TEST(ReadFile, SuccessfulRead) {
  std::string filename = "test_SuccessfulRead.db";
  std::remove(filename.c_str());
  OsFile file;
  bool read_only = false;
  ResultCode rc = file.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);
  std::string exam = "This is a test";
  std::vector<std::byte> examBytes;
  for (char c : exam) {
    examBytes.push_back(static_cast<std::byte>(c));
  }
  rc = file.OsWrite(examBytes);
  EXPECT_EQ(ResultCode::kOk, rc);

  // Compares read data with initially written data for consistency
  std::vector<char> readBuf(examBytes.size());
  std::vector<std::byte> byteBuf(reinterpret_cast<std::byte *>(readBuf.data()),
                                 reinterpret_cast<std::byte *>(readBuf.data() + readBuf.size()));
  file.OsSeek(0);
  rc = file.OsRead(byteBuf);

  EXPECT_EQ(ResultCode::kOk, rc);
  EXPECT_EQ(byteBuf, examBytes);
  file.OsDisplay();
}

// Comprehensive test involving multiple operations on a Virtual File System (VFS)
// Includes file creation, writing, seeking, reading, and deleting
TEST(VFS, SuccessfulOp) {
  // Step 1 : Create a db file
  std::string filename = "test_SuccessfulOp.db";
  std::remove(filename.c_str());
  OsFile file;
  bool read_only = false;
  ResultCode rc = file.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);

  rc = file.OsFileExists();
  EXPECT_EQ(ResultCode::kOk, rc);

  // Step 2 : Seek offset 0 on the database file
  rc = file.OsSeek(0);
  EXPECT_EQ(ResultCode::kOk, rc);

  std::string exam = "hello this is vfs test";
  std::vector<std::byte> exam_bytes;
  for (char c : exam) {
    exam_bytes.push_back(static_cast<std::byte>(c));
  }
  rc = file.OsWrite(exam_bytes);
  EXPECT_EQ(ResultCode::kOk, rc);

  // Step 3 : Seek offset 6 on the database file
  rc = file.OsSeek(6);

  std::vector<char> read_buf(4);
  std::vector<std::byte> byte_buf(reinterpret_cast<std::byte *>(read_buf.data()),
                                  reinterpret_cast<std::byte *>(read_buf.data() + read_buf.size()));
  rc = file.OsRead(byte_buf, 4);

  std::string word = "this";
  std::vector<std::byte> this_bytes;
  for (char c : word) {
    this_bytes.push_back(static_cast<std::byte>(c));
  }
  EXPECT_EQ(ResultCode::kOk, rc);
  EXPECT_EQ(byte_buf, this_bytes);
  file.OsDisplay();

  // Step 4 : Seek offset 11 on the database file and overwrite the data, get this is override
  rc = file.OsSeek(11);
  std::string overwrite = "is override";
  std::vector<std::byte> overwrite_bytes;
  for (char c : overwrite) {
    overwrite_bytes.push_back(static_cast<std::byte>(c));
  }
  rc = file.OsWrite(overwrite_bytes);
  EXPECT_EQ(ResultCode::kOk, rc);
  file.OsDisplay();

  // Step 5 : Close the database file
  rc = file.OsClose();
  EXPECT_EQ(ResultCode::kOk, rc);
  rc = file.OsDelete();
  EXPECT_EQ(ResultCode::kOk, rc);

  rc = file.OsFileExists();
  EXPECT_EQ(ResultCode::kError, rc);
}

// Tests acquiring and releasing a single read lock on a file
TEST(ReadLockFile, SingleReadLock) {
  std::string filename = "test_SingleReadLock.db";
  std::remove(filename.c_str());
  OsFile file;
  bool read_only = false;
  ResultCode rc = file.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);
  rc = file.OsReadLock();
  EXPECT_EQ(ResultCode::kOk, rc);
  file.OsUnlock();
}

// Tests acquiring two read locks on the same file
TEST(ReadLockFile, TwoReadLocks) {
  // Step 1: Open the file and read lock it
  std::string filename = "test_TwoReadLocks.db";
  std::remove(filename.c_str());
  OsFile file_1{};
  bool read_only = false;
  ResultCode rc = file_1.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);
  rc = file_1.OsReadLock();
  EXPECT_EQ(ResultCode::kOk, rc);
  // Step 2: Open the same file with another OsFile and read lock it
  OsFile file_2{};
  rc = file_2.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);
  rc = file_2.OsReadLock();
#if OS_UNIX
  EXPECT_EQ(ResultCode::kOk, rc);
#elif OS_WIN
  EXPECT_EQ(ResultCode::kBusy, rc);
#endif
  // Step 3: Release the locks
  file_1.OsUnlock();
  file_2.OsUnlock();
}

// Tests acquiring and releasing a single write lock on a file
TEST(WriteLockFile, SingleWriteLock) {
  // Step 1: Open the file and write lock it
  std::string filename = "test_SingleWriteLock.db";
  std::remove(filename.c_str());
  OsFile file_1;
  bool read_only = false;
  ResultCode rc = file_1.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);
  rc = file_1.OsWriteLock();
  EXPECT_EQ(ResultCode::kOk, rc);
  // Step 2: Release the lock
  file_1.OsUnlock();
}

// Tests lock behavior when attempting to acquire a read or write lock on a file that is already locked
TEST(WriteLockFile, AttemptLockingAfterWriteLock) {
  // Step 1: Open the file and write lock it
  std::string filename = "test_AttemptLockingAfterWriteLock.db";
  std::remove(filename.c_str());
  OsFile file_1{};
  bool read_only = false;
  ResultCode rc = file_1.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);
  rc = file_1.OsWriteLock();
  EXPECT_EQ(ResultCode::kOk, rc);
  // Step 2: Attempt to read lock the file
  OsFile file_2{};
  rc = file_2.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);
  rc = file_2.OsReadLock();
  EXPECT_EQ(ResultCode::kBusy, rc);

  // Step 3: Attempt to write lock the file
  OsFile file_3{};
  rc = file_3.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);
  rc = file_3.OsWriteLock();
  EXPECT_EQ(ResultCode::kBusy, rc);

  // Step 4: Release the locks
  file_1.OsUnlock();
  file_2.OsUnlock();
  file_3.OsUnlock();
}

// Tests acquiring a read lock on a file after releasing a write lock
TEST(WriteLockFile, AttemptReadLockAfterWriteLockRelease) {
  // Step 1: Open the file and write lock it
  std::string filename = "test_AttemptReadLockAfterWriteLockRelease.db";
  std::remove(filename.c_str());
  OsFile file_1{};
  bool read_only = false;
  ResultCode rc = file_1.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);
  rc = file_1.OsWriteLock();
  EXPECT_EQ(ResultCode::kOk, rc);

  // Step 2: Release the lock
  rc = file_1.OsUnlock();
  EXPECT_EQ(ResultCode::kOk, rc);

  // Step 3: Attempt to read lock the file with another OsFile
  OsFile file_2{};
  rc = file_2.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);
  rc = file_2.OsReadLock();
  EXPECT_EQ(ResultCode::kOk, rc);
  // Step 3: Release the lock on file_2
  file_2.OsUnlock();
}

// Tests writing to a file using a fixed-size byte array
TEST(WriteFunction, WriteByArray) {
  std::string filename = "test_WriteByArray.db";
  std::remove(filename.c_str());
  OsFile file;
  bool read_only = false;
  ResultCode rc = file.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);

  const std::array<std::byte, kPageSize> data = {std::byte(0x41), std::byte(0x42), std::byte(0x43)};

  rc = file.OsWrite(data);
  EXPECT_EQ(ResultCode::kOk, rc);

  rc = file.OsDisplay();
  EXPECT_EQ(ResultCode::kOk, rc);
}

TEST(OsFileTest, SleepFunction) {
  std::string filename = "test_sleep.db";
  std::remove(filename.c_str());
  OsFile file;
  bool read_only = false;
  ResultCode rc = file.OsOpenReadWrite(filename, read_only);
  EXPECT_EQ(ResultCode::kOk, rc);

  // Start timer
  auto start = std::chrono::high_resolution_clock::now();

  // Sleep for a predetermined duration
  int sleep_duration = 100; // milliseconds
  rc = file.OsSleep(sleep_duration);

  // Stop timer
  auto stop = std::chrono::high_resolution_clock::now();

  // Calculate elapsed time
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

  // Check if the sleep duration is within a reasonable threshold
  EXPECT_EQ(ResultCode::kOk, rc);
  EXPECT_TRUE(duration.count() >= sleep_duration);
}
