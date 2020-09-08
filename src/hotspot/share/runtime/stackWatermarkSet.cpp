/*
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
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
 *
 */

#include "precompiled.hpp"
#include "logging/log.hpp"
#include "runtime/atomic.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/safepoint.hpp"
#include "runtime/safepointMechanism.inline.hpp"
#include "runtime/stackWatermark.inline.hpp"
#include "runtime/stackWatermarkSet.hpp"
#include "runtime/thread.hpp"
#include "utilities/debug.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/preserveException.hpp"
#include "utilities/vmError.hpp"

StackWatermarkSet::StackWatermarkSet() :
    _head(NULL) {}

StackWatermarkSet::~StackWatermarkSet() {
  StackWatermark* current = _head;
  while (current != NULL) {
    StackWatermark* next = current->next();
    delete current;
    current = next;
  }
}

static void verify_poll_context() {
#ifdef ASSERT
  Thread* thread = Thread::current();
  if (thread->is_Java_thread()) {
    JavaThread* jt = static_cast<JavaThread*>(thread);
    JavaThreadState state = jt->thread_state();
    assert(state != _thread_in_native, "unsafe thread state");
    assert(state != _thread_blocked, "unsafe thread state");
  } else if (thread->is_VM_thread()) {
  } else {
    assert_locked_or_safepoint(Threads_lock);
  }
#endif
}

void StackWatermarkSet::before_unwind(JavaThread* jt) {
  verify_poll_context();
  if (!jt->has_last_Java_frame()) {
    // Sometimes we throw exceptions and use native transitions on threads that
    // do not have any Java threads. Skip those callsites.
    return;
  }
  for (StackWatermark* current = jt->stack_watermark_set()->_head; current != NULL; current = current->next()) {
    current->before_unwind();
  }
  SafepointMechanism::update_poll_values(jt);
}


void StackWatermarkSet::after_unwind(JavaThread* jt) {
  verify_poll_context();
  if (!jt->has_last_Java_frame()) {
    // Sometimes we throw exceptions and use native transitions on threads that
    // do not have any Java threads. Skip those callsites.
    return;
  }
  for (StackWatermark* current = jt->stack_watermark_set()->_head; current != NULL; current = current->next()) {
    current->after_unwind();
  }
  SafepointMechanism::update_poll_values(jt);
}

void StackWatermarkSet::on_iteration(JavaThread* jt, frame fr) {
  if (VMError::is_error_reported()) {
    // Don't perform barrier when error reporting walks the stack.
    return;
  }
  verify_poll_context();
  for (StackWatermark* current = jt->stack_watermark_set()->_head; current != NULL; current = current->next()) {
    current->on_iteration(fr);
  }
}

void StackWatermarkSet::start_iteration(JavaThread* jt, StackWatermarkKind kind) {
  verify_poll_context();
  for (StackWatermark* current = jt->stack_watermark_set()->_head; current != NULL; current = current->next()) {
    if (current->kind() == kind) {
      current->start_iteration();
    }
  }
}

void StackWatermarkSet::finish_iteration(JavaThread* jt, void* context, StackWatermarkKind kind) {
  for (StackWatermark* current = jt->stack_watermark_set()->_head; current != NULL; current = current->next()) {
    if (current->kind() == kind) {
      current->finish_iteration(context);
    }
  }
}

uintptr_t StackWatermarkSet::lowest_watermark() {
  uintptr_t max_watermark = uintptr_t(0) - 1;
  uintptr_t watermark = max_watermark;
  for (StackWatermark* current = _head; current != NULL; current = current->next()) {
    watermark = MIN2(watermark, current->watermark());
  }
  if (watermark == max_watermark) {
    return 0;
  } else {
    return watermark;
  }
}

void StackWatermarkSet::add_watermark(StackWatermark* watermark) {
  StackWatermark* prev = _head;
  watermark->set_next(prev);
  _head = watermark;
}
