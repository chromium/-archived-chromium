// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/dynamic_annotations.h"
#include "base/third_party/valgrind/valgrind.h"

#ifndef NDEBUG
// Each function is empty and called (via a macro) only in debug mode.
// The arguments are captured by dynamic tools at runtime.

extern "C" void AnnotateRWLockCreate(const char *file, int line,
                                     const volatile void *lock) {}
extern "C" void AnnotateRWLockDestroy(const char *file, int line,
                                      const volatile void *lock) {}
extern "C" void AnnotateRWLockAcquired(const char *file, int line,
                                       const volatile void *lock, long is_w) {}
extern "C" void AnnotateRWLockReleased(const char *file, int line,
                                       const volatile void *lock, long is_w) {}
extern "C" void AnnotateCondVarWait(const char *file, int line,
                                    const volatile void *cv,
                                    const volatile void *lock) {}
extern "C" void AnnotateCondVarSignal(const char *file, int line,
                                      const volatile void *cv) {}
extern "C" void AnnotateCondVarSignalAll(const char *file, int line,
                                         const volatile void *cv) {}
extern "C" void AnnotatePublishMemoryRange(const char *file, int line,
                                           const volatile void *address,
                                           long size) {}
extern "C" void AnnotatePCQCreate(const char *file, int line,
                                  const volatile void *pcq) {}
extern "C" void AnnotatePCQDestroy(const char *file, int line,
                                   const volatile void *pcq) {}
extern "C" void AnnotatePCQPut(const char *file, int line,
                               const volatile void *pcq) {}
extern "C" void AnnotatePCQGet(const char *file, int line,
                               const volatile void *pcq) {}
extern "C" void AnnotateNewMemory(const char *file, int line,
                                  const volatile void *mem,
                                  long size) {}
extern "C" void AnnotateExpectRace(const char *file, int line,
                                   const volatile void *mem,
                                   const char *description) {}
extern "C" void AnnotateBenignRace(const char *file, int line,
                                   const volatile void *mem,
                                   const char *description) {}
extern "C" void AnnotateMutexIsUsedAsCondVar(const char *file, int line,
                                            const volatile void *mu) {}
extern "C" void AnnotateTraceMemory(const char *file, int line,
                                    const volatile void *arg) {}
extern "C" void AnnotateThreadName(const char *file, int line,
                                   const char *name) {}
extern "C" void AnnotateIgnoreReadsBegin(const char *file, int line) {}
extern "C" void AnnotateIgnoreReadsEnd(const char *file, int line) {}
extern "C" void AnnotateIgnoreWritesBegin(const char *file, int line) {}
extern "C" void AnnotateIgnoreWritesEnd(const char *file, int line) {}
extern "C" void AnnotateNoOp(const char *file, int line,
                             const volatile void *arg) {}
#endif // NDEBUG

// When running under valgrind, a non-zero value will be returned.
extern "C" int RunningOnValgrind() {
#if defined(NVALGRIND)
  return 0;
#else
  return RUNNING_ON_VALGRIND;
#endif
}
