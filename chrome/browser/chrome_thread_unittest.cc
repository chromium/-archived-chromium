// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

