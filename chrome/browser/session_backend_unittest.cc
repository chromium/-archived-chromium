// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/time.h"
#include "chrome/browser/session_backend.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef std::vector<SessionCommand*> SessionCommands;

struct TestData {
  SessionCommand::id_type command_id;
  std::string data;
};

SessionCommand* CreateCommandFromData(const TestData& data) {
  SessionCommand* command =
      new SessionCommand(
          data.command_id,
          static_cast<SessionCommand::size_type>(data.data.size()));
  if (!data.data.empty())
    memcpy(command->contents(), data.data.c_str(), data.data.size());
  return command;
}

}  // namespace

class SessionBackendTest : public testing::Test {
 protected:
  virtual void SetUp() {
    wchar_t b[32];
    _itow_s(static_cast<int>(GetTickCount()), b, arraysize(b), 10);

    PathService::Get(base::DIR_TEMP, &path_);
    file_util::AppendToPath(&path_, L"SessionTestDirs");
    file_util::CreateDirectory(path_);
    file_util::AppendToPath(&path_, b);
  }

  virtual void TearDown() {
    file_util::Delete(path_, true);
  }

  void AssertCommandEqualsData(const TestData& data, SessionCommand* command) {
    EXPECT_EQ(data.command_id, command->id());
    EXPECT_EQ(data.data.size(), command->size());
    EXPECT_TRUE(
        memcmp(command->contents(), data.data.c_str(), command->size()) == 0);
  }

  // Path used in testing.
  std::wstring path_;
};

TEST_F(SessionBackendTest, SimpleReadWrite) {
  scoped_refptr<SessionBackend> backend(new SessionBackend(path_));
  struct TestData data = { 1,  "a" };
  std::vector<SessionCommand*> commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(new SessionCommands(commands), false);
  commands.clear();

  // Read it back in.
  backend = NULL;
  backend = new SessionBackend(path_);
  backend->ReadSessionImpl(false, &commands);

  ASSERT_EQ(1, commands.size());
  AssertCommandEqualsData(data, commands[0]);

  STLDeleteElements(&commands);

  backend = NULL;
  backend = new SessionBackend(path_);
  backend->ReadSessionImpl(false, &commands);

  ASSERT_EQ(0, commands.size());

  // Make sure we can delete.
  backend->DeleteSession(false);
  backend->ReadSessionImpl(false, &commands);
  ASSERT_EQ(0, commands.size());
}

TEST_F(SessionBackendTest, RandomData) {
  struct TestData data[] = {
    { 1,  "a" },
    { 2,  "ab" },
    { 3,  "abc" },
    { 4,  "abcd" },
    { 5,  "abcde" },
    { 6,  "abcdef" },
    { 7,  "abcdefg" },
    { 8,  "abcdefgh" },
    { 9,  "abcdefghi" },
    { 10, "abcdefghij" },
    { 11, "abcdefghijk" },
    { 12, "abcdefghijkl" },
    { 13, "abcdefghijklm" },
  };

  for (int i = 0; i < arraysize(data); ++i) {
    scoped_refptr<SessionBackend> backend(new SessionBackend(path_));
    std::vector<SessionCommand*> commands;
    if (i != 0) {
      // Read previous data.
      backend->ReadSessionImpl(false, &commands);
      ASSERT_EQ(i, commands.size());
      for (std::vector<SessionCommand*>::iterator j = commands.begin();
           j != commands.end(); ++j) {
        AssertCommandEqualsData(data[j - commands.begin()], *j);
      }
      backend->AppendCommands(new SessionCommands(commands), false);
      commands.clear();
    }
    commands.push_back(CreateCommandFromData(data[i]));
    backend->AppendCommands(new SessionCommands(commands), false);
  }
}

TEST_F(SessionBackendTest, BigData) {
  struct TestData data[] = {
    { 1,  "a" },
    { 2,  "ab" },
  };

  scoped_refptr<SessionBackend> backend(new SessionBackend(path_));
  std::vector<SessionCommand*> commands;
  commands.push_back(CreateCommandFromData(data[0]));
  const SessionCommand::size_type big_size = SessionBackend::kFileReadBufferSize + 100;
  const SessionCommand::id_type big_id = 50;
  SessionCommand* big_command = new SessionCommand(big_id, big_size);
  reinterpret_cast<char*>(big_command->contents())[0] = 'a';
  reinterpret_cast<char*>(big_command->contents())[big_size - 1] = 'z';
  commands.push_back(big_command);
  commands.push_back(CreateCommandFromData(data[1]));
  backend->AppendCommands(new SessionCommands(commands), false);
  commands.clear();

  backend = NULL;
  backend = new SessionBackend(path_);
  commands.clear();
  backend->ReadSessionImpl(false, &commands);
  ASSERT_EQ(3, commands.size());
  AssertCommandEqualsData(data[0], commands[0]);
  AssertCommandEqualsData(data[1], commands[2]);

  EXPECT_EQ(big_id, commands[1]->id());
  ASSERT_EQ(big_size, commands[1]->size());
  EXPECT_EQ('a', reinterpret_cast<char*>(commands[1]->contents())[0]);
  EXPECT_EQ('z',
            reinterpret_cast<char*>(commands[1]->contents())[big_size - 1]);
  STLDeleteElements(&commands);
}

TEST_F(SessionBackendTest, SaveSession) {
  struct TestData data[] = {
    { 1,  "a" },
    { 2,  "ab" },
  };

  scoped_refptr<SessionBackend> backend(new SessionBackend(path_));
  std::vector<SessionCommand*> commands;
  for (int i = 0; i < arraysize(data); ++i) {
    commands.push_back(CreateCommandFromData(data[i]));
  }
  backend->SaveSession(commands);

  commands.clear();

  backend->ReadSessionImpl(true, &commands);
  ASSERT_EQ(arraysize(data), commands.size());
  for (int i = 0; i < arraysize(data); ++i)
    AssertCommandEqualsData(data[i], commands[i]);
  STLDeleteElements(&commands);
}

TEST_F(SessionBackendTest, EmptyCommand) {
  TestData empty_command;
  empty_command.command_id = 1;
  scoped_refptr<SessionBackend> backend(new SessionBackend(path_));
  std::vector<SessionCommand*> commands;
  commands.push_back(CreateCommandFromData(empty_command));
  backend->SaveSession(commands);

  commands.clear();

  backend->ReadSessionImpl(true, &commands);
  ASSERT_EQ(1, commands.size());
  AssertCommandEqualsData(empty_command, commands[0]);
  STLDeleteElements(&commands);
}

