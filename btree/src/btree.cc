#include "btree.h"

Btree *Btree::instance_ = nullptr;

Btree &Btree::GetInstance(const std::string &filename) {
  if (instance_ == nullptr) {
    instance_ = new Btree(filename);
  }
  return *instance_;
}

Btree &Btree::GetInstance() {
  if (instance_ == nullptr) {
    throw std::runtime_error(
        "Btree instance has not been created yet. Please create it with "
        "GetInstance(filename, cache_size)");
  }
  return *instance_;
}

Btree::Btree(const std::string &filename)
    : filename_(filename),
      has_writable_bt_cursor_(false),
      pager_(std::make_unique<Pager>(filename_, kDefaultCacheSize)),
      read_only_(pager_->SqlitePagerIsReadOnly()),
      in_trans_(false),
      in_ckpt_(false),
      p_first_page_(nullptr) {}

Btree &Btree::RebuildInstance(const std::string &filename) {
  if (instance_ != nullptr) {
    delete instance_;
  }
  instance_ = new Btree(filename);
  return *instance_;
}

Btree::Btree(std::string filename, int cache_size)
    : filename_(filename),
      has_writable_bt_cursor_(false),
      pager_(std::make_unique<Pager>(filename_,
                                     cache_size < 10 ? 10 : cache_size)),
      read_only_(pager_->SqlitePagerIsReadOnly()),
      in_trans_(false),
      in_ckpt_(false),
      p_first_page_(nullptr) {}

// --------------------- Btree Private Functions ---------------------

ResultCode Btree::NewDatabase() {
  // Step 1: Skip the function if the database is not empty
  PageNumber cache_page_count = pager_->SqlitePagerPageCount();
  if (cache_page_count > 1) {
    return ResultCode::kOk;
  }

  // Attempt to get FirstPage (Page 1)
  BasePage *p_base_page = nullptr;
  ResultCode rc;
  rc = pager_->SqlitePagerGet(1, &p_base_page, FirstPage::CreateDerivedPage);
  if (rc != ResultCode::kOk) {
    return rc;
  }
  rc = pager_->SqlitePagerWrite(p_base_page);
  if (rc != ResultCode::kOk) {
    return rc;
  }
  p_first_page_ = dynamic_cast<FirstPage *>(p_base_page);

  // Step 3: Attempt to get Page 2 (Used by VDBE for certain operations)
  NodePage *p_root_page;
  rc = pager_->SqlitePagerGet(2, &p_base_page, NodePage::CreateDerivedPage);
  if (rc != ResultCode::kOk) {
    return rc;
  }
  rc = pager_->SqlitePagerWrite(p_base_page);
  if (rc != ResultCode::kOk) {
    pager_->SqlitePagerUnref(p_base_page);
    return rc;
  }
  p_root_page = dynamic_cast<NodePage *>(p_base_page);

  // Step 4: Initialize FirstPage and RootPage, and then unref RootPage
  p_first_page_->SetDefaultByteView();
  p_root_page->ZeroPage();
  rc = pager_->SqlitePagerUnref(p_root_page);
  return rc;
}

ResultCode Btree::LockBtree() {
  if (p_first_page_ != nullptr) {
    return ResultCode::kOk;
  }
  ResultCode rc;
  BasePage *p_base_page = nullptr;
  rc = pager_->SqlitePagerGet(1, &p_base_page, FirstPage::CreateDerivedPage);
  if (rc != ResultCode::kOk || p_base_page == nullptr) {
    return rc;
  }
  p_first_page_ = dynamic_cast<FirstPage *>(p_base_page);

  if (pager_->SqlitePagerPageCount() > 0 &&
      !p_first_page_->HasCorrectMagicInt()) {
    rc = ResultCode::kCorrupt;
    pager_->SqlitePagerUnref(p_base_page);
    p_first_page_ = nullptr;
  }
  return rc;
}

ResultCode Btree::UnlockBtreeIfUnused() {
  if (!in_trans_ && bt_cursor_set_.empty() && !p_first_page_) {
    pager_->SqlitePagerUnref(p_first_page_);
    p_first_page_ = nullptr;
    in_trans_ = false;
    in_ckpt_ = false;
  }
  return ResultCode::kOk;
}

ResultCode Btree::InitPage(NodePage &node_page, NodePage *p_parent) {
  if (node_page.p_parent_) {
    if (node_page.p_parent_ != p_parent) {
      return ResultCode::kError;
    }
    return ResultCode::kOk;
  }
  if (p_parent) {
    node_page.p_parent_ = p_parent;
    pager_->SqlitePagerRef(p_parent);
  }
  if (node_page.is_init_) {
    return ResultCode::kOk;
  }
  node_page.is_init_ = true;
  node_page.cell_trackers_.clear();
  NodePageHeaderByteView node_page_header =
      node_page.GetNodePageHeaderByteView();
  ImageIndex iterator_idx = node_page_header.first_cell_idx;
  CellHeaderByteView cell_header{};
  u16 free_space = kUsableSpace;
  while (iterator_idx != 0) {
    if (iterator_idx > kPageSize - kMinCellSize ||
        iterator_idx < sizeof(NodePageHeaderByteView)) {
      return ResultCode::kCorrupt;
    }
    cell_header = node_page.GetCellHeaderByteViewByImageIndex(iterator_idx);
    u16 cell_size = cell_header.GetCellSize();
    if (iterator_idx + cell_size > kPageSize) {
      return ResultCode::kCorrupt;
    }
    free_space -= cell_size;
    node_page.SetCellHeaderByteViewByImageIndex(iterator_idx, cell_header);
    CellTracker tracker;
    tracker.image_idx = iterator_idx;
    node_page.cell_trackers_.push_back(tracker);
    iterator_idx = cell_header.next_cell_start_idx;
  }
  node_page.num_free_bytes_ = 0;
  iterator_idx = node_page_header.first_free_block_idx;
  FreeBlockByteView free_block{};
  while (iterator_idx != 0) {
    if (iterator_idx > kPageSize - sizeof(FreeBlockByteView) ||
        iterator_idx < sizeof(NodePageHeaderByteView)) {
      return ResultCode::kCorrupt;
    }
    free_block = node_page.GetFreeBlockByteView(iterator_idx);
    node_page.num_free_bytes_ += free_block.size;
    u16 &next_block_idx = free_block.next_block_idx;
    if (next_block_idx > 0 && next_block_idx < iterator_idx) {
      return ResultCode::kCorrupt;
    }
    iterator_idx = next_block_idx;
  }
  if (node_page.cell_trackers_.empty() && node_page.num_free_bytes_ == 0) {
    return ResultCode::kOk;
  }
  if (node_page.num_free_bytes_ != free_space) {
    return ResultCode::kCorrupt;
  }
  return ResultCode::kOk;
}

ResultCode Btree::AllocatePage(NodePage *&p_node_page,
                               PageNumber &page_number) {
  if (!p_first_page_) {
    return ResultCode::kError;
  }
  ResultCode rc = ResultCode::kOk;
  BasePage *p_base_page = nullptr;
  if (p_first_page_->GetFirstPageByteView().first_free_page != 0) {
    NodePage *p_overflow_page = nullptr;
    rc = pager_->SqlitePagerWrite(p_first_page_);
    if (rc != ResultCode::kOk) {
      return rc;
    }

    p_first_page_->DecrementNumFreePages();
    rc = pager_->SqlitePagerGet(
        p_first_page_->GetFirstPageByteView().first_free_page, &p_base_page,
        NodePage::CreateDerivedPage);
    if (rc != ResultCode::kOk) {
      return rc;
    }

    rc = pager_->SqlitePagerWrite(p_base_page);
    if (rc != ResultCode::kOk) {
      pager_->SqlitePagerUnref(p_base_page);
      return rc;
    }

    p_overflow_page = dynamic_cast<NodePage *>(p_base_page);
    u16 num_free_list_pages = p_overflow_page->GetNumberOfFreeListPages();
    if (num_free_list_pages == 0) {
      page_number = p_first_page_->GetFirstPageByteView().first_free_page;
      FirstPageByteView first_page_byte_view =
          p_first_page_->GetFirstPageByteView();
      first_page_byte_view.first_free_page =
          p_overflow_page->GetOverflowPageHeaderByteView().next_page;
      p_first_page_->SetFirstPageByteView(first_page_byte_view);
      p_node_page = dynamic_cast<NodePage *>(p_base_page);
    } else {
      page_number = p_overflow_page->GetFinalFreeListInfoPageNumber();
      p_overflow_page->DecrementFreeListNumPages();
      rc = pager_->SqlitePagerGet(page_number, &p_base_page,
                                  NodePage::CreateDerivedPage);
      pager_->SqlitePagerUnref(p_overflow_page);
      if (rc == ResultCode::kOk) {
        rc = pager_->SqlitePagerWrite(p_base_page);
      }
      p_node_page = dynamic_cast<NodePage *>(p_base_page);
    }
  } else {
    page_number = pager_->SqlitePagerPageCount() + 1;
    rc = pager_->SqlitePagerGet(page_number, &p_base_page,
                                NodePage::CreateDerivedPage);
    if (rc != ResultCode::kOk) {
      return rc;
    }
    rc = pager_->SqlitePagerWrite(p_base_page);
    if (p_base_page) {
      p_node_page = dynamic_cast<NodePage *>(p_base_page);
    }
  }
  return rc;
}

/**
 * FreePage
 *
 * Free a page and add it to the free list
 */
ResultCode Btree::FreePage(BasePage *&p_input_base_page,
                           PageNumber &page_number, bool is_overflow_page) {
  bool need_unref = false;
  ResultCode rc;
  BasePage *p_base_page = p_input_base_page;
  NodePage *p_overflow_page = nullptr;
  NodePage *p_parent = nullptr;
  if (page_number == 0) {
    if (!p_first_page_) {
      return ResultCode::kOk;
    }
    if (!is_overflow_page) {
      p_parent = dynamic_cast<NodePage *>(p_base_page)->p_parent_;
    }
    p_overflow_page = dynamic_cast<NodePage *>(p_base_page);
    page_number = pager_->SqlitePagerPageNumber(p_base_page);
  }
  if (page_number <= 2) {
    return ResultCode::kError;
  }
  rc = pager_->SqlitePagerWrite(p_first_page_);
  if (rc != ResultCode::kOk) {
    return rc;
  }

  p_first_page_->IncrementNumFreePages();
  FirstPageByteView first_page = p_first_page_->GetFirstPageByteView();
  if (first_page.num_free_pages > 0 && first_page.first_free_page > 0) {
    rc = pager_->SqlitePagerGet(first_page.first_free_page, &p_base_page,
                                NodePage::CreateDerivedPage);
    if (rc == ResultCode::kOk) {
      p_overflow_page = dynamic_cast<NodePage *>(p_base_page);
      bool can_insert = p_overflow_page->CanInsertPageNumber();
      if (can_insert) {
        rc = pager_->SqlitePagerWrite(p_base_page);
        if (rc == ResultCode::kOk) {
          p_overflow_page->InsertPageNumber(page_number);
          pager_->SqlitePagerUnref(p_base_page);
          pager_->SqlitePagerDontWrite(page_number);
          return rc;
        }
      } else {
        pager_->SqlitePagerUnref(p_base_page);
      }
    }
  }
  if (p_overflow_page == nullptr) {
    if (page_number == 0) {
      return ResultCode::kError;
    }
    rc = pager_->SqlitePagerGet(page_number, &p_base_page,
                                NodePage::CreateDerivedPage);
    if (rc != ResultCode::kOk) {
      return rc;
    }
    p_overflow_page = dynamic_cast<NodePage *>(p_base_page);
    need_unref = true;
  }
  rc = pager_->SqlitePagerWrite(p_base_page);
  if (rc != ResultCode::kOk) {
    if (need_unref) {
      pager_->SqlitePagerUnref(p_base_page);
    }
    return rc;
  }

  OverflowPageHeaderByteView overflow_page_header =
      p_overflow_page->GetOverflowPageHeaderByteView();
  overflow_page_header.next_page = first_page.first_free_page;
  p_overflow_page->SetOverflowPageHeaderByteView(overflow_page_header);

  first_page.first_free_page = page_number;
  p_first_page_->SetFirstPageByteView(first_page);
  std::memset(
      p_overflow_page->p_image_->data() + sizeof(OverflowPageHeaderByteView), 0,
      kOverflowSize);
  if (p_parent) {
    pager_->SqlitePagerUnref(p_parent);
  }
  if (need_unref) {
    rc = pager_->SqlitePagerUnref(p_base_page);
  }
  return rc;
}

/*
 * Erase the data out the overflow pages of a cell
 */
ResultCode Btree::ClearCell(NodePage &node_page, u16 cell_idx) {
  // Step 1: Check if the cell has overflow pages
  // If not, return because we don't need to clear anything
  CellHeaderByteView cell_header = node_page.GetCellHeaderByteView(cell_idx);
  if (cell_header.overflow_page == 0 &&
      cell_header.key_size + cell_header.data_size <= kMaxLocalPayload) {
    return ResultCode::kOk;
  }

  // Step 2: Clear the overflow pages and add them to the free list
  PageNumber overflow_page_number = cell_header.overflow_page;
  BasePage *p_base_page = nullptr;
  NodePage *p_overflow_page = nullptr;
  ResultCode rc;
  while (overflow_page_number != 0) {
    rc = pager_->SqlitePagerGet(overflow_page_number, &p_base_page,
                                NodePage::CreateDerivedPage);
    if (rc != ResultCode::kOk) {
      return rc;
    }
    p_overflow_page = dynamic_cast<NodePage *>(p_base_page);
    PageNumber next_overflow_page_number =
        p_overflow_page->GetOverflowPageHeaderByteView().next_page;
    rc = FreePage(p_base_page, overflow_page_number, true);
    if (rc != ResultCode::kOk) {
      return rc;
    }
    if (p_base_page) {
      pager_->SqlitePagerUnref(p_base_page);
    }
    if (overflow_page_number == next_overflow_page_number) {
      return ResultCode::kCorrupt;
    }
    overflow_page_number = next_overflow_page_number;
  }

  return ResultCode::kOk;
}

/*
 *  FillInCell
 *
 *  Fill the payload inside an overflow page if needed.
 */
ResultCode Btree::FillInCell(Cell &cell_in) {
  // Step 1: Check if the payload is small enough to fit in a single page
  // If true, return because we don't need to allocate space in overflow pages
  if (!cell_in.NeedOverflowPage()) {
    return ResultCode::kOk;
  }
  ResultCode rc;
  BasePage *p_base_page = nullptr;
  NodePage *p_node_page = nullptr;
  BasePage *p_prior_page = nullptr;
  NodePage *p_overflow_page = nullptr;
  u32 payload_copy_start_offset = 0;
  PageNumber &next_overflow_page_number = cell_in.cell_header_.overflow_page;

  // Step 2: Keep allocating overflow pages until the payload is fully copied
  while (payload_copy_start_offset < cell_in.GetPayloadSize()) {
    rc = AllocatePage(p_node_page, next_overflow_page_number);
    if (rc != ResultCode::kOk) {
      return rc;
    }
    // Cast p_node_page to OverFreePage
    p_overflow_page = dynamic_cast<NodePage *>(p_node_page);
    if (p_prior_page) {
      pager_->SqlitePagerUnref(p_prior_page);
    }
    p_prior_page = p_overflow_page;

    u32 size_to_copy;
    if (payload_copy_start_offset + kOverflowSize > cell_in.GetPayloadSize()) {
      size_to_copy = cell_in.GetPayloadSize() - payload_copy_start_offset;
    } else {
      size_to_copy = kOverflowSize;
    }
    std::memcpy(
        p_overflow_page->p_image_->data() + sizeof(OverflowPageHeaderByteView),
        cell_in.payload_.data() + payload_copy_start_offset, size_to_copy);
    payload_copy_start_offset += size_to_copy;
  }

  cell_in.payload_.clear();

  if (p_prior_page) {
    pager_->SqlitePagerUnref(p_prior_page);
  }
  return ResultCode::kOk;
}

void Btree::ReParentPage(PageNumber page_number, NodePage *p_new_parent) {
  if (page_number == 0) {
    return;
  }
  NodePage *p_node_page = nullptr;
  BasePage *p_base_page = nullptr;
  pager_->SqlitePagerLookup(page_number, &p_base_page);
  if (p_base_page == nullptr) {
    return;
  }
  p_node_page = dynamic_cast<NodePage *>(p_base_page);
  if (!p_node_page->is_init_ || p_node_page->p_parent_ == p_new_parent) {
    return;
  }

  if (p_node_page->p_parent_) {
    pager_->SqlitePagerUnref(p_node_page->p_parent_);
  }
  p_node_page->p_parent_ = p_new_parent;
  if (p_new_parent) {
    pager_->SqlitePagerRef(p_new_parent);
  }
  pager_->SqlitePagerUnref(p_base_page);
}

void Btree::ReParentChildPages(NodePage &node_page) {
  for (u16 cell_idx = 0; cell_idx < node_page.cell_trackers_.size();
       ++cell_idx) {
    CellHeaderByteView cell_header = node_page.GetCellHeaderByteView(cell_idx);
    ReParentPage(cell_header.left_child, &node_page);
  }
  ReParentPage(node_page.GetNodePageHeaderByteView().right_child, &node_page);
}

ResultCode Btree::ClearDatabasePage(PageNumber page_number, bool free_page) {
  BasePage *p_base_page = nullptr;
  NodePage *p_node_page = nullptr;
  ResultCode rc;
  rc = pager_->SqlitePagerGet(page_number, &p_base_page,
                              NodePage::CreateDerivedPage);
  if (rc != ResultCode::kOk) {
    return rc;
  }
  rc = pager_->SqlitePagerWrite(p_base_page);
  if (rc != ResultCode::kOk) {
    return rc;
  }
  p_node_page = dynamic_cast<NodePage *>(p_base_page);
  NodePageHeaderByteView node_page_header =
      p_node_page->GetNodePageHeaderByteView();
  for (u16 cell_idx = 0; cell_idx < p_node_page->cell_trackers_.size();
       ++cell_idx) {
    CellHeaderByteView cell_header =
        p_node_page->GetCellHeaderByteView(cell_idx);
    if (cell_header.left_child != 0) {
      rc = ClearDatabasePage(cell_header.left_child, true);
      if (rc != ResultCode::kOk) {
        return rc;
      }
    }
    rc = ClearCell(*p_node_page, cell_idx);
    if (rc != ResultCode::kOk) {
      return rc;
    }
  }
  if (node_page_header.right_child != 0) {
    rc = ClearDatabasePage(node_page_header.right_child, true);
    if (rc != ResultCode::kOk) {
      return rc;
    }
  }
  if (free_page) {
    rc = FreePage(p_base_page, page_number, false);
  } else {
    p_node_page->ZeroPage();
  }
  rc = pager_->SqlitePagerUnref(p_base_page);
  return rc;
}

// --------------------- Btree Public Functions ---------------------

ResultCode Btree::BtreeSetCacheSize(int cache_size) {
  pager_->SqlitePagerSetCachesize(cache_size);
  return ResultCode::kOk;
}

/*
 * Starts a new transaction
 *
 * These operations will fail if you are not in a transaction
 *
 *  BtreeCreateTable()
 *  BtreeCreateIndex()
 *  BtreeClearTable()
 *  BtreeDropTable()
 *  BtreeInsert()
 *  BtreeDelete()
 *  BtreeUpdateMeta()
 */
ResultCode Btree::BtreeBeginTrans() {
  ResultCode rc;
  if (in_trans_) {
    return ResultCode::kError;
  }
  if (!p_first_page_) {
    rc = LockBtree();
    if (rc != ResultCode::kOk) {
      return rc;
    }
  }
  if (read_only_) {
    rc = ResultCode::kOk;
  } else {
    rc = pager_->SqlitePagerBegin(p_first_page_);
    if (rc == ResultCode::kOk) {
      rc = NewDatabase();
    }
  }
  if (rc == ResultCode::kOk) {
    in_trans_ = true;
    in_ckpt_ = false;
  } else {
    UnlockBtreeIfUnused();
  }
  return rc;
}

ResultCode Btree::BtreeCommit() {
  ResultCode rc;
  if (!in_trans_) {
    return ResultCode::kError;
  }
  rc = read_only_ ? ResultCode::kOk : pager_->SqlitePagerCommit();
  in_trans_ = false;
  in_ckpt_ = false;
  return rc;
}

ResultCode Btree::BtreeRollback() {
  ResultCode rc;
  if (!in_trans_) {
    return ResultCode::kOk;
  }
  in_trans_ = false;
  in_ckpt_ = false;
  for (auto &bt_cursor : bt_cursor_set_) {
    if (bt_cursor->p_page) {
      pager_->SqlitePagerUnref(bt_cursor->p_page);
      bt_cursor->p_page = nullptr;
    }
  }
  rc = read_only_ ? ResultCode::kOk : pager_->SqlitePagerRollback();
  UnlockBtreeIfUnused();
  return rc;
}

ResultCode Btree::BtreeBeginCkpt() {
  if (!in_trans_ || in_ckpt_) {
    return ResultCode::kError;
  }
  ResultCode rc;
  if (read_only_) {
    rc = ResultCode::kOk;
  } else {
    rc = pager_->SqlitePagerCkptBegin();
  }
  in_ckpt_ = true;
  return rc;
}

ResultCode Btree::BtreeCommitCkpt() {
  ResultCode rc;
  if (in_ckpt_ && !read_only_) {
    rc = pager_->SqlitePagerCkptCommit();
  } else {
    rc = ResultCode::kOk;
  }
  in_ckpt_ = false;
  return rc;
}

ResultCode Btree::BtreeRollbackCkpt() {
  if (!in_ckpt_ || read_only_) {
    return ResultCode::kOk;
  }
  ResultCode rc;
  for (auto &bt_cursor : bt_cursor_set_) {
    if (bt_cursor->p_page) {
      pager_->SqlitePagerUnref(bt_cursor->p_page);
      bt_cursor->p_page = nullptr;
    }
  }
  rc = pager_->SqlitePagerCkptRollback();
  in_ckpt_ = false;
  return rc;
}

ResultCode Btree::BtreeCreateTable(PageNumber &root_page_number) {
  if (!in_trans_) {
    return ResultCode::kError;
  }
  if (read_only_) {
    return ResultCode::kReadOnly;
  }
  ResultCode rc;
  PageNumber page_number;
  NodePage *p_node_page = nullptr;
  rc = AllocatePage(p_node_page, page_number);
  if (rc != ResultCode::kOk) {
    return rc;
  }
  if (!pager_->SqlitePagerIsWritable(p_node_page)) {
    return ResultCode::kError;
  }
  p_node_page->ZeroPage();
  pager_->SqlitePagerUnref(p_node_page);
  root_page_number = page_number;
  return ResultCode::kOk;
}

ResultCode Btree::BtreeCreateIndex(PageNumber &root_page_number) {
  return BtreeCreateTable(root_page_number);
}

ResultCode Btree::BtreeClearTable(PageNumber root_page_number) {
  if (!in_trans_) {
    return ResultCode::kError;
  }
  if (read_only_) {
    return ResultCode::kReadOnly;
  }
  int num_locks = 0;
  auto it = (lock_count_map_.find(root_page_number));
  if (it != lock_count_map_.end()) {
    num_locks = it->second;
  }
  if (num_locks != 0) {
    return ResultCode::kLocked;
  }
  ResultCode rc;
  rc = ClearDatabasePage(root_page_number, false);
  if (rc != ResultCode::kOk) {
    BtreeRollback();
  }
  return rc;
}

ResultCode Btree::BtreeDropTable(PageNumber root_page_number) {
  if (!in_trans_) {
    return ResultCode::kError;
  }
  if (read_only_) {
    return ResultCode::kReadOnly;
  }
  ResultCode rc;
  BasePage *p_base_page = nullptr;
  rc = pager_->SqlitePagerGet(root_page_number, &p_base_page,
                              NodePage::CreateDerivedPage);
  if (rc != ResultCode::kOk) {
    return rc;
  }
  rc = BtreeClearTable(root_page_number);
  if (rc != ResultCode::kOk) {
    return rc;
  }
  if (root_page_number > 2) {
    rc = FreePage(p_base_page, root_page_number, false);
  } else {
    auto *p_node_page = dynamic_cast<NodePage *>(p_base_page);
    p_node_page->ZeroPage();
  }
  rc = pager_->SqlitePagerUnref(p_base_page);
  return rc;
}

u32 Btree::BtreePageCount() { return pager_->SqlitePagerPageCount(); }

ResultCode Btree::BtreeGetMeta(
    std::array<int, kMetaIntArraySize> &meta_int_arr) {
  BasePage *p_base_page = nullptr;
  ResultCode rc;
  rc = pager_->SqlitePagerGet(1, &p_base_page, FirstPage::CreateDerivedPage);
  if (rc != ResultCode::kOk) {
    return rc;
  }
  auto *p_first_page = dynamic_cast<FirstPage *>(p_base_page);
  p_first_page->GetMeta(meta_int_arr);
  rc = pager_->SqlitePagerUnref(p_base_page);
  return rc;
}

ResultCode Btree::BtreeUpdateMeta(
    std::array<int, kMetaIntArraySize> &meta_int_arr) {
  if (!in_trans_) {
    return ResultCode::kError;
  }
  if (read_only_) {
    return ResultCode::kReadOnly;
  }
  BasePage *p_base_page = nullptr;
  ResultCode rc;
  rc = pager_->SqlitePagerGet(1, &p_base_page, FirstPage::CreateDerivedPage);
  if (rc != ResultCode::kOk) {
    return rc;
  }
  auto *p_first_page = dynamic_cast<FirstPage *>(p_base_page);
  p_first_page->UpdateMeta(meta_int_arr);
  return ResultCode::kOk;
}

BtreeAccessor::BtreeAccessor(const std::string &filename) : btree(filename) {}

Btree &BtreeAccessor::GetBtree() { return btree; }