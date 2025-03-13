#include "over_free_page.h"

#include "gtest/gtest.h"

TEST(OverFreePageHeaderByteViewTest, CheckTriviallyCopiable) {
  EXPECT_TRUE(std::is_trivially_copyable_v<OverflowPageHeaderByteView>);
  EXPECT_TRUE(std::is_trivially_copyable_v<FreeListInfoHeaderByteView>);
}

TEST(FreeListInfoHeaderByteViewTest, ReturnsCorrectByteView) {

  // Step 1: Create an OverFreePage
  OverFreePage over_free_page;

  // Step 2: Extract byte view and insert it back
  FreeListInfoHeaderByteView free_list_info = over_free_page.GetFreeListInfoHeaderByteView();
  free_list_info.num_free_pages = 10;
  over_free_page.SetFreeListInfoHeaderByteView(free_list_info);

  // Step 3: Retrieve byte view and check if it is the same
  FreeListInfoHeaderByteView free_list_info_retrieved = over_free_page.GetFreeListInfoHeaderByteView();
  EXPECT_EQ(free_list_info_retrieved.num_free_pages, free_list_info.num_free_pages);
}

TEST(DestroyExtraTest, OverFreePageDestroyExtra) {
  // Step 1: Create an OverFreePage
  OverFreePage over_free_page;

  // Step 2: Modify the header of the over free page
  FreeListInfoHeaderByteView header_before_destroy = over_free_page.GetFreeListInfoHeaderByteView();
  header_before_destroy.num_free_pages = 24;
  over_free_page.SetFreeListInfoHeaderByteView(header_before_destroy);

  // Step 3: Destroy extra space
  over_free_page.DestroyExtra();

  // Step 4: Check if header is still the same
  FreeListInfoHeaderByteView header_after_destroy = over_free_page.GetFreeListInfoHeaderByteView();
  EXPECT_EQ(header_after_destroy.num_free_pages, header_before_destroy.num_free_pages);

}