/*
 * Copyright (c) 2015, 2021, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_GC_Z_ZRELOCATE_HPP
#define SHARE_GC_Z_ZRELOCATE_HPP

#include "gc/z/zRelocationSet.hpp"
#include "gc/z/zPageAge.hpp"

class ZForwarding;
class ZWorkers;

class ZRelocateQueue {
private:
  ZConditionLock       _lock;
  ZArray<ZForwarding*> _queue;
  uint                 _nworkers;
  uint                 _nsynchronized;
  bool                 _synchronize;
  volatile int         _needs_attention;

  bool needs_attention() const;
  void inc_needs_attention();
  void dec_needs_attention();

  bool is_enabled() const;

public:
  ZRelocateQueue();

  void join(uint nworkers);
  void leave();

  void add(ZForwarding* forwarding);
  bool poll(ZForwarding** forwarding, bool* synchronized);
  void clear();

  void synchronize();
  void desynchronize();
};

class ZRelocate {
  friend class ZRelocateTask;

private:
  ZCollector* const _collector;
  ZRelocateQueue    _queue;

  ZWorkers* workers() const;
  void work(ZRelocationSetParallelIterator* iter);

public:
  ZRelocate(ZCollector* collector);

  void start();

  static void add_remset(volatile zpointer* p);
  static void add_remset_for_fields(volatile zaddress addr);

  static ZPageAge compute_to_age(ZPageAge from_age, bool promote_all);

  zaddress relocate_object(ZForwarding* forwarding, zaddress_unsafe from_addr);
  zaddress forward_object(ZForwarding* forwarding, zaddress_unsafe from_addr);

  void relocate(ZRelocationSet* relocation_set);

  void flip_age_pages(const ZArray<ZPage*>* pages, bool promote_all);

  void synchronize();
  void desynchronize();
};

#endif // SHARE_GC_Z_ZRELOCATE_HPP
