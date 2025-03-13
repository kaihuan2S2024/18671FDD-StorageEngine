
#include "node_page.h"

/**
 * Default constructor
 */
Cell::Cell() : cell_header_({0, 0, 0, 0, 0}) {}

/**
 * Constructor with key and data parameters.
 */
Cell::Cell(const std::vector<std::byte> &key_in,
           const std::vector<std::byte> &data_in)
    : cell_header_({0, static_cast<u32>(key_in.size()),
                    static_cast<u32>(data_in.size()), 0, 0}) {
  payload_.reserve(cell_header_.key_size + cell_header_.data_size);
  payload_.insert(payload_.end(), key_in.begin(), key_in.end());
  payload_.insert(payload_.end(), data_in.begin(), data_in.end());
}

/**
 * Constructor with CellHeaderByteView and payload parameters
 */
Cell::Cell(const CellHeaderByteView &cell_header_in,
           const std::vector<std::byte> &payload_in)
    : cell_header_(cell_header_in) {
  payload_.reserve(payload_in.size());
  payload_.insert(payload_.end(), payload_in.begin(), payload_in.end());
}

/**
 * It returns the size of the Payload
 */
u32 Cell::GetPayloadSize() { return payload_.size(); }

/**
 * It returns the size of a Cell.
 *
 * Calculates the size of the cell in bytes for the current payload
 */
u32 Cell::GetCellSize() const {
  if (NeedOverflowPage()) {
    return sizeof(CellHeaderByteView);
  }
  return sizeof(CellHeaderByteView) + cell_header_.key_size +
         cell_header_.data_size;
}

u32 CellHeaderByteView::GetCellSize() const {
  if (key_size + data_size > kMaxLocalPayload) {
    return sizeof(CellHeaderByteView);
  }
  return sizeof(CellHeaderByteView) + key_size + data_size;
}

bool Cell::NeedOverflowPage() const {
  return cell_header_.key_size + cell_header_.data_size > kMaxLocalPayload;
}

CellTracker::CellTracker() : image_idx(0), cell(Cell()) {}

bool CellTracker::IsCellWrittenIntoImage() const { return image_idx != 0; }

/**
 * Default constructor
 */
NodePage::NodePage()
    : is_init_(false),
      p_parent_(nullptr),
      num_free_bytes_(0),
      is_overfull_(false) {}

/**
 * Function called by pager to initialize the page
 */
std::unique_ptr<BasePage> NodePage::CreateDerivedPage() {
  return std::make_unique<NodePage>();
}

/**
 * Returns the NodePageHeaderByteView stored at index 0 of p_image_
 */
NodePageHeaderByteView NodePage::GetNodePageHeaderByteView() const {
  NodePageHeaderByteView node_page_header{};
  std::memcpy(&node_page_header, p_image_->data(),
              sizeof(NodePageHeaderByteView));
  return node_page_header;
}

/**
 * Sets the NodePageHeaderByteView stored at index 0 of p_image_
 */
void NodePage::SetNodePageHeaderByteView(
    NodePageHeaderByteView &node_page_header_byte_view_in) {
  std::memcpy(p_image_->data(), &node_page_header_byte_view_in,
              sizeof(NodePageHeaderByteView));
}

/**
 * Returns a FreeBlockByteView stored at the specified index in p_image_
 */
void NodePage::SetFreeBlockByteView(
    ImageIndex start_idx, FreeBlockByteView &free_block_byte_view_in) {
  std::memcpy(p_image_->data() + start_idx, &free_block_byte_view_in,
              sizeof(FreeBlockByteView));
}

/**
 * Sets a FreeBlockByteView stored at the specified index
 */
FreeBlockByteView NodePage::GetFreeBlockByteView(ImageIndex start_idx) const {
  FreeBlockByteView free_block_byte_view{};
  std::memcpy(&free_block_byte_view, p_image_->data() + start_idx,
              sizeof(FreeBlockByteView));
  return free_block_byte_view;
}

/**
 * Returns the number of cells tracked with cell_header_indexes_
 */
u32 NodePage::GetNumCells() { return cell_trackers_.size(); }

/**
 * Returns the CellHeaderByteView that corresponds to the starting index
 * tracked in cell_header_indexes_
 */
CellHeaderByteView NodePage::GetCellHeaderByteView(u16 cell_idx) const {
  const CellTracker tracker = cell_trackers_[cell_idx];
  if (!tracker.IsCellWrittenIntoImage()) {
    return tracker.cell.cell_header_;
  }
  CellHeaderByteView cell_header_byte_view{};
  std::memcpy(&cell_header_byte_view, p_image_->data() + tracker.image_idx,
              sizeof(CellHeaderByteView));
  return cell_header_byte_view;
}

/**
 * Returns the CellHeaderByteView that corresponds to the starting index
 * tracked in cell_header_indexes_
 */
CellHeaderByteView NodePage::GetCellHeaderByteViewByImageIndex(
    ImageIndex image_idx) const {
  CellHeaderByteView cell_header_byte_view{};
  std::memcpy(&cell_header_byte_view, p_image_->data() + image_idx,
              sizeof(CellHeaderByteView));
  return cell_header_byte_view;
}

/**
 * Sets the CellHeaderByteView that corresponds to the starting index
 * tracked in cell_header_indexes_
 */
void NodePage::SetCellHeaderByteView(
    u16 cell_idx, CellHeaderByteView &cell_header_byte_view_in) {
  CellTracker tracker = cell_trackers_[cell_idx];
  if (!tracker.IsCellWrittenIntoImage()) {
    tracker.cell.cell_header_ = cell_header_byte_view_in;
    cell_trackers_[cell_idx] = tracker;
    return;
  }

  std::memcpy(p_image_->data() + tracker.image_idx, &cell_header_byte_view_in,
              sizeof(CellHeaderByteView));
}

/**
 * Sets the CellHeaderByteView that corresponds to the starting image index
 */
void NodePage::SetCellHeaderByteViewByImageIndex(
    ImageIndex image_idx, CellHeaderByteView &cell_header_byte_view_in) {
  std::memcpy(p_image_->data() + image_idx, &cell_header_byte_view_in,
              sizeof(CellHeaderByteView));
}

/**
 * Resets the content of the page image and other member variables
 */
void NodePage::ZeroPage() {
  memset(p_image_->data(), 0, kPageSize);

  // Step 1: Reset the NodePageHeader
  NodePageHeaderByteView node_page_header{};
  node_page_header.right_child = 0;
  node_page_header.first_cell_idx = 0;
  node_page_header.first_free_block_idx = sizeof(NodePageHeaderByteView);
  SetNodePageHeaderByteView(node_page_header);

  // Step 2: Reset the first free block in the page
  FreeBlockByteView free_block{};
  free_block.size = kPageSize - sizeof(NodePageHeaderByteView);
  free_block.next_block_idx = 0;
  ImageIndex free_block_offset = sizeof(NodePageHeaderByteView);
  SetFreeBlockByteView(free_block_offset, free_block);

  // Step 3: Reset the cell header start indexes
  cell_trackers_.clear();
  is_overfull_ = false;

  // Step 4: Set num_free_bytes_ to default
  num_free_bytes_ = free_block.size;

  // Step 5: Set p_parent to nullptr
  p_parent_ = nullptr;
}

/**
 * This function moves all the cells to the front of the page image.
 * This is done by creating a new page image called new_page and copying the
 * cells found in the old page image to the new page.
 */
void NodePage::DefragmentPage() {
  // Step 1: Create a new page image
  NodePageHeaderByteView node_page_header = GetNodePageHeaderByteView();
  node_page_header.first_cell_idx = sizeof(NodePageHeaderByteView);
  NodePage new_page{};
  std::memcpy(new_page.p_image_->data(), p_image_->data(), kPageSize);

  // Step 2: Copy the cells from the old page image to the new page image
  CellHeaderByteView cell_header{};
  ImageIndex new_cell_start_idx = sizeof(NodePageHeaderByteView);
  for (auto &tracker : cell_trackers_) {
    //    std::memcpy(&cell_header, new_page.data() + old_cell_start_idx,
    //    sizeof(CellHeaderByteView));
    ImageIndex old_cell_start_idx = tracker.image_idx;
    cell_header = GetCellHeaderByteViewByImageIndex(old_cell_start_idx);
    u16 payload_size;
    if (cell_header.key_size + cell_header.data_size > kMaxLocalPayload) {
      payload_size = 0;
    } else {
      payload_size = cell_header.key_size + cell_header.data_size;
    }
    // Note that the payload is located after the cell header, so we need to add
    // the size of the cell header
    ImageIndex old_payload_start_idx =
        old_cell_start_idx + sizeof(CellHeaderByteView);
    ImageIndex new_payload_start_idx =
        new_cell_start_idx + sizeof(CellHeaderByteView);
    if (payload_size > 0) {
      std::memcpy(new_page.p_image_->data() + new_payload_start_idx,
                  p_image_->data() + old_payload_start_idx, payload_size);
    }
    cell_header.next_cell_start_idx =
        new_cell_start_idx + sizeof(CellHeaderByteView) + payload_size;
    //    std::memcpy(new_page.data() + new_cell_start_idx, &cell_header,
    //    sizeof(CellHeaderByteView));
    new_page.SetCellHeaderByteViewByImageIndex(new_cell_start_idx, cell_header);
    // Since we are looping through the vector by reference, we can update the
    // old cell start index
    CellTracker new_tracker;
    new_tracker.image_idx = new_cell_start_idx;
    new_page.cell_trackers_.push_back(new_tracker);
    new_cell_start_idx = cell_header.next_cell_start_idx;
  }
  num_free_bytes_ = kPageSize - new_cell_start_idx;

  // Step 3: Replace the old page image with the new page image
  std::memcpy(p_image_->data(), new_page.p_image_->data(), kPageSize);
  cell_trackers_ = new_page.cell_trackers_;

  // Update the final cell's next_cell_start_idx to 0
  if (!cell_trackers_.empty()) {
    cell_header = GetCellHeaderByteView(cell_trackers_.size() - 1);
    cell_header.next_cell_start_idx = 0;
    SetCellHeaderByteView(cell_trackers_.size() - 1, cell_header);
    node_page_header = GetNodePageHeaderByteView();
    node_page_header.first_cell_idx = cell_trackers_[0].image_idx;
    SetNodePageHeaderByteView(node_page_header);
  }

  // Step 4: Create a free block at the end of the page
  FreeBlockByteView free_block{};
  free_block.size = num_free_bytes_;
  free_block.next_block_idx = 0;
  node_page_header.first_free_block_idx = new_cell_start_idx;
  SetNodePageHeaderByteView(node_page_header);

  if (new_cell_start_idx + sizeof(FreeBlockByteView) < kPageSize) {
    SetFreeBlockByteView(new_cell_start_idx, free_block);
  }
}

/**
 * AllocateSpace(const u32 num_bytes_in)
 *
 * Allocates enough space in p_image_ to fit a number of bytes requested by
 * num_bytes_in
 */
ImageIndex NodePage::AllocateSpace(const u32 num_bytes_in) {
  // Step 1: Iterate through the free blocks from the start of p_image_.
  // Only stop when a free block with enough space is found.
  ImageIndex free_block_idx = GetNodePageHeaderByteView().first_free_block_idx;
  FreeBlockByteView old_free_block = GetFreeBlockByteView(free_block_idx);
  if (num_free_bytes_ < num_bytes_in || IsOverfull()) {
    return 0;
  }
  while (old_free_block.size < num_bytes_in) {
    if (old_free_block.next_block_idx == 0) {
      DefragmentPage();
      free_block_idx = GetNodePageHeaderByteView().first_free_block_idx;
    } else {
      free_block_idx = old_free_block.next_block_idx;
    }
    old_free_block = GetFreeBlockByteView(free_block_idx);
  }
  NodePageHeaderByteView node_page_header = GetNodePageHeaderByteView();
  ImageIndex next_insertion_idx = free_block_idx;
  if (old_free_block.size == num_bytes_in) {
    node_page_header.first_free_block_idx = old_free_block.next_block_idx;
    SetNodePageHeaderByteView(node_page_header);
  } else {
    FreeBlockByteView new_free_block{};
    new_free_block.next_block_idx = old_free_block.next_block_idx;
    new_free_block.size = old_free_block.size - num_bytes_in;
    //    memcpy(p_image_->data() + next_insertion_idx + num_bytes_in,
    //           &new_free_block,
    //           sizeof(FreeBlockByteView));
    SetFreeBlockByteView(next_insertion_idx + num_bytes_in, new_free_block);
    node_page_header.first_free_block_idx = next_insertion_idx + num_bytes_in;
    SetNodePageHeaderByteView(node_page_header);
  }
  num_free_bytes_ -= num_bytes_in;
  return next_insertion_idx;
}

void NodePage::DropCell(u16 cell_idx) {
  CellHeaderByteView cell_header = GetCellHeaderByteView(cell_idx);
  CellTracker tracker = cell_trackers_[cell_idx];
  if (tracker.IsCellWrittenIntoImage()) {
    FreeSpace(tracker.image_idx, cell_header.GetCellSize());
  }
  cell_trackers_.erase(cell_trackers_.begin() + cell_idx);
  ImageIndex first_cell_start_idx = 0;
  if (!cell_trackers_.empty()) {
    first_cell_start_idx = cell_trackers_[0].image_idx;
  }
  NodePageHeaderByteView page_header = GetNodePageHeaderByteView();
  page_header.first_cell_idx = first_cell_start_idx;
  SetNodePageHeaderByteView(page_header);
}

void NodePage::InsertCell(Cell &cell_in, u16 cell_idx) {
  if (cell_idx > GetNumCells()) {
    return;
  }
  u32 cell_size = cell_in.GetCellSize();
  ImageIndex allocated_start_idx = AllocateSpace(cell_size);
  if (allocated_start_idx == 0) {
    CellTracker tracker;
    tracker.cell = cell_in;
    cell_trackers_.insert(cell_trackers_.begin() + cell_idx, tracker);
    is_overfull_ = true;
  } else {
    CellTracker tracker;
    tracker.image_idx = allocated_start_idx;
    cell_trackers_.insert(cell_trackers_.begin() + cell_idx, tracker);
    SetCellHeaderByteView(cell_idx, cell_in.cell_header_);
    u32 final_offset = allocated_start_idx + sizeof(CellHeaderByteView);
    if (!cell_in.NeedOverflowPage()) {
      std::memcpy(p_image_->data() + final_offset, cell_in.payload_.data(),
                  cell_in.GetPayloadSize());
    }
    ImageIndex first_cell_start_idx = cell_trackers_[0].image_idx;
    NodePageHeaderByteView page_header = GetNodePageHeaderByteView();
    page_header.first_cell_idx = first_cell_start_idx;
    SetNodePageHeaderByteView(page_header);
  }
}

void NodePage::FreeSpace(ImageIndex free_start_idx, u16 num_bytes_to_free) {
  // Step 1: Find the index that is 1 pass the last byte to free
  ImageIndex free_end_idx = free_start_idx + num_bytes_to_free;

  // Step 2: Find the first free block and its starting index
  FreeBlockByteView current_free_block{};
  FreeBlockByteView next_free_block{};
  ImageIndex first_free_block_idx =
      GetNodePageHeaderByteView().first_free_block_idx;
  ImageIndex iterator_idx = first_free_block_idx;

  // Step 3: Iterate through the free blocks before the start of the space we
  // want to free
  while (iterator_idx != 0 && iterator_idx < free_start_idx) {
    current_free_block = GetFreeBlockByteView(iterator_idx);

    // Step 3a: If the current free block is adjacent to the space we want to
    // free, merge them and update the free block's size and next_block_idx
    if (iterator_idx + current_free_block.size == free_start_idx) {
      current_free_block.size += num_bytes_to_free;
      if (iterator_idx + current_free_block.size ==
          current_free_block.next_block_idx) {
        next_free_block =
            GetFreeBlockByteView(current_free_block.next_block_idx);
        current_free_block.size += next_free_block.size;
        current_free_block.next_block_idx = next_free_block.next_block_idx;
      }
      SetFreeBlockByteView(iterator_idx, current_free_block);
      num_free_bytes_ += num_bytes_to_free;
      return;
    }
    iterator_idx = current_free_block.next_block_idx;
  }

  // Step 4: Create a new free block starting at free_start_idx
  FreeBlockByteView new_free_block{};
  if (iterator_idx != free_end_idx) {
    new_free_block.size = num_bytes_to_free;
    new_free_block.next_block_idx = iterator_idx;
  } else {
    next_free_block = GetFreeBlockByteView(iterator_idx);
    new_free_block.size = num_bytes_to_free + next_free_block.size;
    new_free_block.next_block_idx = next_free_block.next_block_idx;
  }
  SetFreeBlockByteView(free_start_idx, new_free_block);

  // Step 5: Update the NodePageHeader and num_free_bytes_
  NodePageHeaderByteView node_page_header = GetNodePageHeaderByteView();
  node_page_header.first_free_block_idx = free_start_idx;
  SetNodePageHeaderByteView(node_page_header);
  num_free_bytes_ += num_bytes_to_free;
}

void NodePage::CopyPage(NodePage &dest) {
  auto &dest_p_image = *dest.p_image_;
  memcpy(dest_p_image.data(), p_image_->data(), p_image_->size());
  dest.p_parent_ = p_parent_;
  dest.is_init_ = true;
  dest.num_free_bytes_ = num_free_bytes_;
  dest.is_overfull_ = is_overfull_;
  dest.cell_trackers_ = cell_trackers_;
}

void NodePage::RelinkCellList() {
  if (cell_trackers_.empty()) {
    return;
  }
  if (is_overfull_) {
    return;  // If the page is overfull, we don't want to relink the cells
  }
  NodePageHeaderByteView node_page_header = GetNodePageHeaderByteView();
  node_page_header.first_cell_idx = cell_trackers_[0].image_idx;
  SetNodePageHeaderByteView(node_page_header);
  for (size_t i = 0; i < cell_trackers_.size() - 1; ++i) {
    CellHeaderByteView current_cell_header = GetCellHeaderByteView(i);
    current_cell_header.next_cell_start_idx = cell_trackers_[i + 1].image_idx;
    SetCellHeaderByteView(i, current_cell_header);
  }
  CellHeaderByteView last_cell_header =
      GetCellHeaderByteView(cell_trackers_.size() - 1);
  last_cell_header.next_cell_start_idx = 0;
  SetCellHeaderByteView(cell_trackers_.size() - 1, last_cell_header);
}

Cell NodePage::GetCell(u16 cell_idx) {
  if (cell_idx >= GetNumCells()) {
    return {};
  }
  CellTracker tracker = cell_trackers_[cell_idx];
  if (tracker.image_idx == 0) {
    return tracker.cell;
  }
  CellHeaderByteView cell_header = GetCellHeaderByteView(cell_idx);
  if (cell_header.overflow_page != 0) {
    return Cell(cell_header, std::vector<std::byte>());
  }
  std::vector<std::byte> result(cell_header.key_size + cell_header.data_size);
  ImageIndex start_idx = tracker.image_idx + sizeof(CellHeaderByteView);
  memcpy(result.data(), p_image_->data() + start_idx,
         cell_header.key_size + cell_header.data_size);
  Cell cell(cell_header, result);
  return cell;
}

bool NodePage::IsOverfull() const { return is_overfull_; }

/**
 * Every member variable except for p_image is considered extra.
 *
 * The Pager will call this function to reset every extra variable.
 */
void NodePage::DestroyExtra() {
  is_init_ = false;
  p_parent_ = nullptr;
  num_free_bytes_ = 0;
  cell_trackers_.clear();
  is_overfull_ = false;
}