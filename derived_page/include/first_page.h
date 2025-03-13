#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include "pager.h"
#include "sql_int.h"
#include "sql_limit.h"
#include "sql_rc.h"

/**
 * @struct FirstPageByteView first_page.h
 *
 * The content in bytes that is saved in the first page (page 1)
 */
struct FirstPageByteView {
  // A signed integer we use to check if the page is corrupted.
  // Some databases might use an entire string to check for corruption.
  // We use a simple integer for simplicity.
  int magic_int;

  // An unsigned integer that points to the first free page.
  // The free page will contain information about the next free page,
  // forming a linked list of free pages.
  PageNumber first_free_page;

  // An unsigned integer that stores the number of free pages
  u32 num_free_pages;
};

// Notates the size of the meta integer array. It is used between the VDBE and
// the Btree to store meta information.
constexpr u16 kMetaIntArraySize = 4;

/**
 * @class FirstPage
 *
 * @brief The first page (i.e. page 1) holds the database configuration.
 *
 * It is responsible for manipulating data on page 1 (see FirstPageByteView).
 * - A magic integer (it checks if file is corruption)
 * - A page number that points to the first FreeListInfoPage
 * - An unsigned integer that records the number of free pages. An array of 4
 * integers, metadata used by the VDBE layer
 *
 * @note The first page is special page since many operations require it to be
 * in memory, such as reading database configuration and knowing where the free
 * list starts.
 */
class FirstPage : public BasePage {
 private:
  static constexpr int kCorrectMagicInt = 12345;

 public:
  static std::unique_ptr<BasePage> CreateDerivedPage();

  [[nodiscard]] FirstPageByteView GetFirstPageByteView() const;
  void SetFirstPageByteView(FirstPageByteView &first_page_byte_view_in);
  void SetDefaultByteView();
  [[nodiscard]] bool HasCorrectMagicInt() const;
  void IncrementNumFreePages();
  void DecrementNumFreePages();

  void GetMeta(std::array<int, kMetaIntArraySize> &meta_int_arr);
  void UpdateMeta(std::array<int, kMetaIntArraySize> &meta_int_arr);
  void DestroyExtra() override;
};