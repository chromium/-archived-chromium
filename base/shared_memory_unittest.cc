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

#include <process.h>  // _beginthreadex
#include "base/shared_memory.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace {

class SharedMemoryTest : public testing::Test {
};

unsigned __stdcall MultipleThreadMain(void* param) {
  // Each thread will open the shared memory.  Each thread will take
  // a different 4 byte int pointer, and keep changing it, with some
  // small pauses in between.  Verify that each thread's value in the
  // shared memory is always correct.
  const int kDataSize = 1024;
  std::wstring test_name = L"SharedMemoryOpenThreadTest";
  int16 id = reinterpret_cast<int16>(param);
  SharedMemory memory;
  bool rv = memory.Create(test_name, false, true, kDataSize);
  EXPECT_TRUE(rv);
  rv = memory.Map(kDataSize);
  EXPECT_TRUE(rv);
  int *ptr = static_cast<int*>(memory.memory()) + id;
  EXPECT_EQ(*ptr, 0);
  for (int idx = 0; idx < 100; idx++) {
    *ptr = idx;
    Sleep(1);  // short wait
    EXPECT_EQ(*ptr, idx);
  }
  memory.Close();
  return 0;
}

unsigned __stdcall MultipleLockThread(void* param) {
  // Each thread will open the shared memory.  Each thread will take
  // the memory, and keep changing it while trying to lock it, with some
  // small pauses in between.  Verify that each thread's value in the
  // shared memory is always correct.
  const int kDataSize = sizeof(int);
  int id = static_cast<int>(reinterpret_cast<INT_PTR>(param));
  SharedMemoryHandle handle = NULL;
  {
    SharedMemory memory1;
    EXPECT_TRUE(memory1.Create(L"SharedMemoryMultipleLockThreadTest", false, true,
                              kDataSize));
    EXPECT_TRUE(memory1.ShareToProcess(GetCurrentProcess(), &handle));
  }
  SharedMemory memory2(handle, false);
  EXPECT_TRUE(memory2.Map(kDataSize));
  volatile int* const ptr = static_cast<int*>(memory2.memory());
  for (int idx = 0; idx < 20; idx++) {
    memory2.Lock();
    int i = (id << 16) + idx;
    *ptr = i;
    // short wait
    Sleep(1);
    EXPECT_EQ(*ptr, i);
    memory2.Unlock();
  }
  memory2.Close();
  return 0;
}

}  // namespace

TEST(SharedMemoryTest, OpenClose) {
  const int kDataSize = 1024;
  std::wstring test_name = L"SharedMemoryOpenCloseTest";

  // Open two handles to a memory segment, confirm that they
  // are mapped separately yet point to the same space.
  SharedMemory memory1;
  bool rv = memory1.Open(test_name, false);
  EXPECT_FALSE(rv);
  rv = memory1.Create(test_name, false, false, kDataSize);
  EXPECT_TRUE(rv);
  rv = memory1.Map(kDataSize);
  EXPECT_TRUE(rv);
  SharedMemory memory2;
  rv = memory2.Open(test_name, false);
  EXPECT_TRUE(rv);
  rv = memory2.Map(kDataSize);
  EXPECT_TRUE(rv);
  EXPECT_NE(memory1.memory(), memory2.memory());  // compare the pointers


  // Write data to the first memory segment, verify contents of second.
  memset(memory1.memory(), '1', kDataSize);
  EXPECT_EQ(memcmp(memory1.memory(), memory2.memory(), kDataSize), 0);

  // Close the first memory segment, and verify the
  // second still has the right data.
  memory1.Close();
  char *start_ptr = static_cast<char *>(memory2.memory());
  char *end_ptr = start_ptr + kDataSize;
  for (char* ptr = start_ptr; ptr < end_ptr; ptr++)
    EXPECT_EQ(*ptr, '1');

  // Close the second memory segment
  memory2.Close();
}


TEST(SharedMemoryTest, MultipleThreads) {
  // Create a set of 5 threads to each open a shared memory segment
  // and write to it.  Verify that they are always reading/writing
  // consistent data.
  const int kNumThreads = 5;
  HANDLE threads[kNumThreads];

  // Spawn the threads.
  for (int16 index = 0; index < kNumThreads; index++) {
    void *argument = reinterpret_cast<void*>(index);
    unsigned thread_id;
    threads[index] = reinterpret_cast<HANDLE>(
      _beginthreadex(NULL, 0, MultipleThreadMain, argument, 0, &thread_id));
    EXPECT_NE(threads[index], static_cast<HANDLE>(NULL));
  }

  // Wait for the threads to finish.
  for (int index = 0; index < kNumThreads; index++) {
    DWORD rv = WaitForSingleObject(threads[index], 60*1000);
    EXPECT_EQ(rv, WAIT_OBJECT_0);  // verify all threads finished
    CloseHandle(threads[index]);
  }
}


TEST(SharedMemoryTest, Lock) {
  // Create a set of threads to each open a shared memory segment and write to
  // it with the lock held. Verify that they are always reading/writing
  // consistent data.
  const int kNumThreads = 5;
  HANDLE threads[kNumThreads];

  // Spawn the threads.
  for (int index = 0; index < kNumThreads; ++index) {
    void *argument = reinterpret_cast<void*>(static_cast<INT_PTR>(index));
    threads[index] = reinterpret_cast<HANDLE>(
      _beginthreadex(NULL, 0, &MultipleLockThread, argument, 0, NULL));
    EXPECT_NE(threads[index], static_cast<HANDLE>(NULL));
  }

  // Wait for the threads to finish.
  for (int index = 0; index < kNumThreads; ++index) {
    DWORD rv = WaitForSingleObject(threads[index], 60*1000);
    EXPECT_EQ(rv, WAIT_OBJECT_0);  // verify all threads finished
    CloseHandle(threads[index]);
  }
}
