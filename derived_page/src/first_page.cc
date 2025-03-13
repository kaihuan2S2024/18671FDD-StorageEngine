#include "first_page.h"

FirstPageByteView FirstPage::GetFirstPageByteView() const {
  FirstPageByteView byte_view{};
  std::memcpy(&byte_view, p_image_->data(), sizeof(FirstPageByteView));
  return byte_view;
}

void FirstPage::SetFirstPageByteView(
    FirstPageByteView &first_page_byte_view_in) {
  std::memcpy(p_image_->data(), &first_page_byte_view_in,
              sizeof(FirstPageByteView));
}

void FirstPage::SetDefaultByteView() {
  FirstPageByteView byte_view{};
  byte_view.magic_int = kCorrectMagicInt;
  byte_view.first_free_page = 0;
  byte_view.num_free_pages = 0;
  SetFirstPageByteView(byte_view);
}

bool FirstPage::HasCorrectMagicInt() const {
  FirstPageByteView byte_view = GetFirstPageByteView();
  return byte_view.magic_int == kCorrectMagicInt;
}

void FirstPage::IncrementNumFreePages() {
  FirstPageByteView byte_view = GetFirstPageByteView();
  byte_view.num_free_pages++;
  SetFirstPageByteView(byte_view);
}

void FirstPage::DecrementNumFreePages() {
  FirstPageByteView byte_view = GetFirstPageByteView();
  byte_view.num_free_pages--;
  SetFirstPageByteView(byte_view);
}

void FirstPage::GetMeta(std::array<int, kMetaIntArraySize> &meta_int_arr) {
  for (size_t i = 0; i < kMetaIntArraySize - 1; i++) {
    int meta_int;
    std::memcpy(&meta_int,
                p_image_->data() + sizeof(FirstPageByteView) + i * sizeof(int),
                sizeof(int));
    meta_int_arr[i + 1] = meta_int;
  }
  meta_int_arr[0] = GetFirstPageByteView().num_free_pages;
}

void FirstPage::UpdateMeta(std::array<int, kMetaIntArraySize> &meta_int_arr) {
  for (size_t i = 0; i < kMetaIntArraySize - 1; i++) {
    std::memcpy(p_image_->data() + sizeof(FirstPageByteView) + i * sizeof(int),
                &meta_int_arr[i + 1], sizeof(int));
  }
}

std::unique_ptr<BasePage> FirstPage::CreateDerivedPage() {
  return std::make_unique<FirstPage>();
}

/**
 * Since there is no extra space to destroy, this function does nothing
 * kCorrectMagicInt does not count as extra space as it is a static const int
 * and is not allocated for each instance of FirstPage
 */
void FirstPage::DestroyExtra() {}