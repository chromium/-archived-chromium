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

#include <windows.h>
#include <process.h>

#include "base/thread_local_storage.h"
#include "testing/gtest/include/gtest/gtest.h"

// ignore warnings about ptr->int conversions that we use when
// storing ints into ThreadLocalStorage.
#pragma warning(disable : 4311 4312)

namespace {
  class ThreadLocalStorageTest : public testing::Test {
  };
}


TEST(ThreadLocalStorageTest, Basics) {
  int index = ThreadLocalStorage::Alloc();
  ThreadLocalStorage::Set(index, reinterpret_cast<void*>(123));
  int value = reinterpret_cast<int>(ThreadLocalStorage::Get(index));
  EXPECT_EQ(value, 123);
}

const int kInitialTlsValue = 0x5555;
static int tls_index = 0;

unsigned __stdcall TLSTestThreadMain(void* param) {
  // param contains the thread local storage index.
  int *index = reinterpret_cast<int*>(param);
  *index = kInitialTlsValue;

  ThreadLocalStorage::Set(tls_index, index);

  int *ptr = static_cast<int*>(ThreadLocalStorage::Get(tls_index));
  EXPECT_EQ(ptr, index);
  EXPECT_EQ(*ptr, kInitialTlsValue);
  *index = 0;

  ptr = static_cast<int*>(ThreadLocalStorage::Get(tls_index));
  EXPECT_EQ(ptr, index);
  EXPECT_EQ(*ptr, 0);
  return 0;
}

void ThreadLocalStorageCleanup(void *value) {
  int *ptr = reinterpret_cast<int*>(value);
  if (ptr)
    *ptr = kInitialTlsValue;
}


TEST(ThreadLocalStorageTest, TLSDestructors) {
  // Create a TLS index with a destructor.  Create a set of
  // threads that set the TLS, while the destructor cleans it up.
  // After the threads finish, verify that the value is cleaned up.
  const int kNumThreads = 5;
  HANDLE threads[kNumThreads];
  int values[kNumThreads];

  tls_index = ThreadLocalStorage::Alloc(ThreadLocalStorageCleanup);

  // Spawn the threads.
  for (int16 index = 0; index < kNumThreads; index++) {
    values[index] = kInitialTlsValue;
    void *argument = static_cast<void*>(&(values[index]));
    unsigned thread_id;
    threads[index] = reinterpret_cast<HANDLE>(
      _beginthreadex(NULL, 0, TLSTestThreadMain, argument, 0, &thread_id));
    EXPECT_NE(threads[index], (HANDLE)NULL);
  }

  // Wait for the threads to finish.
  for (int index = 0; index < kNumThreads; index++) {
    DWORD rv = WaitForSingleObject(threads[index], 60*1000);
    EXPECT_EQ(rv, WAIT_OBJECT_0);  // verify all threads finished

    // verify that the destructor was called and that we reset.
    EXPECT_EQ(values[index], kInitialTlsValue);
  }
}
