//
// Created by Yiyang Huo on 9/8/23.
//

#include "pager.h"

#include <fstream>
#include <ostream>

#include "gtest/gtest.h"
#include "os.h"

void printFileContents(const std::string &filename) {
  std::ifstream file(filename, std::ios::binary);
  if (file.is_open()) {
    // Read the entire file into a buffer
    std::vector<char> buffer(std::istreambuf_iterator<char>(file), {});
    file.seekg(0, std::ios::beg);  // Reset the file pointer to the beginning
                                   // (for future reads

    // Iterate over the buffer and print only printable characters
    for (const char &byte : buffer) {
      if (std::isprint(byte) || byte == '\n' || byte == '\r') {
        std::cout << byte;
      }
    }

    std::cout << std::endl;  // Flush the line

    file.close();
  } else {
    std::cerr << "Unable to open file " << filename << std::endl;
  }
}

/*
 * This test is based on the example from SimpleSqlite
 */
TEST(PagerFullTest, ExampleFromSimpleSqlite) {
  std::string filename = "test_ExampleFromSimpleSqlite.db";
  std::string journal_filename = "test_ExampleFromSimpleSqlite.db-journal";
  std::remove(filename.c_str());
  std::remove(journal_filename.c_str());

  ResultCode rc;

  // Step 1: Open a Pager with test.db as its filename
  Pager pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);
  BasePage *p_base_page = nullptr;

  // Use SqlitePagerGet to load 3 pages into the cache
  rc = pager.SqlitePagerGet(1, &p_base_page, SampleMemPage::create);
  EXPECT_EQ(ResultCode::kOk, rc);

  rc = pager.SqlitePagerGet(2, &p_base_page, SampleMemPage::create);
  EXPECT_EQ(ResultCode::kOk, rc);

  rc = pager.SqlitePagerGet(3, &p_base_page, SampleMemPage::create);
  EXPECT_EQ(ResultCode::kOk, rc);

  // Step 2: Use lookup to write data into the 3 pages
  p_base_page = nullptr;

  // str_1 has the byte values of "Page One"
  std::vector<std::byte> str_1 = {
      std::byte(0x50), std::byte(0x61), std::byte(0x67), std::byte(0x65),
      std::byte(0x20), std::byte(0x4f), std::byte(0x6e), std::byte(0x65)};
  rc = pager.SqlitePagerLookup(1, &p_base_page);
  EXPECT_EQ(ResultCode::kOk, rc);
  rc = pager.SqlitePagerWrite(p_base_page);
  EXPECT_EQ(ResultCode::kOk, rc);
  std::memcpy(p_base_page->p_image_->data(), str_1.data(), str_1.size());
  rc = pager.SqlitePagerCommit();
  EXPECT_EQ(ResultCode::kOk, rc);

  // str_2 has the byte values of "Page Two"
  std::vector<std::byte> str_2 = {
      std::byte(0x50), std::byte(0x61), std::byte(0x67), std::byte(0x65),
      std::byte(0x20), std::byte(0x54), std::byte(0x77), std::byte(0x6f)};
  rc = pager.SqlitePagerLookup(2, &p_base_page);
  EXPECT_EQ(ResultCode::kOk, rc);
  rc = pager.SqlitePagerWrite(p_base_page);
  EXPECT_EQ(ResultCode::kOk, rc);
  std::memcpy(p_base_page->p_image_->data(), str_2.data(), str_2.size());
  rc = pager.SqlitePagerCommit();
  EXPECT_EQ(ResultCode::kOk, rc);

  // str_3 has the byte values of "Page Three"
  std::vector<std::byte> str_3 = {
      std::byte(0x50), std::byte(0x61), std::byte(0x67), std::byte(0x65),
      std::byte(0x20), std::byte(0x54), std::byte(0x68), std::byte(0x72),
      std::byte(0x65), std::byte(0x65)};
  rc = pager.SqlitePagerLookup(3, &p_base_page);
  EXPECT_EQ(ResultCode::kOk, rc);
  rc = pager.SqlitePagerWrite(p_base_page);
  EXPECT_EQ(ResultCode::kOk, rc);
  std::memcpy(p_base_page->p_image_->data(), str_3.data(), str_3.size());
  rc = pager.SqlitePagerCommit();
  EXPECT_EQ(ResultCode::kOk, rc);

  // Step 3: Read the pages to make sure changes are committed
  rc = pager.SqlitePagerGet(1, &p_base_page, SampleMemPage::create);
  EXPECT_EQ(ResultCode::kOk, rc);
  EXPECT_EQ(
      std::memcmp(p_base_page->p_image_->data(), str_1.data(), str_1.size()),
      0);

  rc = pager.SqlitePagerGet(2, &p_base_page, SampleMemPage::create);
  EXPECT_EQ(ResultCode::kOk, rc);
  EXPECT_EQ(
      std::memcmp(p_base_page->p_image_->data(), str_2.data(), str_2.size()),
      0);

  rc = pager.SqlitePagerGet(3, &p_base_page, SampleMemPage::create);
  EXPECT_EQ(ResultCode::kOk, rc);
  EXPECT_EQ(
      std::memcmp(p_base_page->p_image_->data(), str_3.data(), str_3.size()),
      0);

  // Step 4: Write data into the third page and rollback to the previous state
  // before commit the changes

  // str_4 has the byte values of "Page test rollback"
  std::vector<std::byte> str_4 = {
      std::byte(0x50), std::byte(0x61), std::byte(0x67), std::byte(0x65),
      std::byte(0x20), std::byte(0x74), std::byte(0x65), std::byte(0x73),
      std::byte(0x74), std::byte(0x20), std::byte(0x72), std::byte(0x6f),
      std::byte(0x6c), std::byte(0x6c), std::byte(0x62), std::byte(0x61),
      std::byte(0x63), std::byte(0x6b)};

  rc = pager.SqlitePagerGet(3, &p_base_page, SampleMemPage::create);
  EXPECT_EQ(ResultCode::kOk, rc);
  rc = pager.SqlitePagerWrite(p_base_page);
  EXPECT_EQ(ResultCode::kOk, rc);
  std::memcpy(p_base_page->p_image_->data(), str_4.data(), str_4.size());

  // Rollback changes to the previous state
  rc = pager.SqlitePagerRollback();
  EXPECT_EQ(ResultCode::kOk, rc);
  rc = pager.SqlitePagerCommit();
  // We expect that the commit will fail due to rollback
  EXPECT_NE(ResultCode::kOk, rc);

  // Read the value and expect it to be "Page 3"
  EXPECT_EQ(
      std::memcmp(p_base_page->p_image_->data(), str_3.data(), str_3.size()),
      0);
  EXPECT_NE(
      std::memcmp(p_base_page->p_image_->data(), str_4.data(), str_4.size()),
      0);

  // Step 5: Check that the page count is 3
  EXPECT_EQ(pager.SqlitePagerPageCount(), 3);
}

TEST(PagerRollbackTest, TestRollbackAndCommit) {
  std::string filename = "test_TestRollbackAndCommit.db";
  ResultCode rc;
  Pager pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);
  rc = pager.SqlitePagerRollback();
  EXPECT_EQ(rc, ResultCode::kOk);
  rc = pager.SqlitePagerCommit();
  EXPECT_EQ(rc, ResultCode::kError);
}

// Test rollback when the journal file is empty
TEST(PagerRollbackTest, MultipleRollbacks) {
  std::string filename = "test_MultipleRollbacks.db";
  std::remove(filename.c_str());
  Pager pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);

  for (int i = 0; i < 5; ++i) {
    ResultCode rc = pager.SqlitePagerRollback();
    EXPECT_EQ(rc, ResultCode::kOk);
  }
}

TEST(PagerGetTest, TestFetchOnePage) {
  std::string filename = "test_TestFetchOnePage.db";
  std::string journal_filename = "test_TestFetchOnePage.db-journal";
  // Delete these 2 files
  std::remove(filename.c_str());
  std::remove(journal_filename.c_str());
  ResultCode rc;
  Pager pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);
  BasePage *p_base_page = nullptr;
  rc = pager.SqlitePagerGet(1, &p_base_page, SampleMemPage::create);
  rc = pager.SqlitePagerGet(2, &p_base_page, SampleMemPage::create);
  rc = pager.SqlitePagerGet(3, &p_base_page, SampleMemPage::create);
  rc = pager.SqlitePagerLookup(1, &p_base_page);
  ASSERT_NE(p_base_page, nullptr);
  rc = pager.SqlitePagerWrite(p_base_page);
  EXPECT_EQ(rc, ResultCode::kOk);
}

// Verify that a page can be fetched and created if not in the cache.
TEST(PagerGetTest, BasicPageFetch) {
  std::string filename = "test_BasicPageFetch.db";
  std::remove(filename.c_str());
  Pager pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);

  BasePage *p_page = nullptr;
  ResultCode rc = pager.SqlitePagerGet(1, &p_page, SampleMemPage::create);
  EXPECT_EQ(rc, ResultCode::kOk);
  EXPECT_NE(p_page, nullptr);

  // Verify page number
  EXPECT_EQ(pager.SqlitePagerPageNumber(p_page), 1);

  // Check the content of the newly created page (should be zero-initialized)
  std::vector<std::byte> expected_content(1024, std::byte(0));
  EXPECT_EQ(std::memcmp(p_page->p_image_->data(), expected_content.data(),
                        expected_content.size()),
            0);
}

TEST(PagerPageCountTest, DatabaseFileEmpty) {
  std::string filename = "test_DatabaseFileEmpty.db";
  std::string journal_filename = "test_DatabaseFileEmpty.db-journal";
  // Delete these 2 files
  std::remove(filename.c_str());
  std::remove(journal_filename.c_str());
  ResultCode rc;
  Pager pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);
  BasePage *p_base_page = nullptr;
  u32 page_count;
  // There is nothing in the database file
  // So the page count should be 0
  page_count = pager.SqlitePagerPageCount();
  EXPECT_EQ(page_count, 0);
  // Load one page into the cache
  rc = pager.SqlitePagerGet(1, &p_base_page, SampleMemPage::create);
  EXPECT_EQ(rc, ResultCode::kOk);
  // Since nothing is written into the database file, the page count should
  // still be 0
  page_count = pager.SqlitePagerPageCount();
  EXPECT_EQ(page_count, 0);
}

TEST(PagerJournalTest, HandlesJournalFilePlayback) {
  // Arrange
  std::string filename = "test_HandlesJournalFilePlayback.db";
  std::string journal_filename = "test_HandlesJournalFilePlayback.db-journal";
  std::remove(filename.c_str());
  std::remove(journal_filename.c_str());
  ResultCode rc;
  Pager pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);
  BasePage *p_base_page = nullptr;
  rc = pager.SqlitePagerGet(1, &p_base_page, SampleMemPage::create);
  EXPECT_EQ(rc, ResultCode::kOk);
  rc = pager.SqlitePagerUnref(p_base_page);
  EXPECT_EQ(rc, ResultCode::kOk);
  rc = pager.SqlitePagerGet(1, &p_base_page, SampleMemPage::create);
  EXPECT_EQ(rc, ResultCode::kOk);
}

// Write data to multiple pages sequentially and ensure the commit operation
// persists all data.
TEST(PagerJournalTest, SequentialWritesAndCommit) {
  std::string filename = "test_SequentialWritesAndCommit.db";
  std::remove(filename.c_str());
  ResultCode rc;
  Pager pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);
  BasePage *p_base_page = nullptr;

  // Write to pages 1 to 3
  for (int i = 1; i <= 3; ++i) {
    rc = pager.SqlitePagerGet(i, &p_base_page, SampleMemPage::create);
    EXPECT_EQ(rc, ResultCode::kOk);
    ASSERT_NE(p_base_page, nullptr);
    rc = pager.SqlitePagerWrite(p_base_page);
    EXPECT_EQ(rc, ResultCode::kOk);

    // Fill page with data
    std::vector<std::byte> data(1024, std::byte(i));
    std::memcpy(p_base_page->p_image_->data(), data.data(), data.size());
  }

  // Commit transaction
  rc = pager.SqlitePagerCommit();
  EXPECT_EQ(rc, ResultCode::kOk);

  // Verify data persistence
  for (int i = 1; i <= 3; ++i) {
    rc = pager.SqlitePagerGet(i, &p_base_page, SampleMemPage::create);
    EXPECT_EQ(rc, ResultCode::kOk);
    ASSERT_NE(p_base_page, nullptr);
    std::vector<std::byte> expected_data(1024, std::byte(i));
    EXPECT_EQ(std::memcmp(p_base_page->p_image_->data(), expected_data.data(),
                          expected_data.size()),
              0);
  }
}

// Ensure that changes made to a page are retained in memory but not persisted
// until committed.
TEST(PagerWriteTest, WriteWithoutCommit) {
  std::string filename = "test_WriteWithoutCommit.db";
  std::remove("test_WriteWithoutCommit.db-journal");
  std::remove("test_WriteWithoutCommit.db-checkpoint");
  std::remove(filename.c_str());
  Pager pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);

  BasePage *p_page = nullptr;

  // Load and modify a page
  ResultCode rc = pager.SqlitePagerGet(1, &p_page, SampleMemPage::create);
  EXPECT_EQ(rc, ResultCode::kOk)
      << "CHECKPOINT 1: Unexpected result in SqlitePagerGet\n";
  ASSERT_NE(p_page, nullptr) << "CHECKPOINT 1: Page retrieval failed\n";
  rc = pager.SqlitePagerWrite(p_page);
  EXPECT_EQ(rc, ResultCode::kOk)
      << "CHECKPOINT 2: Unexpected result in SqlitePagerWrite\n";

  // Fill page with data
  std::vector<std::byte> data(1024, std::byte(42));
  std::memcpy(p_page->p_image_->data(), data.data(), data.size());

  // Verify changes in memory
  std::vector<std::byte> expected_data(1024, std::byte(42));
  EXPECT_EQ(std::memcmp(p_page->p_image_->data(), expected_data.data(),
                        expected_data.size()),
            0)
      << "CHECKPOINT 3: Unexpected memory content before commit\n";

  // Commit changes and verify persistence
  rc = pager.SqlitePagerCommit();
  EXPECT_EQ(rc, ResultCode::kOk) << "CHECKPOINT 4: Commit operation failed\n";

  // Reinitialize the pager to verify persistence
  Pager pager_reloaded(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);
  BasePage *p_page_reloaded = nullptr;

  rc =
      pager_reloaded.SqlitePagerGet(1, &p_page_reloaded, SampleMemPage::create);
  EXPECT_EQ(rc, ResultCode::kOk)
      << "CHECKPOINT 5: Unexpected result in reloaded SqlitePagerGet\n";

  // Verify that reloaded page contains the same data
  EXPECT_EQ(std::memcmp(p_page_reloaded->p_image_->data(), expected_data.data(),
                        expected_data.size()),
            0)
      << "CHECKPOINT 6: Data mismatch after commit and reload\n";
}
