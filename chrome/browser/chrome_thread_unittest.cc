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

#include <vector>

#include "chrome/browser/chrome_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ChromeThreadTest, Get) {
  scoped_ptr<ChromeThread> io_thread;
  scoped_ptr<ChromeThread> file_thread;
  scoped_ptr<ChromeThread> db_thread;
  scoped_ptr<ChromeThread> history_thread;

  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::IO) == NULL);
  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::FILE) == NULL);
  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::DB) == NULL);
  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::HISTORY) == NULL);

  // Phase 1: Create threads.

  io_thread.reset(new ChromeThread(ChromeThread::IO));
  file_thread.reset(new ChromeThread(ChromeThread::FILE));
  db_thread.reset(new ChromeThread(ChromeThread::DB));
  history_thread.reset(new ChromeThread(ChromeThread::HISTORY));

  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::IO) == NULL);
  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::FILE) == NULL);
  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::DB) == NULL);
  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::HISTORY) == NULL);

  // Phase 2: Start the threads.

  io_thread->Start();
  file_thread->Start();
  db_thread->Start();
  history_thread->Start();

  EXPECT_TRUE(io_thread->message_loop() != NULL);
  EXPECT_TRUE(file_thread->message_loop() != NULL);
  EXPECT_TRUE(db_thread->message_loop() != NULL);
  EXPECT_TRUE(history_thread->message_loop() != NULL);

  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::IO) ==
              io_thread->message_loop());
  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::FILE) ==
              file_thread->message_loop());
  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::DB) ==
              db_thread->message_loop());
  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::HISTORY) ==
              history_thread->message_loop());

  // Phase 3: Stop the threads.

  io_thread->Stop();
  file_thread->Stop();
  db_thread->Stop();
  history_thread->Stop();

  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::IO) == NULL);
  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::FILE) == NULL);
  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::DB) == NULL);
  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::HISTORY) == NULL);

  // Phase 4: Destroy the threads.

  io_thread.reset();
  file_thread.reset();
  db_thread.reset();
  history_thread.reset();

  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::IO) == NULL);
  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::FILE) == NULL);
  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::DB) == NULL);
  EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::HISTORY) == NULL);
}
