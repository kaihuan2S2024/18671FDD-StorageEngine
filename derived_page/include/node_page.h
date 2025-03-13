#pragma once

#include <gtest/gtest_prod.h>

#include <array>
#include <vector>

#include "over_free_page.h"
#include "pager.h"
#include "sql_int.h"
#include "sql_limit.h"
#include "sql_rc.h"

/*
 * NodePageHeaderByteView
 *
 * This trivially copyable struct is used to manipulate the header of a
 * NodePage. The NodePageHeader starts at address 0 of the page image.
 */
struct NodePageHeaderByteView {
  // The right child page number when the page is treated as an internal node in
  // a Btree.
  PageNumber right_child;

  // The first cell starts at this index
  ImageIndex first_cell_idx;

  // The first free block starts at this index. Set to 0 if there are no free
  // blocks.
  ImageIndex first_free_block_idx;
};

/*
 * FreeBlockByteView
 *
 * This trivially copyable struct is used to manipulate a free block.
 * It represents a block of free space on a page.
 * It is possible for a page to have multiple free blocks.
 * It is also possible for cells and free blocks to be interleaved.
 * There may also be 2 free blocks that are adjacent to each other.
 *
 * The free blocks are connected as a linked list.
 * Each free block holds the index that holds the next free block.
 * If next_block_idx is 0, then there are no more free blocks.
 *
 */
struct FreeBlockByteView {
  // The size of the free block in bytes. This includes size(FreeBlockByteView)
  // itself.
  u16 size;
  // The index of the next free block
  ImageIndex next_block_idx;
};

/*
 * CellHeaderByteView
 *
 * This trivially copyable struct is used to manipulate a cell header.
 * It represents a cell on a page.
 */
struct CellHeaderByteView {
 public:
  // ######### Public Variables #########

  // The left child page number for the cell's key
  PageNumber left_child;

  // The size of the key in bytes
  u32 key_size;

  // The size of the data in bytes
  u32 data_size;

  // The index of the next cell
  ImageIndex next_cell_start_idx;

  // The page number of the overflow page that holds that cannot be fit within
  // this page. Set to 0 if there is no overflow page.
  PageNumber overflow_page;

  // ######### Public Functions #########

  // Helper function for getting the total size of the cell header.
  [[nodiscard]] u32 GetCellSize() const;
};

// Minimum size of a cell
const u16 kMinCellSize = sizeof(CellHeaderByteView) + 4;

// Usable space on a page in bytes. It is all the space except for the header on
// the page.
const u16 kUsableSpace = kPageSize - sizeof(NodePageHeaderByteView);

/* ------------------------------------
 *  NodePageHeaderByteView (20 bytes)
 *
 *  Remaining space: (kPageSize - 20) bytes
 *  ------------------------------
 */

/*
 * Information regarding NodePageHeaderByteView.left_child and
 * CellHeaderByteView.right_child
 *
 * Note that when a NodePage is treated as an internal node in a Btree,
 * each key is mapped to a 4-byte left child page number, stored in
 * CellHeaderByteView.left_child.
 *
 * Each left child page number points to a page that indicates the next page to
 * traverse to. Find the greatest key whose value that is less the key you are
 * searching for. Then, traverse to the page pointed to by the left child page
 * number. However, when the key you are searching for is greater than or equal
 * to the last key in the page, you must traverse to the page pointed to by the
 * right child page number. The variable right_child stores that page number.
 * Logically, the keys are their left child plus the additional right child can
 * be seen as this: | LeftChild(0) | Key(0) | LeftChild(1) | Key(1) | .... |
 * Ptr(N) | Key(N) | RightChild |
 *
 * When stored on the page image, it looks like this:
 * PageImage: {
 * 	NodePageHeader: { RightChild }
 * 	PageImage: [
 * 		Cell: { LeftChild(0), Key(0) },
 * 		Cell: { LeftChild(1), Key(1) },
 * 		Cell: { LeftChild(2), Key(2) },
 * 		...
 * 		Cell: { LeftChild(N), Key(N) },
 * 	]
 * }
 */

/*
 * Cell
 *
 * A class that is used as a temporary container for a cell.
 * There are 3 cases where we need a container for containing cells outside the
 * page image:
 *
 * 1. When you receive the key and data byte vectors from the VDBE layer, and
 * before you insert it into the page image.
 * 2. When you attempt to insert a key-data pair into a page, discovering that
 * there isn't enough space for the cell (overfull). You have done the work to
 * locate which page and which ordered position (cell_index) to insert the cell.
 * Instead of doing the work again, you can store the cell in a container and
 * let the Balance function handle rest.
 * 3. When you are redistributing cells during the balance process.
 *
 */
class Cell {
  friend class NodePage;
  friend class Btree;

  // ######## Private Variables #######
 private:
  CellHeaderByteView cell_header_;
  std::vector<std::byte> payload_;

  // ######### Public Functions ########
 public:
  Cell();
  Cell(const std::vector<std::byte> &key_in,
       const std::vector<std::byte> &data_in);
  Cell(const CellHeaderByteView &cell_header_in,
       const std::vector<std::byte> &payload_in);
  u32 GetPayloadSize();
  u32 GetCellSize() const;
  bool NeedOverflowPage() const;
};

/*
 * The maximum amount of payload (in bytes) that can be stored locally for
 * a database entry.  If the entry contains more data than this, the
 * extra goes onto overflow pages.
 *
 * This number is chosen so that at least 4 cells will fit on every page.
 * Currently, the number is 238.
 */
const u16 kMaxLocalPayload =
    kUsableSpace / 4 - sizeof(CellHeaderByteView) + sizeof(PageNumber);

/*
 * CellTracker
 *
 * A class that is used to keep track of the cells in a page.
 * It is used to keep track of cells both written into the page image
 * and cells that couldn't be fitted into the page, waiting to be redistributed
 * during the balance process.
 *
 *
 * Cells should be inserted into the page before the balance process is called.
 * During the insertion, the position (cell_idx)  of the cell is determined
 * through binary search on all the cells inside the page.
 *
 * If the order of cells on a page is not maintained, sorting would be required
 * during the balance process, which would be inefficient.
 *
 * If a cell is written on the page, we use the image_idx to find the starting
 * index of the cell header on the page. If a cell is not written on the page,
 * the entire cell would be in the CellTracker
 */
class CellTracker {
 public:
  ImageIndex image_idx;
  Cell cell;

  CellTracker();
  [[nodiscard]] bool IsCellWrittenIntoImage() const;
};

/**
 * @class NodePage
 *
 * @brief This class represents a page that is used as a node in a Btree.
 *
 * It holds p_image, a smart pointer to a byte array representing the page image
 * (inherited from BasePage). The class perform operations on the page image by
 * manipulating its underlying bytes.
 *
 * The page header, free blocks, and cells are all stored in the page image.
 * There are no member variables that represent these data structures.
 * Instead, one can only access them using std::memcpy and the trivially
 * copyable byte view structs defined above.
 *
 * The member functions will only handle operations on the page image that do
 * not require any knowledge of other pages. This limits the functions to mainly
 * rearranging the cells and free blocks on the page. Other operations, such as
 * the insertion of cells, require the use of other pages such as overflow
 * pages. These operations are handled by the Btree class.
 *
 * NodePage is currently a derived class of the OverFreePage since a NodePage
 * might be added to the free list at some point and will need to handle the
 * responsibilities of writing the content of an Overfull page or a
 * FreeListInfoPage.
 */
class NodePage : public OverFreePage {
  friend class Btree;

 private:
  // Indicates if the page has been initialized.
  bool is_init_;

  // The pointer of the parent page. Used by the Btree to easily traverse the
  // tree.
  NodePage *p_parent_;

  // The number of free bytes on the page.
  u32 num_free_bytes_;

  bool is_overfull_;

  std::vector<CellTracker> cell_trackers_;

 public:
  // Constructor and destructor
  NodePage();
  void DestroyExtra() override;

  // -----------------------------------------------------------------------------------------------

 private:
  // Private helper functions for setting and getting the byte views
  // These functions are meant to reduce the appearance of std::memcpy in the
  // code, which makes it harder to read.

  // Sets the NodePageHeaderByteView
  void SetNodePageHeaderByteView(
      NodePageHeaderByteView &tree_page_header_byte_view_in);

  // Gets the NodePageHeaderByteView
  [[nodiscard]] NodePageHeaderByteView GetNodePageHeaderByteView() const;

  // Sets the FreeBlockByteView
  void SetFreeBlockByteView(ImageIndex start_idx,
                            FreeBlockByteView &free_block_byte_view_in);

  // Gets the FreeBlockByteView
  [[nodiscard]] FreeBlockByteView GetFreeBlockByteView(
      ImageIndex start_idx) const;

  // Sets the CellHeaderByteView
  void SetCellHeaderByteView(u16 cell_idx,
                             CellHeaderByteView &cell_header_byte_view_in);

  // Gets the CellHeaderByteView
  [[nodiscard]] CellHeaderByteView GetCellHeaderByteView(u16 cell_idx) const;

  [[nodiscard]] CellHeaderByteView GetCellHeaderByteViewByImageIndex(
      ImageIndex image_idx) const;

  void SetCellHeaderByteViewByImageIndex(
      ImageIndex image_idx, CellHeaderByteView &cell_header_byte_view_in);

  [[nodiscard]] bool IsOverfull() const;

 public:
  // Public functions for manipulating the page image
  ImageIndex AllocateSpace(u32 num_bytes_in);
  void ZeroPage();
  void DefragmentPage();
  void CopyPage(NodePage &dest);
  void DropCell(u16 cell_idx);
  void InsertCell(Cell &cell_in, u16 cell_idx);
  void FreeSpace(ImageIndex free_start_idx, u16 num_bytes_to_free);
  void RelinkCellList();

  // Public functions for inspecting the NodePage and its page image

  u32 GetNumCells();
  Cell GetCell(u16 cell_idx);

  // Public function for BasePage inheritance
  static std::unique_ptr<BasePage> CreateDerivedPage();
};