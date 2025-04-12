/*
 * balance.cc
 *
 * The file is dedicated to the implementation of the Btree::Balance() function.
 * Students will need to implement the 19 TODOS in this file for Assignment 3.
 *
 * Your line(s) code should replace the lines marked with the pattern "// ***"
 *
 */
#include "btree.h"

/**
 * Balance is usually called from after inserting a cell into leaf node, where
 * p_page is the leaf node. The function is called recursively upwards towards
 * the parent, where p_page becomes the parent node. Eventually, the recursive
 * call will reach the root node, and the Balance function will stop.
 *
 * @param p_page: pointer to the current page we are balancing
 * @param p_cursor: cursor attached to the balance operation, it has to be
 * passed to the function because the key-data pair that the cursor is pointing
 * to may be relocated to a different page and even have a different index on
 * that page. Therefore, we keep the p_cursor around to update during the
 * balancing process.
 * @return: appropriate result code
 */
ResultCode Btree::Balance(NodePage *p_page,
                          const std::weak_ptr<BtCursor> &p_cursor) {
  // Step 1: Check if the page is writable, if not, return kError.
  if (!pager_->SqlitePagerIsWritable(p_page)) {
    return ResultCode::kError;
  }

  // Step 2: Check if the page needs any balancing at all
  // Every database system will have its own definition on when to balance.
  // The logic below is what CapybaraDB uses to determine if a page needs
  // balancing.
  if (!p_page->IsOverfull() && p_page->num_free_bytes_ < kPageSize / 2 &&
      p_page->GetNumCells() >= 2) {
    p_page->RelinkCellList(); // Make sure the cells are linked properly in the
                              // page image.
    return ResultCode::kOk;
  }

  ResultCode rc;
  NodePage *p_parent = p_page->p_parent_;
  BasePage *p_base_page = nullptr;
  NodePage *p_extra_unref = nullptr;
  CellHeaderByteView cell_header{};
  std::vector<NodePageHeaderByteView> divider_page_headers{};

  // Step 3: Check if the current is the root page
  // The root page is very special since every Btree is identified through the
  // root page number. Every table, index you inside the database is tracked If
  // it is a root page, we will apply special logic to balance it
  if (!p_parent) {
    bool return_ok_early = false;
    // TODO: A3 -> Call helper function here to handle the root page
    // TODO: Your code here

    // ***

    // ------------------------------------

    if (rc != ResultCode::kOk) {
      return rc;
    }
    if (return_ok_early) {
      return ResultCode::kOk;
    }
  }

  rc = pager_->SqlitePagerWrite(p_parent);
  if (rc != ResultCode::kOk) {
    return rc;
  }

  // Step 4: Find the index of the cell in the parent that points to the current
  // page
  int idx; // The index of the cell in the parent that points to the current
           // page

  // TODO: A3 -> Call helper function to find the parent cell idx that points to
  // the current page
  // TODO: Your code here

  // ***

  // ------------------------------------

  // TODO: A3 -> Assign the correct value to discovered_file_corruption
  // according to the value of idx. Hint: Inspect the logic of
  // BalanceHelperFindChildIdx to determine what it returns when it cannot find
  // the index of the cell in the parent that points to the current page.
  // ------------------------------------
  // TODO: Your code here
  bool discovered_file_corruption;

  // ***

  if (discovered_file_corruption) {
    return ResultCode::kCorrupt;
  }

  /*
   * In the following section, we will store p_page and 2 of its siblings in
   * divider_pages. Divider pages are pages where we will remove its cells and
   * redistribute them to new pages.
   *
   * The cells that are associated with these pages will be known as divider
   * cells. Note that there might be 1 more divider page than divider cell
   * because the right child page number is not associated with a cell.
   *
   * We have already found the index of the cell in p_parent that points to
   * p_page in step 4. Using this information, we can inspect the cell headers
   * to the left and right or idx and find the siblings. Ideally, we want a
   * sibling from the left and right of p_page. However, we might end up with 2
   * siblings the right child is chosen in the case of idx ==
   * p_parent->GetNumCells().
   *
   * In the case of that p_parent has 3 or fewer children, all of p_parents'
   * children will be stored in divider_pages.
   */

  // Some containers to store info about the pages and cells we want to divide

  std::vector<NodePage *>
      divider_pages{}; // Stores the pages we will need to divide
  std::vector<u16> num_cells_in_divider_pages{}; // Stores the number of cells
                                                 // in the divider pages
  std::vector<PageNumber>
      divider_page_numbers{}; // Stores the page number associated with the
                              // divider pages
  std::vector<u16>
      divider_cell_indexes{};        // Stores the indexes of the divider cells
                                     // associated with the page numbers
  std::vector<Cell> divider_cells{}; // Stores the divider cells
  std::vector<Cell> redistributed_cells{}; // Stores the cells that will be
                                           // redistributed to the new pages
  std::vector<u16> new_divider_cell_indexes;

  // Here are some variables we initialize due to using goto
  // ----------------
  u16 cursor_cell_index;
  std::vector<std::unique_ptr<NodePage>> divider_page_copies{};
  std::vector<u32> redistributed_cell_sizes{};
  u32 num_cells_inserted;
  u32 subtotal;
  std::vector<u32> new_combined_cell_sizes{};
  std::vector<u16> first_indexes_of_next_page{};
  std::vector<std::pair<PageNumber, NodePage *>> new_page_number_to_page{};
  NodePageHeaderByteView page_header{};
  PageNumber final_right_child = 0;
  // ---------

  pager_->SqlitePagerRef(p_parent);

  // Step 4: Find which pages are the divider pages
  // These are the pages that will have their cells redistributed to new pages
  int divider_start_cell_idx;
  if (idx == (int)p_parent->GetNumCells()) {
    // In case if idx is the right child, set the start to the 2nd to last cell
    // When iterating through 3 cells, the right child will be the final cell
    divider_start_cell_idx = idx - 2;
  } else {
    // In case if idx is not the right child, set the start to the cell to the
    // left of idx When iterating through 3 cells, idx will be in the middle
    divider_start_cell_idx = idx - 1;
  }
  if (divider_start_cell_idx < 0) {
    divider_start_cell_idx = 0;
  }

  int num_cells_in_parent = (int)p_parent->GetNumCells();

  // Step 5: Store the divider pages, cells, and cell indexes
  for (int i = 0, k = divider_start_cell_idx; i < 3; ++i, ++k) {
    if (k < num_cells_in_parent) {
      // This is the case where the left child is chosen
      divider_cell_indexes.push_back(k);
      PageNumber left_child_page_number;

      // TODO: A3 -> Assign the correct value to left_child_page_number
      // It should be the left child inside the cell header of the cell at index
      // k. There is a function in node_page.cc that you should call here
      // TODO: Your code here

      // ***

      // ------------------------------------

      divider_page_numbers.push_back(left_child_page_number);
      divider_cells.push_back(p_parent->GetCell(k));
    } else if (k == num_cells_in_parent) {
      // This is the case where the right child is chosen
      // Note that since the right child is not associated with a cell header,
      // we will not push anything into divider_cell_headers

      PageNumber right_child_page_number;

      // TODO: A3 -> Assign the correct value to right_child_page_number
      // This value could be found inside the p_parent's node page header
      // There is a function in node_page.cc that you should call here
      // TODO: Your code here

      // ***

      // ------------------------------------

      divider_page_numbers.push_back(right_child_page_number);
    } else {
      break;
    }

    // Get the page of the sibling and store it in divider_pages
    rc = pager_->SqlitePagerGet(divider_page_numbers.back(), &p_base_page,
                                NodePage::CreateDerivedPage);
    if (rc != ResultCode::kOk) {
      goto balance_cleanup;
    }
    auto *p_node_page = dynamic_cast<NodePage *>(p_base_page);
    divider_page_headers.push_back(p_node_page->GetNodePageHeaderByteView());
    rc = InitPage(*p_node_page, p_parent);
    if (rc != ResultCode::kOk) {
      goto balance_cleanup;
    }
    divider_pages.push_back(p_node_page);
  }

  /*
   * Step 6: Keep track of where the cursor should point to after the balancing
   * operation
   *
   * In the later steps, every cell will be removed and combined into a vector
   * called redistributed_cells. The cursor keeps track of which cell on which
   * page it is pointing to. If p_cursor is not expired, we want to make sure
   * that the cursor points to the same cell.
   */
  NodePage *p_old_page;
  cursor_cell_index = 0;
  if (!p_cursor.expired()) {
    auto cursor = *p_cursor.lock();
    cursor_cell_index = 0;
    // Iterate through all the divider pages and its cells to find the cursor's
    // cell index. This value should be between 0 and the size of the
    // redistributed_cells vector. When redistributing the cells, as we reach
    // the cell that the cursor is pointing to, we can correctly update the
    // cursor to point to the new page and cell index
    for (size_t i = 0; i < divider_page_numbers.size(); ++i) {
      if (cursor.p_page == divider_pages[i]) {
        cursor_cell_index += cursor.cell_index;
        break;
      }
      cursor_cell_index += divider_pages[i]->GetNumCells();
      // Note that BalanceHelperHandleRoot could have made p_page into p_parent
      // Thus, we need to check for this case
      if (i < divider_page_numbers.size() - 1 && cursor.p_page == p_parent &&
          cursor.cell_index == divider_cell_indexes[i]) {
        break;
      }
      ++cursor_cell_index;
    }
    p_old_page = cursor.p_page;
  }

  /*
   * Step 7: Remove the cells from the divider pages and store them in
   * redistributed_cells. Also, keep track of the sizes of the cells in
   * redistributed_cell_sizes. We need these sizes to figure out how many pages
   * we need to store the redistributed cells
   */
  for (size_t i = 0; i < divider_page_numbers.size(); ++i) {
    num_cells_in_divider_pages.push_back(divider_pages[i]->GetNumCells());
    for (size_t j = 0; j < divider_pages[i]->GetNumCells(); ++j) {
      redistributed_cells.push_back(divider_pages[i]->GetCell(j));
      redistributed_cell_sizes.push_back(
          redistributed_cells.back().GetCellSize());
    }
    if (i < divider_page_numbers.size() - 1) {
      redistributed_cells.push_back(divider_cells[i]);
      redistributed_cells.back().cell_header_.left_child =
          divider_page_headers[i].right_child;
      redistributed_cell_sizes.push_back(
          redistributed_cells.back().GetCellSize());
      p_parent->DropCell(divider_start_cell_idx);
    } else {
      final_right_child = divider_page_headers[i].right_child;
    }

    // After the cells are removed from the pages, they are no longer needed.
    // We can add them to the free list
    p_base_page = divider_pages[i];
    auto *p_node_page = dynamic_cast<NodePage *>(p_base_page);
    p_node_page->ZeroPage();

    PageNumber page_number_to_free;

    // TODO: A3 -> Assign the correct value to page_number_to_free
    // We have extracted all cells from the page, so we can free it.
    // Assign the correct value to page_number_to_free
    // Where could we find the page number of the page we want to free for
    // iteration i? That is the value you should assign to page_number_to_free
    // TODO: Your code here

    // ***

    // ------------------------------------

    rc = FreePage(p_base_page, page_number_to_free, false);
    if (rc != ResultCode::kOk) {
      return rc;
    }
  }

  /*
   * Step 8: Calculate the sizes of the cells in the new pages.
   * We will redistribute the cells into new pages such that the cells are
   * balanced. The cells will be balanced such that the total size of the cells
   * in each page is less than or equal to kUsableSpace
   */

  subtotal = 0;
  for (u32 i = 0; i < redistributed_cell_sizes.size(); ++i) {
    u32 cell_size = redistributed_cell_sizes[i];
    if (subtotal + cell_size > kUsableSpace) {
      new_combined_cell_sizes.push_back(subtotal);
      new_divider_cell_indexes.push_back(i);
      assert(new_combined_cell_sizes.back() <= kUsableSpace);
      subtotal = cell_size;
    } else {
      subtotal += cell_size;
    }
  }
  new_combined_cell_sizes.push_back(subtotal);
  assert(new_combined_cell_sizes.back() <= kUsableSpace);
  new_divider_cell_indexes.push_back(redistributed_cell_sizes.size());

  /*
   * Step 9: Evenly distribute the cell sizes on each page
   *
   * In the previous step, we tried to cram as many cells as possible into each
   * page. This would result in the front pages having most space used up, but
   * the final page having a lot of free space.
   *
   * We can start from the back, and pull some cells from the front pages to the
   * back pages
   */
  for (u32 i = new_combined_cell_sizes.size() - 1; i > 0; --i) {
    // From the back, pull cells from the front pages to the back pages if
    // as long as the back page has less than kUsableSpace / 2 bytes of free
    // space
    while (new_combined_cell_sizes[i] < kUsableSpace / 2) {
      new_divider_cell_indexes[i - 1] -= 1;
      new_combined_cell_sizes[i] +=
          redistributed_cell_sizes[new_divider_cell_indexes[i - 1]];
      new_combined_cell_sizes[i - 1] -=
          redistributed_cell_sizes[new_divider_cell_indexes[i - 1] - 1];
    }
  }

  assert(new_combined_cell_sizes[0] > 0);

  // Step 10: Allocate new pages for the cells
  for (u32 i = 0; i < new_combined_cell_sizes.size(); ++i) {
    NodePage *p_new_page = nullptr;
    PageNumber new_page_number{};

    // TODO: A3 -> Call a B-Tree function to allocate a new page for p_new_page.
    // The page number is stored in the new_page_number variable
    // TODO: Your code here

    // ***

    // ------------------------------------

    if (rc != ResultCode::kOk) {
      goto balance_cleanup;
    }
    p_new_page->ZeroPage();
    p_new_page->is_init_ = true;
    new_page_number_to_page.emplace_back(new_page_number, p_new_page);
  }

  /*
   * Step 11: Sort the new page number and page pairs by page number
   *
   *
   * In many cases, the VDBE layer will iterate through Btree from left to
   * right. If the pages are not sorted, then Pager will have to jump around the
   * file to read the pages when we could have read the pages in a linear
   * fashion. Having the pages sorted can in turn helps the operating system to
   * deliver pages from the disk more rapidly.
   */

  // TODO: A3 -> Sort the new_page_number_to_page vector by page number
  // Each element in the vector is a pair of page number and NodePage pointer
  // TODO: Your code here

  // ***

  // ------------------------------------

  /*
   * Step 12: Insert the cells into the new pages and update the cursor
   */
  num_cells_inserted = 0; // Keep track of how many cells we have inserted
  for (u32 i = 0; i < new_page_number_to_page.size(); ++i) {
    PageNumber new_page_number;
    NodePage *p_new_page;

    // TODO: A3 -> Assign the correct values to new_page_number and p_new_page
    // for iteration i
    // TODO: Your code here

    // ***
    // ***

    // -----------------------------------

    // 12 - 1: Insert the cells the child pages
    while (num_cells_inserted < new_divider_cell_indexes[i]) {
      Cell cell_to_insert = redistributed_cells[num_cells_inserted];
      // Update the cursor if necessary
      if (num_cells_inserted == cursor_cell_index && !p_cursor.expired()) {
        // TODO: A3 -> Assign the correct values to p_page and cell_index inside
        // p_cursor Assign the correct values to p_page and cell_index inside
        // p_cursor p_page should point to p_new_page cell_index should be the
        // number of cells in p_new_page
        // TODO: Your code here

        // ***
        // ***

        // -----------------------------------
      }

      // TODO: A3 -> Insert the cell (cell_to_insert) into the new page
      // The cell_index should be the number of cells in p_new_page
      // There is a function in node_page.cc that can help with this
      // TODO: Your code here

      // ***

      // -----------------------------------

      num_cells_inserted++;
    }
    // 12 - 2: Insert cells into the parent page
    if (i < new_page_number_to_page.size() - 1) {
      page_header = p_new_page->GetNodePageHeaderByteView();
      page_header.right_child =
          redistributed_cells[num_cells_inserted].cell_header_.left_child;
      p_new_page->SetNodePageHeaderByteView(page_header);
      Cell cell_to_insert = redistributed_cells[num_cells_inserted];
      cell_to_insert.cell_header_.left_child = new_page_number;
      if (num_cells_inserted == cursor_cell_index && !p_cursor.expired()) {

        // TODO: A3 -> Assign the correct values to p_page and cell_index inside
        // p_cursor p_page should point to p_parent cell_index should be
        // divider_start_cell_idx
        // TODO: Your code here

        // ***
        // ***

        // -----------------------------------
      }

      // TODO: A3 -> Insert the cell (cell_to_insert) into the parent page
      // The cell_index should be divider_start_cell_idx
      // There should be a function in node_page.cc that can help with this
      // TODO: Your code here

      // ***

      // -----------------------------------

      num_cells_inserted++;
      divider_start_cell_idx++;
    }
  }

  // 12 - 3: Update the right child of the parent properly
  page_header =
      new_page_number_to_page.back().second->GetNodePageHeaderByteView();
  page_header.right_child = final_right_child;
  new_page_number_to_page.back().second->SetNodePageHeaderByteView(page_header);
  if (divider_start_cell_idx == p_parent->GetNumCells()) {
    page_header = p_parent->GetNodePageHeaderByteView();
    page_header.right_child = new_page_number_to_page.back().first;
    p_parent->SetNodePageHeaderByteView(page_header);
  } else {
    cell_header = p_parent->GetCellHeaderByteView(divider_start_cell_idx);
    cell_header.left_child = new_page_number_to_page.back().first;
    p_parent->SetCellHeaderByteView(divider_start_cell_idx, cell_header);
  }
  if (!p_cursor.expired()) {
    auto cursor = *p_cursor.lock();
    if (num_cells_inserted <= cursor_cell_index && cursor.p_page == p_parent &&
        cursor.cell_index > num_cells_in_divider_pages.back()) {
      cursor.cell_index +=
          new_page_number_to_page.size() - divider_page_numbers.size();
    } else {
      pager_->SqlitePagerRef(p_cursor.lock()->p_page);
      pager_->SqlitePagerUnref(p_old_page);
    }
  }

  /*
   * Step 13: Re-parent the child pages and the parent page
   *
   * After the cells are redistributed, into the node pages, we must update the
   * parent pointers.
   */

  // TODO: A3 -> Call B-Tree function to re-parent child pages on each of the
  // new pages we created.
  // The pointer to each page is stored in new_page_number_to_page
  // TODO: Your code here

  // ***

  // ------------------------------------

  ReParentChildPages(*p_parent);

// Step 14: Call Balance on the parent page

// TODO: A3 -> Call Balance on the parent page
// TODO: Your code here

// ***

// ------------------------------------

// Step 15: Cleanup, unreferencing the pages and returning the result code
balance_cleanup:
  if (p_extra_unref) {
    pager_->SqlitePagerUnref(p_extra_unref);
  }
  for (auto &page : divider_pages) {
    pager_->SqlitePagerUnref(page);
  }
  for (auto &page_number_page_pair : new_page_number_to_page) {
    pager_->SqlitePagerUnref(page_number_page_pair.second);
  }
  if (!p_cursor.expired() && !p_cursor.lock()->p_page) {
    p_cursor.lock()->p_page = p_parent;
    p_cursor.lock()->cell_index = 0;
  } else {
    pager_->SqlitePagerUnref(p_parent);
  }

  return rc;
}

/**
 * This is a helper function for Btree::Balance(), which helps handle the
 * special case of the root page. Hint: remember that the root page identifies
 * the table or index in the database. For any other page in the table, you can
 * copy its content into memory, free the page, allocate another page, and copy
 * the content back into the new page. But you cannot do this with the root
 * page!
 * @param p_page: pointer to the current page
 * @param p_parent: parent of the current page
 * @param p_cursor: weak reference to the cursor
 * @param p_extra_unref: temporary placeholder for page that needs to be unref'd
 * later
 * @param return_ok_early: early balance termination flag
 * @return: appropriate result code
 */
ResultCode
Btree::BalanceHelperHandleRoot(NodePage *&p_page, NodePage *&p_parent,
                               const std::weak_ptr<BtCursor> &p_cursor,
                               NodePage *&p_extra_unref,
                               bool &return_ok_early) {

  ResultCode rc;
  PageNumber child_page_number;
  NodePage *p_child = nullptr;

  // Step 1: Handle the case where the root page has no cells
  if (p_page->cell_trackers_.empty()) {

    /*
     * 1-1: Handle the case where the root page has a right child
     *
     * There may be cases where a single page needed to redistribute all the
     * cells during the balance process. Eventually, the Balance function will
     * attach that page to the right child of the root page with all the cells
     * in the root page being removed.
     *
     * When this occurs, we have to copy the content of the right child into the
     * root page and free the right child.
     */
    bool root_has_right_child;

    // TODO: A3 -> Assign the correct value to root_has_right_child
    // You can find the right child page number inside the root page's node page
    // header p_page should point to the root page at this point. A function in
    // node_page.cc could help you find the page's node page header. After that
    // you can check if the right child page number is 0. If it is not 0, then
    // it means that the root page has a right child. If it is 0, then it means
    // that the root page has no right child
    // TODO: Your code here

    // ***

    // --------------------------

    if (root_has_right_child) {
      // Get right child
      BasePage *p_base_page = nullptr;
      child_page_number = p_page->GetNodePageHeaderByteView().right_child;
      rc = pager_->SqlitePagerGet(child_page_number, &p_base_page,
                                  NodePage::CreateDerivedPage);
      if (rc != ResultCode::kOk) {
        return rc;
      }
      p_child = dynamic_cast<NodePage *>(p_base_page);

      // Copy the right child into the root page
      p_child->CopyPage(*p_page);
      p_page->p_parent_ = nullptr;
      ReParentChildPages(*p_page);

      // Free the right child
      auto cursor = p_cursor.lock();
      if (cursor->p_page == p_child) {
        pager_->SqlitePagerUnref(p_child);
        cursor->p_page = p_page;
        pager_->SqlitePagerRef(p_page);
      }
      FreePage(p_base_page, child_page_number, false);
      pager_->SqlitePagerUnref(p_child);
    } else {
      p_page->RelinkCellList();
    }
    return_ok_early = true;
    return ResultCode::kOk;
  }

  /*
   * Step 2: Handle case where the page is not overfull
   *
   * Overfull occurs when a cell cannot be written into the page image because
   * there is not enough space. If every cell has been written into the page
   * image, we can return early.
   */
  bool root_page_is_overfull;

  // TODO: A3 -> Assign the correct value to root_page_is_overfull
  // At this point p_page points to the root page.
  // There should be a function in node_page.cc that can help you determine
  // if the page that p_page points to is overfull
  // TODO: Your code here

  // ***

  // -------------------------

  if (!root_page_is_overfull) {
    p_page->RelinkCellList();
    return_ok_early = true;
    return ResultCode::kOk;
  }

  // At this point, the root page is overfull.
  // Create a child page and copy the root page's content into the child page.
  // Replace the original root page with 0 cells and a right child pointer to
  // the child page. The code inside Balance() after the helper function should
  // split the child page's content properly.
  rc = pager_->SqlitePagerWrite(p_page);
  if (rc != ResultCode::kOk) {
    return rc;
  }
  rc = AllocatePage(p_child, child_page_number);
  if (rc != ResultCode::kOk) {
    return rc;
  }

  p_page->CopyPage(*p_child);
  p_child->p_parent_ = p_page;
  pager_->SqlitePagerRef(p_page);
  p_child->is_overfull_ = true;
  if (p_cursor.lock()->p_page == p_page) {
    pager_->SqlitePagerUnref(p_page);
    p_cursor.lock()->p_page = p_child;
  } else {
    p_extra_unref = p_child;
  }
  p_page->ZeroPage();
  NodePageHeaderByteView header = p_page->GetNodePageHeaderByteView();
  header.right_child = child_page_number;
  p_page->SetNodePageHeaderByteView(header);
  // The next part of the balancing function will redistribute the parent's
  // cells into the child pages We can trick the function into thinking that
  // p_page is a parent page
  p_parent = p_page;
  p_page = p_child;
  return_ok_early = false; // We still need to balance the child page
  return ResultCode::kOk;
}

/**
 * This function is meant to check which cell in the parent (p_parent) points at
 * the current page (p_page)
 * @param p_page: pointer to the current page
 * @param p_parent: pointer to the parent of current page
 * @return index of correct cell or -1 if failed
 */
int Btree::BalanceHelperFindChildIdx(NodePage *p_page, NodePage *p_parent) {
  // Find the index of the p_parent's cells that points to p_page and store it
  // in idx
  int idx = -1;
  PageNumber current_page_number;

  // TODO: A3 -> Assign the correct value to current_page_number
  // The Btree layer normally doesn't know what page number each page has.
  // It is an information only known by the Pager layer.
  // Assign the correct value to current_page_number by calling the function in
  // pager.cc that returns the pager number of a page
  // TODO: Your code here

  // ***

  // -----------------------------

  // Step 1: Find the index of the cell in p_parent that points to p_page
  for (int i = 0; i < p_parent->GetNumCells(); ++i) {
    bool is_left_child = false;

    // TODO: A3 -> Assign the correct value to is_left_child
    // You should get the the cell header inside the parent nodes that points to
    // the current page.
    // If the left child of the cell points to the current page, then
    // is_left_child should be true
    //
    // There should be a function in node_page.cc that allows you to get the
    // cell header of a cell at index i.
    // TODO: Your code here

    // ***
    // ***

    // -----------------------------

    if (is_left_child) {
      // Found the index of the cell in p_parent that points to p_page
      // Store it in idx and break
      idx = i;
      break;
    }
  }

  // Step 2: Handle the case where the right child of p_parent points to p_page
  // If there is no cell in p_parent that points to p_page, check the p_parent's
  // right child It that right child points to p_page, then set idx to
  // p_parents' number of cells
  if (idx < 0 && p_parent->GetNodePageHeaderByteView().right_child ==
                     current_page_number) {
    idx = (int)p_parent->GetNumCells();
  }

  // Step 3: Return the index
  // In cases where the idx is neither found in the left child of each cell nor
  // the right child of the page, the value of -1 will be returned.
  return idx;
}
