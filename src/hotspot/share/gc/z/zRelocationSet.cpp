/*
 * Copyright (c) 2017, 2021, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "precompiled.hpp"
#include "gc/z/zArray.inline.hpp"
#include "gc/z/zCollectedHeap.hpp"
#include "gc/z/zDriver.hpp"
#include "gc/z/zForwarding.inline.hpp"
#include "gc/z/zForwardingAllocator.inline.hpp"
#include "gc/z/zPageAllocator.hpp"
#include "gc/z/zRelocationSet.inline.hpp"
#include "gc/z/zRelocationSetSelector.inline.hpp"
#include "gc/z/zStat.hpp"
#include "gc/z/zTask.hpp"
#include "gc/z/zWorkers.hpp"
#include "runtime/atomic.hpp"
#include "utilities/debug.hpp"

class ZRelocationSetInstallTask : public ZTask {
private:
  ZForwardingAllocator* const    _allocator;
  ZForwarding**                  _forwardings;
  const size_t                   _nforwardings;
  const bool                     _promote_all;
  ZArrayParallelIterator<ZPage*> _small_iter;
  ZArrayParallelIterator<ZPage*> _medium_iter;
  volatile size_t                _small_next;
  volatile size_t                _medium_next;

  void install(ZForwarding* forwarding, volatile size_t* next) {
    const size_t index = Atomic::fetch_and_add(next, 1u);
    assert(index < _nforwardings, "Invalid index");

    forwarding->page()->log_msg(" (relocation selected)");

    _forwardings[index] = forwarding;
  }

  void install_small(ZForwarding* forwarding) {
    install(forwarding, &_small_next);
  }

  void install_medium(ZForwarding* forwarding) {
    install(forwarding, &_medium_next);
  }

public:
  ZRelocationSetInstallTask(ZForwardingAllocator* allocator, const ZRelocationSetSelector* selector) :
      ZTask("ZRelocationSetInstallTask"),
      _allocator(allocator),
      _forwardings(NULL),
      _nforwardings(selector->selected_small()->length() + selector->selected_medium()->length()),
      _promote_all(selector->promote_all()),
      _small_iter(selector->selected_small()),
      _medium_iter(selector->selected_medium()),
      _small_next(selector->selected_medium()->length()),
      _medium_next(0) {

    // Reset the allocator to have room for the relocation
    // set, all forwardings, and all forwarding entries.
    const size_t relocation_set_size = _nforwardings * sizeof(ZForwarding*);
    const size_t forwardings_size = _nforwardings * sizeof(ZForwarding);
    const size_t forwarding_entries_size = selector->forwarding_entries() * sizeof(ZForwardingEntry);
    _allocator->reset(relocation_set_size + forwardings_size + forwarding_entries_size);

    // Allocate relocation set
    _forwardings = new (_allocator->alloc(relocation_set_size)) ZForwarding*[_nforwardings];
  }

  ~ZRelocationSetInstallTask() {
    assert(_allocator->is_full(), "Should be full");
  }

  virtual void work() {
    // Allocate and install forwardings for small pages
    for (ZPage* page; _small_iter.next(&page);) {
      ZForwarding* const forwarding = ZForwarding::alloc(_allocator, page, _promote_all);
      install_small(forwarding);
    }

    // Allocate and install forwardings for medium pages
    for (ZPage* page; _medium_iter.next(&page);) {
      ZForwarding* const forwarding = ZForwarding::alloc(_allocator, page, _promote_all);
      install_medium(forwarding);
    }
  }

  ZForwarding** forwardings() const {
    return _forwardings;
  }

  size_t nforwardings() const {
    return _nforwardings;
  }
};

ZRelocationSet::ZRelocationSet(ZCollector* collector) :
    _collector(collector),
    _allocator(),
    _forwardings(NULL),
    _nforwardings(0),
    _promotion_lock(),
    _promote_flipped_pages(),
    _promote_relocated_pages() {}

ZWorkers* ZRelocationSet::workers() const {
  return _collector->workers();
}

ZCollector* ZRelocationSet::collector() const {
  return _collector;
}

ZArray<ZPage*>* ZRelocationSet::promote_flipped_pages() {
  return &_promote_flipped_pages;
}

ZArray<ZPage*>* ZRelocationSet::promote_relocated_pages() {
  return &_promote_relocated_pages;
}

void ZRelocationSet::install(const ZRelocationSetSelector* selector) {
  // Install relocation set
  ZRelocationSetInstallTask task(&_allocator, selector);
  workers()->run(&task);

  _forwardings = task.forwardings();
  _nforwardings = task.nforwardings();

  // Update statistics
  _collector->stat_relocation()->set_at_install_relocation_set(_allocator.size());
}

static void destroy_and_clear(ZPageAllocator* page_allocator, ZArray<ZPage*>* array) {
  for (int i = 0; i < array->length(); i++) {
    // Delete non-relocating promoted pages from last cycle
    ZPage* page = array->at(i);
    page_allocator->safe_destroy_page(page);
  }
  array->clear();
}
void ZRelocationSet::reset(ZPageAllocator* page_allocator) {
  // Destroy forwardings
  ZRelocationSetIterator iter(this);
  for (ZForwarding* forwarding; iter.next(&forwarding);) {
    forwarding->~ZForwarding();
  }

  _nforwardings = 0;

  destroy_and_clear(page_allocator, &_promote_relocated_pages);
  destroy_and_clear(page_allocator, &_promote_flipped_pages);
}

void ZRelocationSet::register_promote_flipped(const ZArray<ZPage*>& pages) {
  ZLocker<ZLock> locker(&_promotion_lock);
  for (ZPage* page : pages) {
    assert(!_promote_flipped_pages.contains(page), "no duplicates allowed");
    _promote_flipped_pages.append(page);
  }
}

void ZRelocationSet::register_promote_relocated(ZPage* page) {
  ZLocker<ZLock> locker(&_promotion_lock);
  assert(!_promote_relocated_pages.contains(page), "no duplicates allowed");
  _promote_relocated_pages.append(page);
}
