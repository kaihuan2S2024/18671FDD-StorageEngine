#include "btree.h"

// --------------------- BtCursor Private Functions ---------------------

/*
 * Copies the contents of the cursor into a temporary cursor.
 * It also increases the ref count of the page that the cursor is pointing to.
 */
void Btree::GetTempCursor(BtCursor &cursor, BtCursor &temp_cursor) {
  temp_cursor = cursor;
  if (temp_cursor.p_page) {
    pager_->SqlitePagerRef(temp_cursor.p_page);
  }
}

/*
 * Decreases the ref count of the page that the cursor is pointing to.
 */
void Btree::ReleaseTempCursor(BtCursor &temp_cursor) {
  if (temp_cursor.p_page) {
    pager_->SqlitePagerUnref(temp_cursor.p_page);
  }
}

ResultCode Btree::GetPayload(const BtCursor &cursor, u32 offset, u32 amount, std::vector<std::byte> &result) {
  if (!cursor.p_page || cursor.cell_index >= cursor.p_page->GetNumCells()) {
    return ResultCode::kError;
  }
  ResultCode rc;
  PageNumber next_page_number = cursor.p_page->GetCellHeaderByteView(cursor.cell_index).overflow_page;
  ImageIndex cell_start_idx = cursor.p_page->cell_trackers_[cursor.cell_index].image_idx;

  if (next_page_number == 0) {
    u32 a = amount;
    result.resize(a);
    u32 total_offset = cell_start_idx + sizeof(CellHeaderByteView) + offset;
    memcpy(result.data(),
           cursor.p_page->p_image_->data() + total_offset, a);
    if (a == amount) {
      return ResultCode::kOk;
    } else {
      return ResultCode::kError;
    }
  }
  while (amount > 0 && next_page_number != 0) {
    BasePage *p_base_page = nullptr;
    rc = pager_->SqlitePagerGet(next_page_number, &p_base_page,
                                NodePage::CreateDerivedPage);
    if (rc != ResultCode::kOk) { return rc; }
    auto *p_overflow_page = dynamic_cast<OverFreePage *>(p_base_page);
    next_page_number = p_overflow_page->GetOverflowPageHeaderByteView().next_page;
    if (offset < kOverflowSize) {
      u32 a = amount;
      if (a + offset > kOverflowSize) {
        a = kOverflowSize - offset;
      }
      result.resize(result.size() + a);
      memcpy(result.data() + result.size() - a,
             p_overflow_page->p_image_->data() + sizeof(OverflowPageHeaderByteView) + offset, a);
      offset = 0;
      amount -= a;
    } else {
      offset -= kOverflowSize;
    }
    pager_->SqlitePagerUnref(p_base_page);
  }
  if (amount > 0) {
    return ResultCode::kCorrupt;
  }
  return ResultCode::kOk;
}

/*
 * Moves the cursor to point to its child page, as indicated by the child_page_number.
 */
ResultCode Btree::MoveToChild(BtCursor &cursor, PageNumber child_page_number) {
  ResultCode rc;
  BasePage *p_base_page = nullptr;
  rc = pager_->SqlitePagerGet(child_page_number, &p_base_page,
                              NodePage::CreateDerivedPage);
  if (rc != ResultCode::kOk) { return rc; }
  auto p_node_page = dynamic_cast<NodePage *>(p_base_page);
  rc = InitPage(*p_node_page, cursor.p_page);
  if (rc != ResultCode::kOk) { return rc; }
  pager_->SqlitePagerUnref(cursor.p_page);
  cursor.p_page = p_node_page;
  cursor.cell_index = 0;
  return ResultCode::kOk;
}

/*
 * Moves the cursor to point to its parent page, as indicated by the cursor.p_page->p_parent_.
 */
ResultCode Btree::MoveToParent(BtCursor &cursor) {
  PageNumber old_page_number = pager_->SqlitePagerPageNumber(cursor.p_page);
  NodePage *p_parent = cursor.p_page->p_parent_;
  if (!p_parent) { return ResultCode::kInternal; }
  pager_->SqlitePagerRef(p_parent);
  pager_->SqlitePagerUnref(cursor.p_page);
  cursor.p_page = p_parent;
  cursor.cell_index = p_parent->GetNumCells();
  for (size_t i = 0; i < p_parent->GetNumCells(); ++i) {
    CellHeaderByteView cell_header = p_parent->GetCellHeaderByteView(i);
    if (cell_header.left_child == old_page_number) {
      cursor.cell_index = i;
      break;
    }
  }
  return ResultCode::kOk;
}

ResultCode Btree::MoveToRoot(BtCursor &cursor) {
  BasePage *p_base_page = nullptr;
  ResultCode rc;
  rc = pager_->SqlitePagerGet(cursor.root_page_number, &p_base_page,
                              NodePage::CreateDerivedPage);
  if (rc != ResultCode::kOk) { return rc; }
  auto p_node_page = dynamic_cast<NodePage *>(p_base_page);
  rc = InitPage(*p_node_page, nullptr);
  if (rc != ResultCode::kOk) { return rc; }
  pager_->SqlitePagerUnref(p_base_page);
  cursor.p_page = p_node_page;
  cursor.cell_index = 0;
  return ResultCode::kOk;
}

ResultCode Btree::MoveToLeftmost(BtCursor &cursor) {
  ResultCode rc;
  PageNumber left_child;
  CellHeaderByteView cell_header = cursor.p_page->GetCellHeaderByteView(cursor.cell_index);
  left_child = cell_header.left_child;
  while (left_child != 0) {
    rc = MoveToChild(cursor, left_child);
    if (rc != ResultCode::kOk) { return rc; }
    cell_header = cursor.p_page->GetCellHeaderByteView(cursor.cell_index);
    left_child = cell_header.left_child;
  }
  return ResultCode::kOk;
}


