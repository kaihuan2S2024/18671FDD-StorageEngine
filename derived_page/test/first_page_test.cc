#include "first_page.h"
#include "pager.h"

#include "gtest/gtest.h"

TEST(FirstPageByteViewTest, CheckTriviallyCopiable) {
  EXPECT_TRUE(std::is_trivially_copyable_v<FirstPageByteView>);
}

TEST(FirstPageTest, DetectsCorrectMagicInt) {
  // Step 1: Create a FirstPage object and set the default byte view, which has the correct magic int
  FirstPage first_page{};
  first_page.SetDefaultByteView();
  // Step 2: Check that the magic int is correct
  EXPECT_TRUE(first_page.HasCorrectMagicInt());
}

TEST(FirstPageTest, DetectsIncorrectMagicInt) {
  // Step 1: Create a FirstPage object and set the default byte view, which has the correct magic int
  FirstPage first_page{};
  first_page.SetDefaultByteView();

  // Step 2: Corrupt the magic int
  FirstPageByteView corrupted_byte_view{};
  corrupted_byte_view.magic_int = 321;
  std::memcpy(first_page.p_image_->data(), &corrupted_byte_view, sizeof(FirstPageByteView));

  // Step 3: Check that the magic int is incorrect
  EXPECT_FALSE(first_page.HasCorrectMagicInt());
}

TEST(PagerInteractionTest, CanModifyPageContent) {
  // Step 1: Create a pager and get a first page
  std::string filename = "test.db";
  Pager pager(filename, kMaxPageCount);
  FirstPage first_page{};
  BasePage *p_base_page = &first_page;
  ResultCode rc;
  rc = pager.SqlitePagerGet(1, &p_base_page, FirstPage::CreateDerivedPage);
  EXPECT_EQ(rc, ResultCode::kOk);
  auto *p_first_page = dynamic_cast<FirstPage *>(p_base_page);
  p_first_page->SetDefaultByteView();
  EXPECT_TRUE(p_first_page->HasCorrectMagicInt());
}

