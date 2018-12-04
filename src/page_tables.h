#ifndef page_tables_h
#define page_tables_h

#include "types.h"

class PageAttributes {
public:
  PageAttributes& set_present(bool present) { present_ = present; return *this; }
  PageAttributes& set_writable(bool writable) { writable_ = writable; return *this; }
  PageAttributes& set_user_accessible(bool user_accessible) { user_accessible_ = user_accessible; return *this; }
  PageAttributes& set_global(bool global) { global_ = global; return *this; }
  PageAttributes& set_no_execute(bool no_execute) { no_execute_ = no_execute; return *this; }

  bool present() const { return present_; }
  bool writable() const { return writable_; }
  bool user_accessible() const { return user_accessible_; }
  bool global() const { return global_; }
  bool no_execute() const { return no_execute_; }

private:
  bool present_ = true;
  bool writable_ = true;
  bool user_accessible_ = true;
  bool global_ = false;
  bool no_execute_ = false;
};

class PageTableManager {
public:
  PageTableManager();

  void Map(phys_addr_t phys_start, phys_addr_t phys_end,
           virt_addr_t virt_start, virt_addr_t virt_end,
           const PageAttributes& attrs);

  phys_addr_t table_root() const { return table_; }

private:
  void ClearTable(phys_addr_t table);

  phys_addr_t table_;
};

#endif
