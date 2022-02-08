/*
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_GC_X_ZCPU_HPP
#define SHARE_GC_X_ZCPU_HPP

#include "memory/allocation.hpp"
#include "memory/padded.hpp"
#include "utilities/globalDefinitions.hpp"

class Thread;

namespace ZOriginal {

class ZCPU : public AllStatic {
private:
  struct ZCPUAffinity {
    Thread* _thread;
  };

  static PaddedEnd<ZCPUAffinity>* _affinity;
  static THREAD_LOCAL Thread*     _self;
  static THREAD_LOCAL uint32_t    _cpu;

  static uint32_t id_slow();

public:
  static void initialize();

  static uint32_t count();
  static uint32_t id();
};

} // namespace ZOriginal

#endif // SHARE_GC_X_ZCPU_HPP
