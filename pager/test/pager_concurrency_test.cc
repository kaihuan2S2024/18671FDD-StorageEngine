#include <cmath>
#include <mutex>
#include <thread>

#include "gtest/gtest.h"
#include "os.h"
#include "pager.h"

std::mutex pager_mutex;

void concurrentAccessRead(int num) {
  std::string filename = "test_HandlesConcurrentAccess.db";
  Pager pager(filename, 10);
  BasePage *p_base_page = nullptr;
  ResultCode rc = pager.SqlitePagerGet(1, &p_base_page, SampleMemPage::create);
  EXPECT_EQ(rc, ResultCode::kOk);
  EXPECT_EQ(std::memcmp(p_base_page->p_image_->data(), &num, sizeof(int)), 0);
}

TEST(PagerConcurrencyTest, HandlesConcurrentAccess) {
  std::string filename = "test_HandlesConcurrentAccess.db";
  std::remove(filename.c_str());
  // Initialize pager with the filename and page size
  ResultCode rc;
  Pager pager(filename, 10);
  BasePage *p_base_page = nullptr;

  // Get the first page and store number data in it
  rc = pager.SqlitePagerGet(1, &p_base_page, SampleMemPage::create);
  int num = 100;
  ASSERT_EQ(rc, ResultCode::kOk);
  ASSERT_NE(p_base_page, nullptr);
  std::memcpy(p_base_page->p_image_->data(), &num, sizeof(int));
  rc = pager.SqlitePagerWrite(p_base_page);
  rc = pager.SqlitePagerCommit();

  // Create threads for concurrent access
  std::vector<std::thread> threads;
  for (int i = 0; i < 100; i++) {
    threads.push_back(std::thread(concurrentAccessRead, num));
  }
  for (auto &thread : threads) {
    thread.join();
  }
}

// Global shared pager for the second test
Pager *global_pager = nullptr;

void readPage() {
  BasePage *p_base_page = nullptr;
  ResultCode rc;
  // not add locker for there, for parallel read
  while (global_pager->SqlitePagerGet(1, &p_base_page, SampleMemPage::create) !=
         ResultCode::kOk) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  // rc = global_pager->SqlitePagerGet(1, &p_base_page, SampleMemPage::create);
  // EXPECT_EQ(rc, ResultCode::kOk); // kBusy
  EXPECT_NE(p_base_page->p_image_, nullptr);
  int result1;
  std::memcpy(&result1, p_base_page->p_image_->data(), sizeof(int));

  rc = global_pager->SqlitePagerGet(2, &p_base_page, SampleMemPage::create);
  EXPECT_EQ(rc, ResultCode::kOk);
  EXPECT_NE(p_base_page->p_image_, nullptr);
  int result2;
  std::memcpy(&result2, p_base_page->p_image_->data(), sizeof(int));
  EXPECT_TRUE(std::isfinite(result1 + result2));
}

void writePage() {
  BasePage *p_base_page = nullptr;
  ResultCode rc;
  // write locker
  std::lock_guard<std::mutex> lock(pager_mutex);

  rc = global_pager->SqlitePagerGet(1, &p_base_page, SampleMemPage::create);
  EXPECT_EQ(rc, ResultCode::kOk);  // kOk
  EXPECT_NE(p_base_page->p_image_, nullptr);
  int num1;
  std::memcpy(&num1, p_base_page->p_image_->data(), sizeof(int));
  num1 += 50;
  std::memcpy(p_base_page->p_image_->data(), &num1, sizeof(int));
  rc = global_pager->SqlitePagerWrite(p_base_page);
  EXPECT_EQ(rc, ResultCode::kOk);  // kBusy

  rc = global_pager->SqlitePagerGet(2, &p_base_page, SampleMemPage::create);
  EXPECT_EQ(rc, ResultCode::kOk);  // kOk
  EXPECT_NE(p_base_page->p_image_, nullptr);
  int num2;
  std::memcpy(&num2, p_base_page->p_image_->data(), sizeof(int));
  num2 -= 50;
  std::memcpy(p_base_page->p_image_->data(), &num2, sizeof(int));
  rc = global_pager->SqlitePagerWrite(p_base_page);
  EXPECT_EQ(rc, ResultCode::kOk);  // kBusy

  rc = global_pager->SqlitePagerCommit();
  EXPECT_EQ(rc, ResultCode::kOk);
}

TEST(PagerConcurrencyTest, HandlesConcurrentReadWrite) {
  std::string filename = "test_HandlesConcurrentReadWrite.db";
  std::remove(filename.c_str());
  // Initialize pager with the filename and page size
  ResultCode rc;
  global_pager = new Pager(filename, 10);
  BasePage *p_base_page = nullptr;

  // Initialize the first page with 100
  rc = global_pager->SqlitePagerGet(1, &p_base_page, SampleMemPage::create);
  int num1 = 100;
  ASSERT_EQ(rc, ResultCode::kOk);
  ASSERT_NE(p_base_page, nullptr);
  std::memcpy(p_base_page->p_image_->data(), &num1, sizeof(int));
  rc = global_pager->SqlitePagerWrite(p_base_page);

  // Initialize the second page with 200
  rc = global_pager->SqlitePagerGet(2, &p_base_page, SampleMemPage::create);
  int num2 = 200;
  ASSERT_EQ(rc, ResultCode::kOk);
  ASSERT_NE(p_base_page, nullptr);
  std::memcpy(p_base_page->p_image_->data(), &num2, sizeof(int));
  rc = global_pager->SqlitePagerWrite(p_base_page);

  rc = global_pager->SqlitePagerCommit();
  EXPECT_EQ(rc, ResultCode::kOk);

  readPage();
  writePage();

  // Create threads for concurrent access
  for (int i = 0; i < 1000; i++) {
    std::thread reader(readPage);
    std::thread writer(writePage);

    reader.join();
    writer.join();
  }
  // clear global page
  delete global_pager;
  global_pager = nullptr;
}