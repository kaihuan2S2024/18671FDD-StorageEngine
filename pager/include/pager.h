//
// Created by Yiyang Huo on 6/21/23.
//

#pragma once
#include <array>
#include <boost/dynamic_bitset.hpp>
#include <cstddef>
#include <list>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "os.h"
#include "sql_int.h"
#include "sql_limit.h"
#include "sql_rc.h"

// we just use the original sqlite definition
static const std::vector<std::byte> kAJournalMagic{
    std::byte{0xd9}, std::byte{0xd5}, std::byte{0x05}, std::byte{0xf9},
    std::byte{0x20}, std::byte{0xa1}, std::byte{0x63}, std::byte{0xd4},
};

// Since the database is 1-indexed, we have to make extra room for bitmap
static constexpr int kBitMapPlaceHolder = 1;
static constexpr int kMaxPageNum = 10;

// Definition of lock state
enum class SqliteLockState : u8 {
  K_SQLITE_UNLOCK = 0,
  K_SQLITE_READ_LOCK = 1,
  K_SQLITE_WRITE_LOCK = 2,
};

enum class SqlitePagerError : u8 {
  K_PAGER_ERROR_FULL = 0,
  K_PAGER_ERROR_MEM = 1,
  K_PAGER_ERROR_LOCK = 2,
  K_PAGER_ERROR_CORRUPT = 3,
  K_PAGER_ERROR_DISK = 4
};

// Define the eviction policy
enum class EvictionPolicy {
  FIRST_NON_DIRTY,
  LRU  // Least Frequently Used
};

// Added static const to fix linker error
static constexpr int kPagerErrorFull = 0x01;

class Pager;
class BasePage;

class PageRecord {
 public:
  PageNumber page_number_;
  std::array<std::byte, kPageSize> p_image_;
  std::vector<std::byte> ImageVector();
};

class PageHeader {
 public:
  Pager *p_pager_;                        // the Pager this page belongs to
  PageNumber page_number_;                // the page number of this page
  u32 num_ref_;                           // number of references to this page
  BasePage *p_prev_free_, *p_next_free_;  // free linked list for pages
  BasePage *p_prev_all_, *p_next_all_;    // all linked list for pages
  bool is_in_journal_;     // true if the page is currently in the journal file
  bool is_in_checkpoint_;  // true if the page is in the checkpoint journal file
  bool is_dirty_;  // true if the page has been modified since the last journal
                   // flush

  void PageRef();  // increment the reference count
  std::vector<std::byte>
  PageNumberVector();  // TO_TESTIFY: return the page number as a vector of byte

  explicit PageHeader(Pager *p_pager, PageNumber page_number)
      : p_pager_(p_pager),
        page_number_(page_number),
        num_ref_(0),
        p_prev_free_(nullptr),
        p_next_free_(nullptr),
        p_prev_all_(nullptr),
        p_next_all_(nullptr),
        is_in_journal_(false),
        is_in_checkpoint_(false),
        is_dirty_(false) {};
  ~PageHeader() = default;
};

/**
 * @class Pager
 *
 * @brief It is the main class responsible for managing database pages,
 * including reading, writing, and caching database file pages in memory. It
 * ensures that data is stored efficiently and safely, providing mechanisms for
 * transactions, caching, and rollback.
 *
 * @note Each open datafile is managed through a separate Pager object, and each
 * Pager object is associated with one and only one instance of an open
 * datafile. (At the pager module level, the object is synonymous to the
 * datafile, i.e., database connection.)
 */
class Pager {
 public:
  std::string file_name_;
  std::string journal_file_name_;
  std::string checkpoint_journal_file_name_;
  std::unique_ptr<OsFile> fd_, journal_fd_, checkpoint_journal_fd_;
  u32 checkpoint_size_, checkpoint_journal_size_;
  // extra bytes at the end of mempage, aka sizeof(Mempage) -
  // sizeof(page_image), TODO-Delete;
  u32 n_extra_size_{};
  u32 num_mem_pages_{};               // number of pages in memory
  u32 num_mem_pages_ref_positive_{};  // number of pages in memory with positive
                                      // reference count
  u32 num_mem_pages_max_{};  // maximum number of pages allowed in memory

  /* Cache hits, missing, and LRU overflows */
  u32 num_pages_hit_{}, num_pages_miss_{}, num_pages_overflow_{};
  int num_database_original_size_{};  // original size of the database file
  int num_database_size_{};           // The number of pages in the file
  bool is_journal_open_{};            // true if the journal file is open

  // true if the checkpoint journal file is open
  bool is_checkpoint_journal_open_{};

  // true if the checkpoint journal file is in use
  bool is_checkpoint_journal_use_{};
  bool is_journal_sync_allowed_{};  // true if the journal can sync
  SqliteLockState lock_state_{};    // current lock state
  std::unordered_set<SqlitePagerError>
      err_mask_{};             // TO_DELETE: seems like this one is not needed.
  bool is_temp_file_{};        // true if this is a temporary file
  bool is_read_only_;          // true if this is a read-only file
  bool is_journal_need_sync_;  // true if the journal needs to be synced
  bool is_dirty_{};            // true if the database has been modified

  // TO_TESTIFY: this is a quick bitmap to check if a page is in journal
  boost::dynamic_bitset<> page_journal_bit_map_;
  // TO_TESTIFY: this is a bitmap to check if a page is in checkpoint journal
  boost::dynamic_bitset<> page_checkpoint_journal_bit_map_;

  // below is relevant to caching
  BasePage *p_free_page_first_{}, *p_free_page_last_{};  // first and last page

  // Cannot delete, since it functions as the LRU list
  BasePage *p_all_page_first_{};
  EvictionPolicy eviction_policy_;

  // TO_TESTIFY: this is a hash table to find a page by page number, when the
  // map loss the reference to the page, the page will be deleted.
  std::unique_ptr<std::map<PageNumber, std::unique_ptr<BasePage>>>
      page_hash_table_;
  std::list<BasePage *> lru_list_;  // Maintain the LRU list

  // Map page numbers to their position in the LRU list
  std::unordered_map<PageNumber, std::list<BasePage *>::iterator> lru_map_;

  // Update the LRU list when a page is accessed
  void updateLRU(BasePage *p_page);
  // get the page to evict according to the eviction policy
  BasePage *evictPage();

  // constructors
  Pager(std::string &file_name, int max_page_num,
        EvictionPolicy policy = EvictionPolicy::FIRST_NON_DIRTY);

  void SqlitePagerSetCachesize(
      int max_page_num);  // TO_DELETE: seems like we don't need to dynamically
                          // change the cache size
  ResultCode SqlitePagerGet(
      PageNumber page_number, BasePage **pp_page,
      const std::function<std::unique_ptr<BasePage>()> &create_page);
  ResultCode SqlitePagerLookup(PageNumber page_number,
                               BasePage **pp_page);  // return a page if exists
  ResultCode SqlitePagerRef(
      BasePage *p_page);  // Increase the reference count of a page
  ResultCode SqlitePagerUnref(
      BasePage *p_page);  // Decrease the reference count of a page
  ResultCode SqlitePagerWrite(
      BasePage *p_page);  // Ask for access to write a page image
  bool SqlitePagerIsWritable(
      BasePage *p_page);       // return true if a page is writable
  u32 SqlitePagerPageCount();  // return the page count in the database file
                               // (NOT cache)
  PageNumber SqlitePagerPageNumber(
      BasePage *p_page);  // return the page number of a page

  // below is relevant to journal, checkpoint
  ResultCode SqlitePagerBegin(
      BasePage *p_page);             // this is to begin a transaction
  ResultCode SqlitePagerCommit();    // this is to commit a transaction
  ResultCode SqlitePagerRollback();  // this is to rollback a transaction
  bool SqlitePagerIsReadOnly();  // TO_DELETE: return true if the pager is read
                                 // only, we don't want readonly pager
  ResultCode SqlitePagerCkptBegin();     // this is to begin a checkpoint
  ResultCode SqlitePagerCkptCommit();    // this is to commit a checkpoint
  ResultCode SqlitePagerCkptRollback();  // this is to rollback a checkpoint
  void SqlitePagerDontWrite(
      PageNumber page_number);  // TO_DELETE: seems like we don't need this

 private:
  ResultCode SqlitePagerPrivatePlayback();
  ResultCode SqlitePagerPrivateCkptPlayback();
  ResultCode SqlitePagerPrivatePlaybackOnePage(OsFile *fd);
  BasePage *SqlitePagerPrivateCacheLookup(PageNumber page_number) const;
  ResultCode SqlitePagerPrivateSyncAllPages();
  void SqlitePagerRefPrivate(BasePage *p_page);
  void SqlitePagerPrivatePagerReset();
  ResultCode SqlitePagerPrivateUnWriteLock();
  ResultCode SqlitePagerPrivateRetrieveError() const;
  ResultCode SqlitePagerPrivateCommitAbort();
  void SqlitePagerPrivateAddCreatedPageToCache(PageNumber page_number,
                                               BasePage *&p_page);
  void SqlitePagerPrivateRemovePageFromCache(PageNumber page_number,
                                             BasePage *p_page);
};

/**
 * @class BasePage
 * @brief Represents a page content in memory.
 *
 * Every derived Page class should implement a static `create` method that
 * constructs an instance of the derived class and returns it as a
 * `std::unique_ptr<BasePage>`.
 *
 * Example usage in a derived class:
 * @code
 *   static std::unique_ptr<BasePage> create() {
 *       return std::make_unique<DerivedPage>();
 *   }
 * @endcode
 *
 * This `create` method should be used as the `create_page` function,
 * which is passed to `sqlite_pager_get()` to create new page objects of
 * the derived class.
 *
 * See SampleMemPage for an example implementation.
 */
class BasePage {
 public:
  // Image data that holds the content of a page
  std::unique_ptr<std::array<std::byte, kPageSize>> p_image_;

  BasePage()
      : p_image_(std::make_unique<std::array<std::byte, kPageSize>>()) {};

  // Virtual destructor to allow for polymorphism
  virtual ~BasePage() = default;

  // TO_TESTIFY: this is necessary to initialize the page header
  void InitPageHeader(Pager *pager, PageNumber page_number);
  BasePage *GetFirstNonDirtyPage();
  virtual void DestroyExtra() = 0;

  // This header space will still exist for any child classes, yet the child
  // classes will not be able to access the space (information hidding)
 private:
  // TO_TESTIFY: pointer to the page header, useful for Pager layer since btree
  // layer only has access to the page content
  std::unique_ptr<PageHeader> p_header_;
  // retrieve vector of byte pointer to p image
  std::vector<std::byte> ImageVector();

  friend class Pager;
  friend class PageHeader;
};

/**
 * @class SampleMemPage
 * @brief A test implementation of the BasePage class.
 *
 * This class is used for testing the functionality of the `BasePage` class.
 * It provides a concrete implementation that can be used in unit tests
 * or development scenarios to verify behavior.
 *
 * Example usage:
 * @code
 *   std::unique_ptr<BasePage> page = SampleMemPage::create();
 * @endcode
 *
 */
class SampleMemPage : public BasePage {
 public:
  static std::unique_ptr<BasePage> create() {
    return std::make_unique<SampleMemPage>();
  }
  void DestroyExtra() override {
    // destroy extra data here
  }
};