// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/thread_local_storage.h"

#include <windows.h>

#include "base/logging.h"

// In order to make TLS destructors work, we need to keep function
// pointers to the destructor for each TLS that we allocate.
// We make this work by allocating a single OS-level TLS, which
// contains an array of slots for the application to use.  In
// parallel, we also allocate an array of destructors, which we
// keep track of and call when threads terminate.

// tls_key_ is the one native TLS that we use.  It stores our
// table.
long ThreadLocalStorage::tls_key_ = TLS_OUT_OF_INDEXES;

// tls_max_ is the high-water-mark of allocated thread local storage.
// We intentionally skip 0 so that it is not confused with an
// unallocated TLS slot.
long ThreadLocalStorage::tls_max_ = 1;

// An array of destructor function pointers for the slots.  If
// a slot has a destructor, it will be stored in its corresponding
// entry in this array.
ThreadLocalStorage::TLSDestructorFunc
  ThreadLocalStorage::tls_destructors_[kThreadLocalStorageSize];

void **ThreadLocalStorage::Initialize() {
  if (tls_key_ == TLS_OUT_OF_INDEXES) {
    long value = TlsAlloc();
    DCHECK(value != TLS_OUT_OF_INDEXES);

    // Atomically test-and-set the tls_key.  If the key is TLS_OUT_OF_INDEXES,
    // go ahead and set it.  Otherwise, do nothing, as another
    // thread already did our dirty work.
    if (InterlockedCompareExchange(&tls_key_, value, TLS_OUT_OF_INDEXES) !=
            TLS_OUT_OF_INDEXES) {
      // We've been shortcut. Another thread replaced tls_key_ first so we need
      // to destroy our index and use the one the other thread got first.
      TlsFree(value);
    }
  }
  DCHECK(TlsGetValue(tls_key_) == NULL);

  // Create an array to store our data.
  void **tls_data = new void*[kThreadLocalStorageSize];
  memset(tls_data, 0, sizeof(void*[kThreadLocalStorageSize]));
  TlsSetValue(tls_key_, tls_data);
  return tls_data;
}

TLSSlot ThreadLocalStorage::Alloc(TLSDestructorFunc destructor) {
  if (tls_key_ == TLS_OUT_OF_INDEXES || !TlsGetValue(tls_key_))
    Initialize();

  // Grab a new slot.
  int slot = InterlockedIncrement(&tls_max_) - 1;
  if (slot >= kThreadLocalStorageSize) {
    NOTREACHED();
    return -1;
  }

  // Setup our destructor.
  tls_destructors_[slot] = destructor;
  return slot;
}

void ThreadLocalStorage::Free(TLSSlot slot) {
  // At this time, we don't reclaim old indices for TLS slots.
  // So all we need to do is wipe the destructor.
  tls_destructors_[slot] = NULL;
}

void* ThreadLocalStorage::Get(TLSSlot slot) {
  void **tls_data = static_cast<void**>(TlsGetValue(tls_key_));
  if (!tls_data)
    tls_data = Initialize();
  DCHECK(slot >= 0 && slot < kThreadLocalStorageSize);
  return tls_data[slot];
}

void ThreadLocalStorage::Set(TLSSlot slot, void* value) {
  void **tls_data = static_cast<void**>(TlsGetValue(tls_key_));
  if (!tls_data)
    tls_data = Initialize();
  DCHECK(slot >= 0 && slot < kThreadLocalStorageSize);
  tls_data[slot] = value;
}

void ThreadLocalStorage::ThreadExit() {
  void **tls_data = static_cast<void**>(TlsGetValue(tls_key_));

  // Maybe we have never initialized TLS for this thread.
  if (!tls_data)
    return;

  for (int slot = 0; slot < tls_max_; slot++) {
    if (tls_destructors_[slot] != NULL) {
      void *value = tls_data[slot];
      tls_destructors_[slot](value);
    }
  }

  delete[] tls_data;

  // In case there are other "onexit" handlers...
  TlsSetValue(tls_key_, NULL);
}

// Thread Termination Callbacks.
// Windows doesn't support a per-thread destructor with its
// TLS primitives.  So, we build it manually by inserting a
// function to be called on each thread's exit.
// This magic is from http://www.codeproject.com/threads/tls.asp
// and it works for VC++ 7.0 and later.

#ifdef _WIN64

// This makes the linker create the TLS directory if it's not already
// there.  (e.g. if __declspec(thread) is not used).
#pragma comment(linker, "/INCLUDE:_tls_used")

#else  // _WIN64

// This makes the linker create the TLS directory if it's not already
// there.  (e.g. if __declspec(thread) is not used).
#pragma comment(linker, "/INCLUDE:__tls_used")

#endif  // _WIN64

// Static callback function to call with each thread termination.
void NTAPI OnThreadExit(PVOID module, DWORD reason, PVOID reserved)
{
  // On XP SP0 & SP1, the DLL_PROCESS_ATTACH is never seen. It is sent on SP2+
  // and on W2K and W2K3. So don't assume it is sent.
  if (DLL_THREAD_DETACH == reason || DLL_PROCESS_DETACH == reason)
    ThreadLocalStorage::ThreadExit();
}

// .CRT$XLA to .CRT$XLZ is an array of PIMAGE_TLS_CALLBACK pointers that are
// called automatically by the OS loader code (not the CRT) when the module is
// loaded and on thread creation. They are NOT called if the module has been
// loaded by a LoadLibrary() call. It must have implicitly been loaded at
// process startup.
// By implicitly loaded, I mean that it is directly referenced by the main EXE
// or by one of its dependent DLLs. Delay-loaded DLL doesn't count as being
// implicitly loaded.
//
// See VC\crt\src\tlssup.c for reference.
#ifdef _WIN64

// .CRT section is merged with .rdata on x64 so it must be constant data.
#pragma const_seg(".CRT$XLB")
const PIMAGE_TLS_CALLBACK p_thread_callback = OnThreadExit;

// Reset the default section.
#pragma const_seg()

#else  // _WIN64

#pragma data_seg(".CRT$XLB")
PIMAGE_TLS_CALLBACK p_thread_callback = OnThreadExit;

// Reset the default section.
#pragma data_seg()

#endif  // _WIN64