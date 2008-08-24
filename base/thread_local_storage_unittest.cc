// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  ThreadLocalStorage::Slot slot;
  slot.Set(reinterpret_cast<void*>(123));
  int value = reinterpret_cast<int>(slot.Get());
  EXPECT_EQ(value, 123);
}

const int kInitialTlsValue = 0x5555;
static ThreadLocalStorage::Slot tls_slot(base::LINKER_INITIALIZED);

unsigned __stdcall TLSTestThreadMain(void* param) {
  // param contains the thread local storage index.
  int *index = reinterpret_cast<int*>(param);
  *index = kInitialTlsValue;

  tls_slot.Set(index);

  int *ptr = static_cast<int*>(tls_slot.Get());
  EXPECT_EQ(ptr, index);
  EXPECT_EQ(*ptr, kInitialTlsValue);
  *index = 0;

  ptr = static_cast<int*>(tls_slot.Get());
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

  tls_slot.Initialize(ThreadLocalStorageCleanup);

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

