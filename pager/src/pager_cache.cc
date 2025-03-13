#include "pager.h"

/**
 * Function to update a page's usage information to achieve the LRU behavior
 */
void Pager::updateLRU(BasePage *p_page) {
  /*
   * TODO: A2 -> Complete the function below
   * Hint: Think about what needs to be updated about the two data structures
   * pager maintains
   */
}

/**
 * Function to evict a page when the cache is full, returns a pointer to the
 * page to be evicted
 */
BasePage *Pager::evictPage() {
  if (eviction_policy_ == EvictionPolicy::FIRST_NON_DIRTY) {
    // original caching policy
    return p_free_page_first_->GetFirstNonDirtyPage();
  } else if (eviction_policy_ == EvictionPolicy::LRU) {  // LRU policy
    /*
     * TODO: A2 -> Complete this function
     * Hint: think about how to find the least recently used page?
     */
  }
  return nullptr;
}

void Pager::SqlitePagerPrivateAddCreatedPageToCache(PageNumber page_number,
                                                    BasePage *&p_page) {
  page_hash_table_->operator[](page_number)->InitPageHeader(this, page_number);
  p_page = page_hash_table_->operator[](page_number).get();
  p_all_page_first_ = p_page;
  if (eviction_policy_ == EvictionPolicy::LRU) {
    if (p_free_page_first_ == nullptr) {
      p_free_page_first_ = p_page;
      p_free_page_last_ = p_page;
    } else {
      p_free_page_last_->p_header_->p_next_free_ = p_page;
      p_page->p_header_->p_prev_free_ = p_free_page_last_;
      p_free_page_last_ = p_page;
    }
  }
}

void Pager::SqlitePagerPrivateRemovePageFromCache(PageNumber page_number,
                                                  BasePage *p_page) {
  // unlink the old page from the free list
  // remove from prev_free
  if (p_page->p_header_->p_prev_free_ != nullptr) {
    p_page->p_header_->p_prev_free_->p_header_->p_next_free_ =
        p_page->p_header_->p_next_free_;
  } else {
    p_free_page_first_ = p_page->p_header_->p_next_free_;
  }

  // remove from next_free
  if (p_page->p_header_->p_next_free_ != nullptr) {
    p_page->p_header_->p_next_free_->p_header_->p_prev_free_ =
        p_page->p_header_->p_prev_free_;
  } else {
    p_free_page_last_ = p_page->p_header_->p_prev_free_;
  }

  // clean itself
  p_page->p_header_->p_next_free_ = p_page->p_header_->p_prev_free_ = nullptr;

  // move hash table
  PageNumber old_page_number = p_page->p_header_->page_number_;
  page_hash_table_->operator[](page_number) =
      std::move(page_hash_table_->operator[](old_page_number));
  page_hash_table_->erase(old_page_number);
}

BasePage *Pager::SqlitePagerPrivateCacheLookup(PageNumber page_number) const {
  // the reason why we do this is that by directly access, we create a nullptr
  // for the key, which is not that good
  if (eviction_policy_ == EvictionPolicy::FIRST_NON_DIRTY) {
    auto it = page_hash_table_->find(page_number);
    if (it != page_hash_table_->end()) {
      return it->second.get();
    } else {
      return nullptr;
    }
  } else {
    /*
     * TODO: A2 -> Complete the logic here
     * Hint: How to get a page from the LRU cache using a page number?
     */
  }
}
