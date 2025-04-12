#include "btree.h"

#include "gtest/gtest.h"

/*
 * btree_test.cc
 *
 * These tests are meant for the developers to test the Btree class
 * There are still edge case that still need to be tested.
 *
 */

TEST(BtreeConstructorTest, CanOpenSuccessfully) {

  // Step 1 : Create a Btree
  std::string filename = "test_CanOpenSuccessfully.db";
  std::string journal_filename = "test_CanOpenSuccessfully.db-journal";
  std::remove(filename.c_str());
  std::remove(journal_filename.c_str());
  // Step 2: Expect that the Btree can be created without throwing an exception
  EXPECT_NO_THROW(Btree btree(filename, 10));
}

TEST(BtCursorCreateTest, CanCreateSuccessfully) {

  // Step 1: Create a Btree
  std::string filename = "test_CanCreateSuccessfully.db";
  std::string journal_filename = "test_CanCreateSuccessfully.db-journal";
  std::remove(filename.c_str());
  std::remove(journal_filename.c_str());
  Btree btree(filename, 10);

  // Step 2: Create a BtCursor and expect it to not be expired
  BtCursor cursor;
  std::weak_ptr<BtCursor> p_cursor_weak;
  ResultCode rc = btree.BtCursorCreate(2, true, p_cursor_weak);
  EXPECT_EQ(rc, ResultCode::kOk);
  EXPECT_FALSE(p_cursor_weak.expired());
}

TEST(BtCursorCloseTest, CanCloseSuccessfully) {
  std::string filename = "test_CanCloseSuccessfully.db";
  std::string journal_filename = "test_CanCloseSuccessfully.db-journal";
  std::remove(filename.c_str());
  std::remove(journal_filename.c_str());
  Btree btree(filename, 10);
  std::weak_ptr<BtCursor> p_cursor_weak;
  ResultCode rc = btree.BtCursorCreate(2, true, p_cursor_weak);
  EXPECT_EQ(rc, ResultCode::kOk);
  EXPECT_FALSE(p_cursor_weak.expired());
  rc = btree.BtCursorClose(p_cursor_weak);
  EXPECT_EQ(rc, ResultCode::kOk);
  EXPECT_TRUE(p_cursor_weak.expired());
  std::remove(filename.c_str());
  std::remove(journal_filename.c_str());
}

TEST(DerivedPageTypeCastingTest, CastBetweenBaseNodeOverfull) {
  std::string filename = "test_CastBetweenBaseNodeOverfull.db";
  std::string journal_filename = "test_CastBetweenBaseNodeOverfull.db-journal";
  std::remove(filename.c_str());
  std::remove(journal_filename.c_str());
  std::unique_ptr<Pager> p_pager = std::make_unique<Pager>(filename, 10);
  BasePage *p_base_page = nullptr;
  NodePage *p_node_page = nullptr;
  p_pager->SqlitePagerGet(5, &p_base_page, NodePage::CreateDerivedPage);
  p_node_page = dynamic_cast<NodePage *>(p_base_page);
  EXPECT_NE(p_node_page, nullptr);
  OverFreePage *p_overflow_page = nullptr;
  p_overflow_page = dynamic_cast<OverFreePage *>(p_node_page);
  EXPECT_NE(p_overflow_page, nullptr);
  p_node_page = dynamic_cast<NodePage *>(p_overflow_page);
  EXPECT_NE(p_node_page, nullptr);
  std::remove(filename.c_str());
  std::remove(journal_filename.c_str());
}

TEST(MetadataTest, UpdateAndGetMetadata) {

  // Step 1: Create a Btree and start a transaction
  std::string filename = "test_UpdateAndGetMetadata.db";
  std::string journal_filename = "test_UpdateAndGetMetadata.db-journal";
  std::remove(filename.c_str());
  std::remove(journal_filename.c_str());
  ResultCode rc;
  Btree btree(filename, 10);
  rc = btree.BtreeBeginTrans();
  EXPECT_EQ(rc, ResultCode::kOk);

  // Step 2: Insert the meta data

  // BtreeUpdateMeta will only insert starting from the second element in the
  // array This is because the first element reserved to write the number of
  // free pages In this case, only 200, 300, 400 will be written to the metadata
  std::array<int, kMetaIntArraySize> meta_int_arr = {100, 200, 300, 400};
  rc = btree.BtreeUpdateMeta(meta_int_arr);
  EXPECT_EQ(rc, ResultCode::kOk);

  // Step 3: Retrieve the metadata and check if it is the same as the one
  // inserted

  // Currently, there are no free pages
  // Therefore, the first element passed back will be 0
  // The rest of the elements will be 200, 300, 400
  std::array<int, kMetaIntArraySize> retrieved_meta_int_arr = {500, 600, 700,
                                                               800};
  std::array<int, kMetaIntArraySize> expected_meta_int_arr = {0, 200, 300, 400};
  rc = btree.BtreeGetMeta(retrieved_meta_int_arr);
  EXPECT_EQ(rc, ResultCode::kOk);
  EXPECT_EQ(retrieved_meta_int_arr, expected_meta_int_arr);
}

TEST(DropTableTest, DropOneTable) {

  // Step 1: Create a Btree and start a transaction
  std::string filename = "test_DropOneTable.db";
  std::string journal_filename = "test_DropOneTable.db-journal";
  std::remove(filename.c_str());
  std::remove(journal_filename.c_str());
  ResultCode rc;
  Btree btree(filename, 10);
  rc = btree.BtreeBeginTrans();
  EXPECT_EQ(rc, ResultCode::kOk);

  // Step 2: Create a table and get the root page number
  PageNumber root_page_number;
  rc = btree.BtreeCreateTable(root_page_number);
  EXPECT_EQ(rc, ResultCode::kOk);

  // Step 3: Drop the table
  rc = btree.BtreeDropTable(root_page_number);
  EXPECT_EQ(rc, ResultCode::kOk);
}

TEST(DestroyExtraTest, FirstPageDestroyExtra) {

  // Step 1: Create a FirstPage
  FirstPage first_page;

  // Step 2: Modify the header of the first page
  FirstPageByteView header_before_destroy = first_page.GetFirstPageByteView();
  header_before_destroy.magic_int = 12345;
  header_before_destroy.first_free_page = 42;
  header_before_destroy.num_free_pages = 24;
  first_page.SetFirstPageByteView(header_before_destroy);

  // Step 3: Destroy extra space
  first_page.DestroyExtra();

  // Step 4: Check if header is still the same
  FirstPageByteView header_after_destroy = first_page.GetFirstPageByteView();
  EXPECT_EQ(header_after_destroy.magic_int, header_before_destroy.magic_int);
}
