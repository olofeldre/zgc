/*
 * Copyright (c) 2019, 2021, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_GC_X_ZATTACHEDARRAY_HPP
#define SHARE_GC_X_ZATTACHEDARRAY_HPP

#include "utilities/globalDefinitions.hpp"

namespace ZOriginal {

template <typename ObjectT, typename ArrayT>
class ZAttachedArray {
  friend class VMStructs;

private:
  const size_t _length;

  static size_t object_size();
  static size_t array_size(size_t length);

public:
  template <typename Allocator>
  static void* alloc(Allocator* allocator, size_t length);

  static void* alloc(size_t length);
  static void free(ObjectT* obj);

  ZAttachedArray(size_t length);

  size_t length() const;
  ArrayT* operator()(const ObjectT* obj) const;
};

} // namespace ZOriginal

#endif // SHARE_GC_X_ZATTACHEDARRAY_HPP
