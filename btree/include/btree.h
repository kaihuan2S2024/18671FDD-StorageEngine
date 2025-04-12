#pragma once

#include <cstddef>
#include <memory>
#include <numeric>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "first_page.h"
#include "node_page.h"
#include "over_free_page.h"
#include "pager.h"
#include "sql_int.h"
#include "sql_limit.h"
#include "sql_rc.h"

/**
 * @class BtCursor
 *
 * @brief A BtCursor is an iterator that allows the VDBE layer to traverse the
 * Btree.
 *
 * From an external perspective, the requests a BtCursor from the Btree class
 * and uses it to traverse the Btree.
 *
 * From an internal perspective, the Btree class has to keep track of
 * information about each cursor. These include the root page number (identifies
 * the table or index), which NodePage it is pointing to, and which cell inside
 * a NodePage it is pointing to.
 *
 * The codebase attempts to achieve this by keeping track of a set of BtCursor
 * objects whose private variables are only visible to the Btree and not the
 * VDBE layer. This way, the BtCursor can act both as an iterator to the VDBE
 * layer and a BtCursor information container to the Btree.
 *
 * In order to make the Btree a Singleton, the methods
 * @code
 * GetInstance(const std::string &filename, int cache_size)
 * GetInstance().
 * @endcode
 *
 * The VDBE layer can call the first method once. After that, it can call the
 * second one to get the same instance. If it calls the second one first, it
 * will throw an exception.
 */
class BtCursor {
  friend class Btree;

 private:
  PageNumber root_page_number;
  NodePage *p_page;
  u16 cell_index;
  bool writable;
  bool skip_next;
  int compare_result;

 public:
  BtCursor();
};

struct SharedBtCursorPtrHash {
  std::size_t operator()(const std::shared_ptr<BtCursor> &ptr) const {
    return std::hash<std::shared_ptr<BtCursor>>()(ptr);
  }
};

struct SharedBtCursorPtrEqual {
  bool operator()(const std::shared_ptr<BtCursor> &a,
                  const std::shared_ptr<BtCursor> &b) const {
    return a == b;
  }
};

/**
 * @class Btree
 *
 * This is the class responsible to handles all the Btree operations and
 * interactions with the VDBE layer. From an external perspective, one instance
 * of a Btree allows the VDBE layer to multiple tables / indexes and multiple
 * BtCursors on those table / indexes.
 *
 * From an internal perspective, the Btree class has to keep track of all the
 * BtCursors and link the pages together to form a Btree.
 *
 * @note The Btree layer creates an interface for the VDBE layer to write and
 * retrieve key-data pairs as if they were stored in an ordered map. Under the
 * hood, it calls the Pager to read and write data into multiple pages and link
 * them into a Btree, a data structure that allows for log m (N) time complexity
 * for insertion, deletion, and search operations. N is the size of the database
 * file and m is the page size.
 *
 */
class Btree {
  friend class BtreeAccessor;

 private:
  // It keeps track of BtCursors that are currently in use
  std::unordered_set<std::shared_ptr<BtCursor>, SharedBtCursorPtrHash,
                     SharedBtCursorPtrEqual>
      bt_cursor_set_;

  // It keeps track of which pages are locked and how many times they are locked
  std::unordered_map<PageNumber, int> lock_count_map_;

  std::string filename_;          // Database file name
  bool has_writable_bt_cursor_;   // Whether there is a writable BtCursor
  std::unique_ptr<Pager> pager_;  // A pointer to an instance of a Pager
  bool read_only_;                // Whether the database is read only
  bool in_trans_;                 // Whether the database is in a transaction
  bool in_ckpt_;                  // Whether the database is in a checkpoint

  // Pointer to first page.
  FirstPage *p_first_page_;

  // These are functions that don't involve BtCursor and are privately used by
  // the Btree class

  ResultCode NewDatabase();  // Creates a new database
  // Locks the Btree, basically putting First Page in memory
  ResultCode LockBtree();
  ResultCode UnlockBtreeIfUnused();  // Unlocks the Btree if it is not in use

  // Initializes a page. Set up variables in memory using information from the
  // page's image.
  ResultCode InitPage(NodePage &node_page, NodePage *p_parent);

  // Allocating pages.
  ResultCode AllocatePage(NodePage *&p_node_page, PageNumber &page_number);
  ResultCode FreePage(BasePage *&p_input_base_page, PageNumber &page_number,
                      bool is_overflow_page);

  ResultCode ClearCell(NodePage &node_page, u16 cell_idx);
  ResultCode FillInCell(Cell &cell_in);
  void ReParentPage(PageNumber page_number, NodePage *p_new_parent);
  void ReParentChildPages(NodePage &node_page);
  ResultCode ClearDatabasePage(PageNumber page_number, bool free_page);

  // ######################  BtCursor Public Functions   ######################
  // These are functions that involve BtCursor and are publicly used by the
  // Btree class

  void GetTempCursor(BtCursor &cursor, BtCursor &temp_cursor);
  void ReleaseTempCursor(BtCursor &temp_cursor);
  ResultCode GetPayload(const BtCursor &cursor, u32 offset, u32 amount,
                        std::vector<std::byte> &result);
  ResultCode MoveToChild(BtCursor &cursor, PageNumber child_page_number);
  ResultCode MoveToParent(BtCursor &cursor);
  ResultCode MoveToRoot(BtCursor &cursor);
  ResultCode MoveToLeftmost(BtCursor &cursor);

  // Btree Private Functions: Balance
  ResultCode Balance(NodePage *p_page, const std::weak_ptr<BtCursor> &p_cursor);

  // These are helper functions used inside the balance function
  ResultCode BalanceHelperHandleRoot(NodePage *&p_page, NodePage *&p_parent,
                                     const std::weak_ptr<BtCursor> &p_cursor,
                                     NodePage *&p_extra_unref,
                                     bool &return_ok_early);
  int BalanceHelperFindChildIdx(NodePage *p_page, NodePage *p_parent);

  Btree(const std::string &filename);
  static Btree *instance_;

 public:
  Btree(Btree &other) = delete;
  void operator=(const Btree &) = delete;
  static Btree &GetInstance(const std::string &filename);
  static Btree &GetInstance();
  static Btree &RebuildInstance(const std::string &filename);

  // ############################ Btree Public Functions ####################
  Btree(std::string filename, int cache_size);
  ResultCode BtreeSetCacheSize(int cache_size);
  ResultCode BtreeBeginTrans();
  ResultCode BtreeCommit();
  ResultCode BtreeRollback();

  ResultCode BtreeBeginCkpt();
  ResultCode BtreeCommitCkpt();
  ResultCode BtreeRollbackCkpt();

  // For create table and index, Btree decides what the root_page_number is and
  // returns by reference
  ResultCode BtreeCreateTable(PageNumber &root_page_number);
  ResultCode BtreeCreateIndex(PageNumber &root_page_number);

  // For clear table and drop table, you pass in the root_page_number obtained
  // from table and index creation
  ResultCode BtreeClearTable(PageNumber root_page_number);
  ResultCode BtreeDropTable(PageNumber root_page_number);

  // Helper function for testing, not part of the original sqlite3, meant to
  // help students find the page count by calling the pagers' PagerPageCount
  // function.
  u32 BtreePageCount();

  // BtCursor Public Functions
  ResultCode BtCursorCreate(PageNumber root_page_number, bool writable,
                            std::weak_ptr<BtCursor> &p_cursor_weak);
  ResultCode BtCursorClose(const std::weak_ptr<BtCursor> &p_cursor_weak);
  ResultCode BtreeKeySize(const std::weak_ptr<BtCursor> &p_cursor_weak,
                          u32 &key_size);
  u32 BtreeKey(const std::weak_ptr<BtCursor> &p_cursor_weak, u32 offset,
               u32 amount, std::vector<std::byte> &result);
  ResultCode BtreeDataSize(const std::weak_ptr<BtCursor> &p_cursor_weak,
                           u32 &data_size);
  u32 BtreeData(const std::weak_ptr<BtCursor> &p_cursor_weak, u32 offset,
                u32 amount, std::vector<std::byte> &result);
  ResultCode BtreeKeyCompare(const std::weak_ptr<BtCursor> &p_cursor_weak,
                             std::vector<std::byte> &key, u32 num_ignore,
                             int &result);
  ResultCode BtreeFirst(const std::weak_ptr<BtCursor> &p_cursor_weak,
                        bool &table_is_empty);
  ResultCode BtreeLast(const std::weak_ptr<BtCursor> &p_cursor_weak,
                       bool &table_is_empty);
  ResultCode BtreeMoveTo(const std::weak_ptr<BtCursor> &p_cursor_weak,
                         std::vector<std::byte> &key, int &result);
  ResultCode BtreeNext(const std::weak_ptr<BtCursor> &p_cursor_weak,
                       bool &already_at_last_entry);
  ResultCode BtreeInsert(const std::weak_ptr<BtCursor> &p_cursor_weak,
                         std::vector<std::byte> &key,
                         std::vector<std::byte> &data);

  ResultCode BtreeGetNodeDepth(const std::weak_ptr<BtCursor> &p_cursor_weak,
                               u32 &depth);

  ResultCode BtreeDelete(const std::weak_ptr<BtCursor> &p_cursor_weak);
  ResultCode BtreeGetMeta(std::array<int, kMetaIntArraySize> &meta_int_arr);
  ResultCode BtreeUpdateMeta(std::array<int, kMetaIntArraySize> &meta_int_arr);
};

// Created for testing purposes to make it easier possible to test the Btree
// Singleton
class BtreeAccessor {
 private:
  Btree btree;

 public:
  Btree &GetBtree();
  BtreeAccessor(const std::string &filename);
};
