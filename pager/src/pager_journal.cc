#include "pager.h"

// Play back the transaction journal when the function is executed, the lock
// state must be go back to read lock
ResultCode Pager::SqlitePagerPrivatePlayback() {
  assert(is_journal_open_);
  u32 journal_size;
  u32 num_record;
  PageNumber max_page;
  ResultCode rc;
  std::vector<std::byte> magic_buffer(kAJournalMagic.size());
  std::vector<std::byte> page_number_buffer(sizeof(PageNumber));

  // first, find how many journal there is
  journal_fd_->OsSeek(0);
  rc = journal_fd_->OsFileSize(journal_size);
  if (rc != ResultCode::kOk) {
    SqlitePagerPrivateUnWriteLock();
    err_mask_.insert(SqlitePagerError::K_PAGER_ERROR_CORRUPT);
    return rc;
  }
  // the journal file's structure is [magicnumber][totalsize][PageRecord]*n
  num_record = (journal_size - kAJournalMagic.size() - sizeof(PageNumber)) /
               sizeof(PageRecord);

  if (num_record == 0) {
    SqlitePagerPrivateUnWriteLock();
    err_mask_.insert(SqlitePagerError::K_PAGER_ERROR_CORRUPT);
    return rc;
  }

  /* Read the beginning of the journal and truncate the
  ** database file back to its original size.
  */
  rc = journal_fd_->OsRead(magic_buffer);
  if (rc != ResultCode::kOk || magic_buffer != kAJournalMagic) {
    rc = ResultCode::kProtocol;
    SqlitePagerPrivateUnWriteLock();
    err_mask_.insert(SqlitePagerError::K_PAGER_ERROR_CORRUPT);
    return rc;
  }
  rc = journal_fd_->OsRead(page_number_buffer);
  if (rc != ResultCode::kOk) {
    SqlitePagerPrivateUnWriteLock();
    err_mask_.insert(SqlitePagerError::K_PAGER_ERROR_CORRUPT);
    return rc;
  }

  // Copy the data from the buffer into the max_page variable
  std::memcpy(&max_page, page_number_buffer.data(), sizeof(PageNumber));

  // truncate the database file to the original size recorded in journal
  rc = fd_->OsTruncate(max_page * kPageSize);
  if (rc != ResultCode::kOk) {
    SqlitePagerPrivateUnWriteLock();
    err_mask_.insert(SqlitePagerError::K_PAGER_ERROR_CORRUPT);
    return rc;
  }

  num_database_size_ = max_page;

  /* Copy original pages out of the journal and back into the database file.
   */
  for (int idx = num_record - 1; idx >= 0; idx--) {
    rc = SqlitePagerPrivatePlaybackOnePage(journal_fd_.get());
    if (rc != ResultCode::kOk) {
      SqlitePagerPrivateUnWriteLock();
      err_mask_.insert(SqlitePagerError::K_PAGER_ERROR_CORRUPT);
      rc = ResultCode::kCorrupt;
      return rc;
    }
  }

  rc = SqlitePagerPrivateUnWriteLock();
  return rc;
}

// play back the checkpoint journal
ResultCode Pager::SqlitePagerPrivateCkptPlayback() {
  // TODO: implement this function
  return ResultCode::kError;
}

ResultCode Pager::SqlitePagerPrivateSyncAllPages() {
  // relevent to journal, so we will implement it later
  if (is_journal_need_sync_) {
    ResultCode rc = journal_fd_->OsSync();
    if (rc != ResultCode::kOk) {
      return rc;
    }
    is_journal_need_sync_ = false;
  }

  for (BasePage *cur_page = p_free_page_first_; cur_page != nullptr;
       cur_page = cur_page->p_header_->p_next_free_) {
    if (cur_page->p_header_->is_dirty_) {
      ResultCode rc =
          fd_->OsSeek((cur_page->p_header_->page_number_ - 1) * kPageSize);
      if (rc != ResultCode::kOk) {
        return rc;
      }
      rc = fd_->OsWrite(cur_page->ImageVector());
      if (rc != ResultCode::kOk) {
        return rc;
      }
      cur_page->p_header_->is_dirty_ = false;
    }
  }
  return ResultCode::kOk;
}

ResultCode Pager::SqlitePagerPrivateUnWriteLock() {
  // Add comments
  ResultCode rc = ResultCode::kOk;
  if (lock_state_ != SqliteLockState::K_SQLITE_WRITE_LOCK) return rc;
  SqlitePagerCkptCommit();
  if (is_checkpoint_journal_open_) {
    checkpoint_journal_fd_->OsClose();
    is_checkpoint_journal_open_ = false;
  }
  journal_fd_->OsClose();
  is_journal_open_ = false;
  journal_fd_->OsDelete();
  rc = fd_->OsUnlock();
  assert(rc == ResultCode::kOk);
  page_journal_bit_map_.clear();

  for (BasePage *page = p_all_page_first_; page;
       page = page->p_header_->p_next_all_) {
    page->p_header_->is_in_journal_ = false;
    page->p_header_->is_dirty_ = false;
  }

  lock_state_ = SqliteLockState::K_SQLITE_READ_LOCK;
  return rc;
}

/**
 * Checkpointing ensures that periodically, the current state of the database
 * pages is written to the checkpoint journal file.
 *
 * SqlitePagerCkptBegin() begins the checkpoint process.
 */
ResultCode Pager::SqlitePagerCkptBegin() {
  ResultCode rc;
  assert(is_journal_open_);
  assert(!is_checkpoint_journal_use_);
  page_checkpoint_journal_bit_map_ =
      boost::dynamic_bitset<>(num_database_size_ + kBitMapPlaceHolder);
  if (page_checkpoint_journal_bit_map_.empty()) {
    fd_->OsReadLock();
    rc = ResultCode::kNoMem;
    return rc;
  }
  rc = journal_fd_->OsFileSize(checkpoint_journal_size_);
  if (rc != ResultCode::kOk) {
    page_checkpoint_journal_bit_map_.resize(0);
    return rc;
  }

  checkpoint_size_ = num_database_size_;
  if (!is_checkpoint_journal_open_) {
    checkpoint_journal_fd_ = std::make_unique<OsFile>();
    rc = checkpoint_journal_fd_->OsOpenReadWrite(checkpoint_journal_file_name_,
                                                 is_read_only_);

    if (rc != ResultCode::kOk) {
      if (!page_checkpoint_journal_bit_map_.empty()) {
        page_checkpoint_journal_bit_map_.resize(0);
      }
      return rc;
    }
    is_checkpoint_journal_open_ = true;
  }
  is_checkpoint_journal_use_ = true;
  rc = ResultCode::kOk;
  return rc;
}

/**
 * Checkpointing ensures that periodically, the current state of the database
 * pages is written to the checkpoint journal file.
 *
 * SqlitePagerCkptCommit() commits the checkpoint.
 */
ResultCode Pager::SqlitePagerCkptCommit() {
  ResultCode rc;
  if (is_checkpoint_journal_use_) {
    checkpoint_journal_fd_->OsTruncate(0);
    is_checkpoint_journal_use_ = false;
    page_checkpoint_journal_bit_map_.resize(kBitMapPlaceHolder + 0);
    BasePage *cur_page;
    for (cur_page = p_all_page_first_; cur_page != nullptr;
         cur_page = cur_page->p_header_->p_next_all_) {
      cur_page->p_header_->is_in_checkpoint_ = false;
    }
  }
  return ResultCode::kOk;
}

ResultCode Pager::SqlitePagerCkptRollback() {
  ResultCode rc;
  if (is_checkpoint_journal_use_) {
    rc = SqlitePagerPrivateCkptPlayback();
    SqlitePagerCkptCommit();
  } else {
    rc = ResultCode::kOk;
  }
  return rc;
}

ResultCode Pager::SqlitePagerPrivatePlaybackOnePage(OsFile *fd) {
  PageRecord page_record = PageRecord();
  std::vector<std::byte> buffer(sizeof(PageRecord));
  ResultCode rc = fd->OsRead(buffer);
  if (rc != ResultCode::kOk) {
    return rc;
  }

  std::memcpy(&page_record, buffer.data(), sizeof(PageRecord));

  /* Sanity checking on the page */
  if (page_record.page_number_ > num_database_size_ ||
      page_record.page_number_ == 0)
    return ResultCode::kCorrupt;

  /* Playback the page.  Update the in-memory copy of the page
   ** at the same time, if there is one.
   */

  // revise memory
  BasePage *current_page =
      SqlitePagerPrivateCacheLookup(page_record.page_number_);
  if (current_page) {
    // the old one would be automatically deleted
    current_page->p_image_ = std::make_unique<std::array<std::byte, kPageSize>>(
        page_record.p_image_);
    // TODO: how to accomplish this line is debatable
    // memset(PGHDR_TO_EXTRA(current_page), 0, pPager->nExtra);
  }

  // revise disk
  rc = fd->OsSeek((page_record.page_number_ - 1) * kPageSize);
  if (rc == ResultCode::kOk) {
    rc = fd->OsWrite(page_record.ImageVector());
  }

  return rc;
}
