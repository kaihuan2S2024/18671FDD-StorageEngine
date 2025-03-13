#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include "pager.h"
#include "sql_int.h"
#include "sql_limit.h"
#include "sql_rc.h"

/**
 * @struct OverflowPageHeaderByteView
 * @brief Represents the header structure for an overflow page.
 *
 * This struct provides a view of the metadata stored at the beginning
 * of an overflow page. It maintains a reference to the next free page,
 * effectively forming a linked list of free pages within the storage system.
 *
 * @note This structure is typically used in the context of free page
 * management, where multiple free pages are chained together.
 */
struct OverflowPageHeaderByteView {
  /**
   * @brief The page number of the next free page.
   *
   * This field stores the identifier of the next available free page in the
   * system. Each free page contains information about the subsequent free page,
   * forming a linked list of available pages for efficient allocation and
   * reuse.
   */
  PageNumber next_page;
};

// It stores the number of free pages
struct FreeListInfoHeaderByteView {
  PageNumber num_free_pages;
};

constexpr u16 kOverflowSize = kPageSize - sizeof(OverflowPageHeaderByteView);

/**
 * @class OverFreePage
 *
 * This class represents an entity that can modify the content of its page image
 * byte array as both an Overflow Page (contains payload of overflow cells), a
 * FreeListInfoPage (contains page numbers to free list leaf pages, also known
 * as FreeListTrunkPage in some books), and a FreePage (contains pure garbage
 * data ready to be overwritten, FreeListLeafPage ).
 */
class OverFreePage : public BasePage {
 public:
  OverFreePage() = default;

  void SetOverflowPageHeaderByteView(
      OverflowPageHeaderByteView &overflow_page_header_byte_view_in);
  OverflowPageHeaderByteView GetOverflowPageHeaderByteView();
  void SetFreeListInfoHeaderByteView(
      FreeListInfoHeaderByteView &info_header_byte_view);
  FreeListInfoHeaderByteView GetFreeListInfoHeaderByteView();

  void IncrementFreeListNumPages();
  void DecrementFreeListNumPages();
  PageNumber GetFreeListInfoPageNumber(u16 free_list_idx);
  PageNumber GetFinalFreeListInfoPageNumber();
  u16 GetNumberOfFreeListPages();
  void SetFreeListInfoPageNumber(u16 free_list_idx, PageNumber page_number);
  bool CanInsertPageNumber();
  void InsertPageNumber(PageNumber page_number);
  void DestroyExtra() override;
};