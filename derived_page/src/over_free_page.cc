#include "over_free_page.h"

OverflowPageHeaderByteView OverFreePage::GetOverflowPageHeaderByteView() {
  OverflowPageHeaderByteView byte_view{};
  std::memcpy(&byte_view, p_image_->data(), sizeof(OverflowPageHeaderByteView));
  return byte_view;
}

void OverFreePage::SetOverflowPageHeaderByteView(
    OverflowPageHeaderByteView &overflow_page_header_byte_view_in) {
  std::memcpy(p_image_->data(), &overflow_page_header_byte_view_in,
              sizeof(OverflowPageHeaderByteView));
}

FreeListInfoHeaderByteView OverFreePage::GetFreeListInfoHeaderByteView() {
  FreeListInfoHeaderByteView byte_view{};
  ImageIndex start_idx = sizeof(OverflowPageHeaderByteView);
  std::memcpy(&byte_view, p_image_->data() + start_idx,
              sizeof(FreeListInfoHeaderByteView));
  return byte_view;
}

u16 OverFreePage::GetNumberOfFreeListPages() {
  FreeListInfoHeaderByteView free_list_info_byte_view =
      GetFreeListInfoHeaderByteView();
  return (u16)free_list_info_byte_view.num_free_pages;
}

void OverFreePage::SetFreeListInfoHeaderByteView(
    FreeListInfoHeaderByteView &info_header_byte_view) {
  ImageIndex start_idx = sizeof(OverflowPageHeaderByteView);
  std::memcpy(p_image_->data() + start_idx, &info_header_byte_view,
              sizeof(FreeListInfoHeaderByteView));
}

void OverFreePage::IncrementFreeListNumPages() {
  FreeListInfoHeaderByteView free_list_info_byte_view =
      GetFreeListInfoHeaderByteView();
  free_list_info_byte_view.num_free_pages++;
  SetFreeListInfoHeaderByteView(free_list_info_byte_view);
}

void OverFreePage::DecrementFreeListNumPages() {
  FreeListInfoHeaderByteView free_list_info_byte_view =
      GetFreeListInfoHeaderByteView();
  free_list_info_byte_view.num_free_pages--;
  SetFreeListInfoHeaderByteView(free_list_info_byte_view);
}

PageNumber OverFreePage::GetFreeListInfoPageNumber(u16 free_list_idx) {
  FreeListInfoHeaderByteView free_list_info_byte_view =
      GetFreeListInfoHeaderByteView();
  ImageIndex start_idx = sizeof(OverflowPageHeaderByteView);
  if (free_list_idx >= free_list_info_byte_view.num_free_pages) {
    return 0;
  }
  PageNumber page_number = 0;
  std::memcpy(&page_number,
              p_image_->data() + start_idx +
                  sizeof(FreeListInfoHeaderByteView) +
                  free_list_idx * sizeof(PageNumber),
              sizeof(PageNumber));
  return page_number;
}
PageNumber OverFreePage::GetFinalFreeListInfoPageNumber() {
  FreeListInfoHeaderByteView free_list_info_byte_view =
      GetFreeListInfoHeaderByteView();
  return GetFreeListInfoPageNumber(free_list_info_byte_view.num_free_pages - 1);
}

void OverFreePage::SetFreeListInfoPageNumber(u16 free_list_idx,
                                             PageNumber page_number) {
  ImageIndex start_idx =
      sizeof(OverflowPageHeaderByteView) + sizeof(FreeListInfoHeaderByteView);
  std::memcpy(p_image_->data() + start_idx + free_list_idx * sizeof(PageNumber),
              &page_number, sizeof(PageNumber));
}

bool OverFreePage::CanInsertPageNumber() {
  ImageIndex start_idx = sizeof(OverflowPageHeaderByteView);
  u16 usable_space =
      kPageSize - (start_idx + sizeof(FreeListInfoHeaderByteView));
  u16 num_free_list_pages = GetNumberOfFreeListPages();
  return num_free_list_pages < usable_space / sizeof(PageNumber);
}

void OverFreePage::InsertPageNumber(PageNumber page_number) {
  if (!CanInsertPageNumber()) {
    return;
  }
  u16 num_free_list_pages = GetNumberOfFreeListPages();
  SetFreeListInfoPageNumber(num_free_list_pages, page_number);
  IncrementFreeListNumPages();
}

/**
 * Since there is no extra space to destroy, this function does nothing
 */
void OverFreePage::DestroyExtra() {}
