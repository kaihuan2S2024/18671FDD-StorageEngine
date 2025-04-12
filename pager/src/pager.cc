#include "pager.h"

std::vector<std::byte> PageRecord::ImageVector() {
  return {p_image_.begin(), p_image_.end()};
}

/**
 * The constructor of Pager
 * It initializes the pager with the given file name, max page number and extra
 * size. In the original C code, this function is called sqlitepager_open
 *
 * Note that extra size denotes the size of the extra bytes needed to represent
 * a Node Page in the memory.
 * However, we might not need this extra size in C++ implementation.
 * By calling the constructor of NodePage, it will automatically allocate the
 * extra bytes needed. Hence, the Pager does not need to know the extra size.
 */
Pager::Pager(std::string &file_name, int max_page_num, EvictionPolicy policy) {
  // We are using the OsFile(std::string) constructor
  // such that OsOpenReadWrite/Exclusive/Readonly will have filename_
  // initialized when it is called.
  std::unique_ptr fd = std::make_unique<OsFile>(file_name);
  is_read_only_ = false;
  // we skip the scenario where the file does not have a file name
  ResultCode rc = fd->OsOpenReadWrite(file_name, is_read_only_);
  if (rc != ResultCode::kOk) throw SqliteException(ResultCode::kCantOpen);

  // handled the nomem scenario outside in the create function

  // create the pager
  file_name_ = file_name;
  journal_file_name_ = file_name + "-journal";
  checkpoint_journal_file_name_ = file_name + "-checkpoint";
  fd_ = std::move(fd);
  journal_fd_ = std::make_unique<OsFile>(journal_file_name_);
  is_journal_open_ = false;
  is_checkpoint_journal_open_ = false;
  is_checkpoint_journal_use_ = false;
  num_mem_pages_ref_positive_ = 0;
  num_database_size_ =
      -1;  // The number of pages in the database file is initialized to -1
  checkpoint_size_ = 0;
  checkpoint_journal_size_ = 0;
  num_mem_pages_ = 0;
  num_mem_pages_max_ = max_page_num > kMaxPageNum ? max_page_num : kMaxPageNum;
  lock_state_ = SqliteLockState::K_SQLITE_UNLOCK;
  is_journal_sync_allowed_ = true;
  is_journal_need_sync_ = false;
  p_free_page_first_ = nullptr;
  p_free_page_last_ = nullptr;
  page_hash_table_ =
      std::make_unique<std::map<PageNumber, std::unique_ptr<BasePage>>>();
  eviction_policy_ = policy;
}

/*
 * Change the maximum number of pages in the cache.
 */
void Pager::SqlitePagerSetCachesize(int max_page_num) {
  // we temporarily ignore the scenario where the max_page_num smaller than zero
  if (max_page_num > kMaxPageNum) num_mem_pages_max_ = max_page_num;
}

/**
 * Loads a page into the cache by its page number.
 * If the page is already in the cache, it returns a pointer to the page.
 *
 * If the page is not in the cache, it will read the bytes in the file and
 * create an instance of a derived class from BasePage with its page image byte
 * array with bytes from the file.
 *
 * If the page requested is outside the range of the database file, such as
 * requesting a page when the database file hasn't been created, it will simply
 * create a derived class instance and fill its page image byte array with 0s.
 *
 */
ResultCode Pager::SqlitePagerGet(
    PageNumber page_number, BasePage **pp_page,
    const std::function<std::unique_ptr<BasePage>()> &create_page) {
  BasePage *p_page = nullptr;
  // first check if page_number is valid
  if (page_number == 0) return ResultCode::kError;

  if (err_mask_.size() >
      err_mask_.count(SqlitePagerError::K_PAGER_ERROR_FULL)) {
    return SqlitePagerPrivateRetrieveError();
  }

  // If the number of pages with positive reference count is 0, then this is the
  // first page accessed. In that case, we apply a read lock.
  if (num_mem_pages_ref_positive_ == 0) {
    ResultCode rc = fd_->OsReadLock();
    if (rc != ResultCode::kOk) {
      return rc;
    }
    lock_state_ = SqliteLockState::K_SQLITE_READ_LOCK;
    if (journal_fd_->OsFileExists() == ResultCode::kOk) {
      /* If a journal file exists, try to play it back */
      rc = fd_->OsWriteLock();
      if (rc != ResultCode::kOk) {
        rc = fd_->OsUnlock();
        if (rc != ResultCode::kOk) return ResultCode::kError;
        return ResultCode::kBusy;
      }
      lock_state_ = SqliteLockState::K_SQLITE_WRITE_LOCK;

      std::unique_ptr journal_fd = std::make_unique<OsFile>();
      rc = journal_fd->OsOpenReadWrite(journal_file_name_, is_read_only_);
      if (rc != ResultCode::kOk) {
        rc = fd_->OsUnlock();
        if (rc != ResultCode::kOk) return ResultCode::kError;
        return ResultCode::kBusy;
      }
      journal_fd_ = std::move(journal_fd);
      is_journal_open_ = true;
      // not yet implemented, implement in doing transaction / rollback
      rc = SqlitePagerPrivatePlayback();
      if (rc != ResultCode::kOk) {
        return rc;
      }
    }
  } else {
    // if it is not the first page accessed
    // check if the page is in the cache
    p_page = SqlitePagerPrivateCacheLookup(page_number);
  }

  // At this point, the only way for p_page to be not null it from the cache
  // lookup in the else block above. If p_page is null, then the page is not in
  // the cache.
  if (p_page == nullptr) {
    // if the page is not in the cache
    num_pages_miss_++;
    // create a new page
    if (num_mem_pages_ < num_mem_pages_max_ || p_free_page_first_ == nullptr) {
      try {
        page_hash_table_->operator[](page_number) = create_page();
      } catch (const std::bad_alloc &) {
        *pp_page = nullptr;
        SqlitePagerPrivateUnWriteLock();
        err_mask_.insert(SqlitePagerError::K_PAGER_ERROR_MEM);
        return ResultCode::kNoMem;
      } catch (const SqliteException &e) {
        return e.code();
      }
      SqlitePagerPrivateAddCreatedPageToCache(page_number, p_page);
      num_mem_pages_++;
    } else {
      // this means that the cache is currently full, we have to evict one page
      // to make room
      p_page = evictPage();
      if (p_page == nullptr) {
        // this means that all pages are dirty, we have to sync the journal fill
        ResultCode rc = SqlitePagerPrivateSyncAllPages();
        if (rc != ResultCode::kOk) {
          // if the sync is unsuccessful, rollback.
          SqlitePagerRollback();
          return ResultCode::kIOError;
        }
        p_page = p_free_page_first_;
      }
      SqlitePagerPrivateRemovePageFromCache(page_number, p_page);
      num_pages_overflow_++;
    }

    p_page->p_header_->page_number_ = page_number;
    // set in journal and in checkpoint
    if (page_journal_bit_map_.count() &&
        page_number <= num_database_original_size_) {
      // this line needs more explanation
      p_page->p_header_->is_in_journal_ =
          (bool)page_journal_bit_map_[page_number];
    } else {
      p_page->p_header_->is_in_journal_ = false;
    }

    if (page_checkpoint_journal_bit_map_.count() &&
        page_number <= checkpoint_size_) {
      p_page->p_header_->is_in_checkpoint_ =
          (bool)page_checkpoint_journal_bit_map_[page_number];
    } else {
      p_page->p_header_->is_in_checkpoint_ = false;
    }

    p_page->p_header_->is_dirty_ = false;
    p_page->p_header_->num_ref_ = 1;
    num_mem_pages_ref_positive_++;

    if (num_database_size_ < 0) {
      SqlitePagerPageCount();
    }
    if (num_database_size_ >= page_number) {
      // this means that the page is in the database file, and we have to read
      // the page from the database file should OS seek take a u32 instead of a
      // int?
      fd_->OsSeek((page_number - 1) * kPageSize);
      // we should transfer string stream back to a byte array
      std::vector<std::byte> img_vec = p_page->ImageVector();
      ResultCode rc = fd_->OsRead(img_vec, kPageSize);
      std::copy(img_vec.begin(), img_vec.end(), p_page->p_image_->begin());

      if (rc != ResultCode::kOk) {
        return rc;
      }
    }
    // the extra set has been completed in the factory create_pag
  } else {
    // if the page is in the cache
    num_pages_hit_++;
    SqlitePagerRefPrivate(p_page);
  }
  updateLRU(p_page);  // Update LRU for accessed page
  *pp_page = p_page;
  return ResultCode::kOk;
}

/*
 * This function looks up a page in the cache by its page number.
 * If the page is in the cache, it returns a pointer to the page.
 * If it does not, it returns nullptr.
 *
 * The difference between this function and SqlitePagerGet is that
 * SqlitePagerGet will read bytes from the database file into the cache
 * if the page is not already in cache.
 *
 * This function should only be called when you are certain that the page is in
 * the cache.
 */
ResultCode Pager::SqlitePagerLookup(PageNumber page_number,
                                    BasePage **pp_page) {
  if (page_number == 0) {
    return ResultCode::kFormat;
  }

  // if there is an error other than pager is full
  if (err_mask_.size() >
      err_mask_.count(SqlitePagerError::K_PAGER_ERROR_FULL)) {
    return ResultCode::kError;
  }

  if (num_mem_pages_ref_positive_ == 0) {
    return ResultCode::kEmpty;
  }

  *pp_page = SqlitePagerPrivateCacheLookup(page_number);

  // If the page is in the cache, increase its reference count
  if (pp_page) {
    SqlitePagerRefPrivate(*pp_page);
    updateLRU(*pp_page);  // Update LRU when page is found
  }

  return ResultCode::kOk;
}

/*
 * Increases the reference count of a page.
 */
ResultCode Pager::SqlitePagerRef(BasePage *p_page) {
  SqlitePagerRefPrivate(p_page);
  updateLRU(p_page);  // Update LRU when page is referenced
  return ResultCode::kOk;
}

/*
 * Release a page, if the ref is down to 0, add the page to LRU list, when pager
 * ref is 0, release all, do a rollback and remove all the locks
 */
ResultCode Pager::SqlitePagerUnref(BasePage *p_page) {
  p_page->p_header_->num_ref_--;
  if (p_page->p_header_->num_ref_ == 0) {
    if (eviction_policy_ == EvictionPolicy::LRU) {
      lru_list_.push_front(p_page);
      lru_map_[p_page->p_header_->page_number_] = lru_list_.begin();
    } else {
      // Original handling for FIRST_NON_DIRTY
      p_page->p_header_->p_next_free_ = nullptr;
      p_page->p_header_->p_prev_free_ = p_free_page_last_;
      p_free_page_first_ = p_page;

      if (p_page->p_header_->p_prev_free_ != nullptr) {
        p_page->p_header_->p_prev_free_->p_header_->p_next_free_ = p_page;
      } else {
        p_free_page_first_ = p_page;
      }

      /* When all pages reach the freelist, drop the read lock from
      ** the database file.
      */
      if (--num_mem_pages_ref_positive_ == 0) {
        SqlitePagerPrivatePagerReset();
      }
    }
  }
  return ResultCode::kOk;
}

/**
 * @brief It gives the pager the permission to write target page
 *
 * This function is called to write changes to a page.
 *  1. If the page hasn't already been recorded in the journal, the original
 * page number and contents are written into journal_fd_.
 *  2. The page_journal_bit_map_ is updated to mark that the page is in the
 *  journal. When a page is written, its page number and contents are saved to
 *  journal_fd_ unless already present.
 *  3. The page_journal_bit_map_ bitmap marks pages included in the journal, and
 * is_journal_need_sync_ is set if syncing is needed.
 */
ResultCode Pager::SqlitePagerWrite(BasePage *p_page) {
  // if there is any other error
  if (!err_mask_.empty()) return ResultCode::kError;
  if (is_read_only_) return ResultCode::kPerm;
  /* Mark the page as dirty.  If the page has already been written
  ** to the journal then we can return right away.
  */
  p_page->p_header_->is_dirty_ = true;
  updateLRU(p_page);  // Update LRU when page is modified

  // if page is in journal already, and it is in checkpoint, or we don't use
  // checkpoint
  if (p_page->p_header_->is_in_journal_ &&
      (p_page->p_header_->is_in_checkpoint_ || !is_checkpoint_journal_use_)) {
    //  flag to indicate that pager contains one or more dirty pages
    is_dirty_ = true;
    return ResultCode::kOk;
  }
  // if so, doesn't the whole journal only contains one page? no, because it
  // will only create new journal file if the pager is in readlock, which means
  // the journal file is empty
  ResultCode rc = SqlitePagerBegin(p_page);
  is_dirty_ = true;
  if (rc != ResultCode::kOk) return rc;

  // write to transaction journal if not exists the journal should be the
  // original database file, the first time it should be empty, so we won't go
  // to write it
  if (!p_page->p_header_->is_in_journal_ &&
      p_page->p_header_->page_number_ <= num_database_original_size_) {
    // if the page is not in journal
    rc = journal_fd_->OsWrite(p_page->p_header_->PageNumberVector());
    if (rc == ResultCode::kOk) {
      rc = journal_fd_->OsWrite(*p_page->p_image_);
    }
    if (rc != ResultCode::kOk) {
      SqlitePagerRollback();
      // means that the journal is full
      err_mask_.insert(SqlitePagerError::K_PAGER_ERROR_FULL);
      return rc;
    }
    page_journal_bit_map_[p_page->p_header_->page_number_] = true;
    is_journal_need_sync_ = is_journal_sync_allowed_;
    p_page->p_header_->is_in_journal_ = true;
    if (is_checkpoint_journal_use_) {
      page_checkpoint_journal_bit_map_[p_page->p_header_->page_number_] = true;
      p_page->p_header_->is_in_checkpoint_ = true;
    }
  }

  // write to checkpoint journal if not exists
  if (is_checkpoint_journal_use_ && !p_page->p_header_->is_in_checkpoint_ &&
      p_page->p_header_->page_number_ <= num_database_original_size_) {
    rc = checkpoint_journal_fd_->OsWrite(p_page->p_header_->PageNumberVector());
    if (rc == ResultCode::kOk) {
      rc = checkpoint_journal_fd_->OsWrite(p_page->ImageVector());
    }
    if (rc != ResultCode::kOk) {
      SqlitePagerRollback();
      // means that the checkpoint journal is full
      err_mask_.insert(SqlitePagerError::K_PAGER_ERROR_FULL);
      return rc;
    }
    page_checkpoint_journal_bit_map_[p_page->p_header_->page_number_] = true;
    p_page->p_header_->is_in_checkpoint_ = true;
  }
  // update the database file size and return.
  if (num_database_size_ < p_page->p_header_->page_number_) {
    num_database_size_ = p_page->p_header_->page_number_;
  }

  return rc;
}

/**
 * Return TRUE if the page given in the argument was previously passed
 * to sqlitepager_write().  In other words, return TRUE if it is ok
 * to change the content of the page.
 */
bool Pager::SqlitePagerIsWritable(BasePage *p_page) {
  return p_page->p_header_->is_dirty_;
}

/**
 * Returns the number of pages in the database file. (NOT cache)
 *
 * If num_database_size_ == -1, the pager has never checked the database file
 * size after initialization. In this case, the function will call OsFileSize to
 * get the database file size, divide the file size by the page size to get the
 * page count, and finally store it in num_database_size_
 *
 */
u32 Pager::SqlitePagerPageCount() {
  u32 db_file_size = 0;  // the database file size in bytes
  if (num_database_size_ >= 0) {
    return num_database_size_;  // TODO: Check why it's ok to return 0 when
                                // num_database_size_ == 0
  }
  // The variable db_file_size is passed by reference
  // If the result code is kOK, then db_file_size will be the size of the
  // database file in bytes
  if (fd_->OsFileSize(db_file_size) != ResultCode::kOk) {
    err_mask_.insert(SqlitePagerError::K_PAGER_ERROR_DISK);
    return 0;
  }
  db_file_size /= kPageSize;
  if (lock_state_ != SqliteLockState::K_SQLITE_UNLOCK) {
    num_database_size_ = (int)db_file_size;
  }
  return db_file_size;
}

/**
 * Returns the page number of a page
 */
PageNumber Pager::SqlitePagerPageNumber(BasePage *p_page) {
  return p_page->p_header_->page_number_;
}

// below is the journal/ checkpoint related functions

/**
 * @brief Acquire a write-lock on the database.
 * initialize journal, record the current database size
 *
 * The method begins a transaction by acquiring a write lock on the database.
 * If the Pager is in a read-lock state (K_SQLITE_READ_LOCK), it prepares the
 * journal file for writing by opening it (OsOpenExclusive) and writing the
 * kAJournalMagic and current database page count (num_database_size_).
 *
 * The lock is removed if any of the following occurs:
 * 1. SqlitePagerCommit() is called
 * 2. SqlitePagerRollback() is called
 * 3. The Pager destructor is called
 * 4. SqlitePagerUnref() is called on every outstanding page
 *
 * (Note that SqlitePagerWrite() can only be called on an outstanding page which
 * means that the pager must be in SQLITE_READLOCK before it transitions to
 * SQLITE_WRITELOCK.)
 */
ResultCode Pager::SqlitePagerBegin(BasePage *p_page) {
  ResultCode rc = ResultCode::kOk;
  assert(p_page->p_header_->num_ref_ > 0);
  assert(lock_state_ != SqliteLockState::K_SQLITE_UNLOCK);
  if (lock_state_ == SqliteLockState::K_SQLITE_READ_LOCK) {
    // if the pager is in read lock, it means that the journal is empty, we
    // should create new one
    assert(page_journal_bit_map_.empty());
    rc = fd_->OsWriteLock();
    if (rc != ResultCode::kOk) {
      return rc;
    }

    // SqlitePagerPageCount(); // in theory, we cannot have a -1 here
    //  A bad_alloc occurs when calling boost::dynamic_bitset<>(-1)
    //  it shouldn't be -1 since the pager
    page_journal_bit_map_ = boost::dynamic_bitset<>(
        std::max(kBitMapPlaceHolder, kBitMapPlaceHolder + num_database_size_));

    // In the Sqlite source code, there is an if statement here
    // that checks if calling malloc to allocate memory for
    // page_journal_bit_map_ returns a nullptr.
    // If such, the function returns SQLITE_NOMEM
    // Since we are not using malloc here, the if statement is omitted.

    rc = journal_fd_->OsOpenExclusive(0);
    if (rc != ResultCode::kOk) {
      page_journal_bit_map_.resize(kBitMapPlaceHolder);
      fd_->OsReadLock();
      return ResultCode::kCantOpen;
    }
    is_journal_open_ = true;
    is_journal_need_sync_ = false;
    is_dirty_ = false;
    lock_state_ = SqliteLockState::K_SQLITE_WRITE_LOCK;
    SqlitePagerPageCount();
    num_database_original_size_ = num_database_size_;

    // Attempt to write these two variables into the journal file
    // 1. kAJournalMagic: An 8 byte magic string that identifies the journal
    // file as a proper journal file
    // 2. num_database_size_: A 4 byte integer representing the number of pages
    // in the database file

    // we must assume that the journal file is empty (begin as empty or cleared)
    // before we start writing to the journal
    rc = journal_fd_->OsWrite(kAJournalMagic);
    if (rc == ResultCode::kOk) {
      // write num of database into the journal file
      std::vector<std::byte> db_size_vec(sizeof(num_database_size_));
      std::memcpy(db_size_vec.data(), &num_database_size_,
                  sizeof(num_database_size_));
      rc = journal_fd_->OsWrite(db_size_vec);
    }

    if (rc != ResultCode::kOk) {
      rc = SqlitePagerPrivateUnWriteLock();
      if (rc == ResultCode::kOk) rc = ResultCode::kFull;
    }
  }
  return rc;
}

/**
 * @brief It commits all changes to the database file and release the write
 * lock.
 * 1. Commits all changes(dirty pages) made during the transaction to the
 * database.
 * 2. Ensures all changes are written from memory to the database file (fd_) and
 * performs a file sync if required (OsSync).
 * 3. The journal file is truncated or deleted, and the lock state is reset to
 * K_SQLITE_READ_LOCK.
 */
ResultCode Pager::SqlitePagerCommit() {
  ResultCode rc;

  if (err_mask_.count(SqlitePagerError::K_PAGER_ERROR_FULL)) {
    rc = SqlitePagerRollback();
    if (rc == ResultCode::kOk) rc = ResultCode::kFull;
    return rc;
  }
  if (!err_mask_.empty()) {
    rc = SqlitePagerPrivateRetrieveError();
    return rc;
  }
  if (lock_state_ != SqliteLockState::K_SQLITE_WRITE_LOCK) {
    return ResultCode::kError;
  }
  assert(is_journal_open_);
  if (is_dirty_ == 0) {
    /* Exit early (without doing the time-consuming sqliteOsSync() calls)
    ** if there have been no changes to the database file. */
    rc = SqlitePagerPrivateUnWriteLock();
    num_database_size_ = -1;
    return rc;
  }

  // make sure the content has all been written to the file
  if (is_journal_need_sync_ && journal_fd_->OsSync() != ResultCode::kOk)
    SqlitePagerPrivateCommitAbort();

  // Iterate through the linked list of page headers and write the page image
  // associated with each page header into the disk
  for (BasePage *cur_page = p_all_page_first_; cur_page != nullptr;
       cur_page = cur_page->p_header_->p_next_all_) {
    if (cur_page->p_header_->is_dirty_ == 0) continue;
    rc = fd_->OsSeek((cur_page->p_header_->page_number_ - 1) * kPageSize);
    if (rc != ResultCode::kOk) SqlitePagerPrivateCommitAbort();
    rc = fd_->OsWrite(cur_page->ImageVector());
    if (rc != ResultCode::kOk) SqlitePagerPrivateCommitAbort();
  }
  if (is_journal_sync_allowed_ && fd_->OsSync() != ResultCode::kOk)
    SqlitePagerPrivateCommitAbort();
  rc = SqlitePagerPrivateUnWriteLock();
  num_database_size_ = -1;
  return rc;
}

/**
 * Restores the database to its state before the transaction begins by reading
 * the original pages from the journal file and writing them back to the
 * database file.
 *
 * SqlitePagerPrivatePlayback() is called to handle the actual playback of the
 * journal entries.
 *
 * The page number is extracted, and data is copied back to restore the original
 * database state.
 */
ResultCode Pager::SqlitePagerRollback() {
  ResultCode rc;
  if (err_mask_.size() >
      err_mask_.count(SqlitePagerError::K_PAGER_ERROR_FULL)) {
    // have pager error_full, make pager cache small to test this branch
    if (lock_state_ == SqliteLockState::K_SQLITE_WRITE_LOCK) {
      // pager is in write lock
      SqlitePagerPrivatePlayback();
    }
    return SqlitePagerPrivateRetrieveError();
  }
  if (lock_state_ != SqliteLockState::K_SQLITE_WRITE_LOCK) {
    // when pager is not in write lock, do nothing
    return ResultCode::kOk;
  }
  // play back one page
  rc = SqlitePagerPrivatePlayback();
  if (rc != ResultCode::kOk) {
    rc = ResultCode::kCorrupt;
    err_mask_.insert(SqlitePagerError::K_PAGER_ERROR_CORRUPT);
  }
  // this means that the number of pages in the file is again unknown, should be
  // recalculated again
  num_database_size_ = -1;
  return rc;
}

bool Pager::SqlitePagerIsReadOnly() { return is_read_only_; }

void Pager::SqlitePagerDontWrite(PageNumber page_number) {
  BasePage *cur_page = SqlitePagerPrivateCacheLookup(page_number);
  if (cur_page != nullptr) {
    // no need for the second condition
    cur_page->p_header_->is_dirty_ = false;
  }
}

void Pager::SqlitePagerRefPrivate(BasePage *p_page) {
  if (p_page->p_header_->num_ref_ == 0) {
    // page is currently in the free list, so remove it
    if (p_page->p_header_->p_prev_free_ != nullptr) {
      p_page->p_header_->p_prev_free_->p_header_->p_next_free_ =
          p_page->p_header_->p_next_free_;
    } else {
      p_free_page_first_ = p_page->p_header_->p_next_free_;
    }

    if (p_page->p_header_->p_next_free_ != nullptr) {
      p_page->p_header_->p_next_free_->p_header_->p_prev_free_ =
          p_page->p_header_->p_prev_free_;
    } else {
      p_free_page_last_ = p_page->p_header_->p_prev_free_;
    }
    num_mem_pages_ref_positive_++;
  }
  p_page->p_header_->num_ref_++;
}

// TODO-test: reset the page
void Pager::SqlitePagerPrivatePagerReset() {
  if (eviction_policy_ == EvictionPolicy::FIRST_NON_DIRTY) {
    page_hash_table_->clear();
  } else {
    lru_map_.clear();
    lru_list_.clear();
  }
  p_all_page_first_ = nullptr;
  p_free_page_first_ = nullptr;
  p_free_page_last_ = nullptr;
  num_mem_pages_ = 0;
  if (lock_state_ == SqliteLockState::K_SQLITE_WRITE_LOCK) {
    SqlitePagerRollback();
  }
  fd_->OsUnlock();
  lock_state_ = SqliteLockState::K_SQLITE_UNLOCK;
  num_database_size_ = -1;
  num_mem_pages_ref_positive_ = 0;
}

ResultCode Pager::SqlitePagerPrivateRetrieveError() const {
  ResultCode rc = ResultCode::kOk;
  if (err_mask_.count(SqlitePagerError::K_PAGER_ERROR_LOCK))
    rc = ResultCode::kProtocol;
  if (err_mask_.count(SqlitePagerError::K_PAGER_ERROR_DISK))
    rc = ResultCode::kIOError;
  if (err_mask_.count(SqlitePagerError::K_PAGER_ERROR_FULL))
    rc = ResultCode::kFull;
  if (err_mask_.count(SqlitePagerError::K_PAGER_ERROR_MEM))
    rc = ResultCode::kNoMem;
  if (err_mask_.count(SqlitePagerError::K_PAGER_ERROR_CORRUPT))
    rc = ResultCode::kCorrupt;
  return rc;
}

// TODO-test
// abort the transaction
ResultCode Pager::SqlitePagerPrivateCommitAbort() {
  ResultCode rc = SqlitePagerRollback();
  if (rc == ResultCode::kOk) {
    rc = ResultCode::kFull;
  }
  return rc;
}

// below is the implementation of BasePage
void BasePage::InitPageHeader(Pager *pager, PageNumber page_number) {
  // Make the new page the first page in the linked list
  p_header_ = std::make_unique<PageHeader>(pager, page_number);
  p_header_->p_next_all_ = pager->p_all_page_first_;

  // If there was a page that was previously the first page in the linked list,
  // it is now placed as the next page after the new page

  if (pager->p_all_page_first_ != nullptr)
    pager->p_all_page_first_->p_header_->p_prev_all_ = this;

  // There should be no previous page for this new page
  p_header_->p_prev_all_ = nullptr;
}

BasePage *BasePage::GetFirstNonDirtyPage() {
  BasePage *p_page = this;
  while (p_page != nullptr && p_page->p_header_->is_dirty_)
    p_page = p_page->p_header_->p_next_free_;
  return p_page;
}

// currently the getter, we copy the array to a vector
std::vector<std::byte> BasePage::ImageVector() {
  return {p_image_->begin(), p_image_->end()};
}

// below is the implementation of Page Header
void PageHeader::PageRef() {
  if (num_ref_ == 0) {
    /* The page is currently on the freelist.  Remove it. */
    if (p_prev_free_ != nullptr) {
      p_prev_free_->p_header_->p_next_free_ = p_next_free_;
    } else {
      p_pager_->p_free_page_first_ = p_next_free_;
    }

    if (p_next_free_ != nullptr) {
      p_next_free_->p_header_->p_prev_free_ = p_prev_free_;
    } else {
      p_pager_->p_free_page_last_ = p_prev_free_;
    }

    p_pager_->num_mem_pages_ref_positive_++;
  }
  num_ref_++;
}

std::vector<std::byte> PageHeader::PageNumberVector() {
  // TODO: Check if this is the correct way to replace the original
  // reinterpret_cast solution
  std::vector<std::byte> page_number_vector(sizeof(page_number_));
  std::memcpy(page_number_vector.data(), &page_number_, sizeof(page_number_));
  return page_number_vector;
}