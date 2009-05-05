// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/path_service.h"
#include "base/time.h"
#include "chrome/browser/sessions/session_backend.h"
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
    std::string b = Int64ToString(base::Time::Now().ToInternalValue());

    PathService::Get(base::DIR_TEMP, &path_);
    path_ = path_.Append(FILE_PATH_LITERAL("SessionTestDirs"));
    file_util::CreateDirectory(path_);
    path_ = path_.AppendASCII(b);
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
  FilePath path_;
};

TEST_F(SessionBackendTest, SimpleReadWrite) {
  scoped_refptr<SessionBackend> backend(
      new SessionBackend(BaseSessionService::SESSION_RESTORE, path_));
  struct TestData data = { 1,  "a" };
  std::vector<SessionCommand*> commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(new SessionCommands(commands), false);
  commands.clear();

  // Read it back in.
  backend = NULL;
  backend = new SessionBackend(BaseSessionService::SESSION_RESTORE, path_);
  backend->ReadLastSessionCommandsImpl(&commands);

  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(data, commands[0]);

  STLDeleteElements(&commands);

  backend = NULL;
  backend = new SessionBackend(BaseSessionService::SESSION_RESTORE, path_);
  backend->ReadLastSessionCommandsImpl(&commands);

  ASSERT_EQ(0U, commands.size());

  // Make sure we can delete.
  backend->DeleteLastSession();
  backend->ReadLastSessionCommandsImpl(&commands);
  ASSERT_EQ(0U, commands.size());
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

  for (size_t i = 0; i < arraysize(data); ++i) {
    scoped_refptr<SessionBackend> backend(
        new SessionBackend(BaseSessionService::SESSION_RESTORE, path_));
    std::vector<SessionCommand*> commands;
    if (i != 0) {
      // Read previous data.
      backend->ReadLastSessionCommandsImpl(&commands);
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

  scoped_refptr<SessionBackend> backend(
      new SessionBackend(BaseSessionService::SESSION_RESTORE, path_));
  std::vector<SessionCommand*> commands;
  commands.push_back(CreateCommandFromData(data[0]));
  const SessionCommand::size_type big_size =
      SessionBackend::kFileReadBufferSize + 100;
  const SessionCommand::id_type big_id = 50;
  SessionCommand* big_command = new SessionCommand(big_id, big_size);
  reinterpret_cast<char*>(big_command->contents())[0] = 'a';
  reinterpret_cast<char*>(big_command->contents())[big_size - 1] = 'z';
  commands.push_back(big_command);
  commands.push_back(CreateCommandFromData(data[1]));
  backend->AppendCommands(new SessionCommands(commands), false);
  commands.clear();

  backend = NULL;
  backend = new SessionBackend(BaseSessionService::SESSION_RESTORE, path_);
  commands.clear();
  backend->ReadLastSessionCommandsImpl(&commands);
  ASSERT_EQ(3U, commands.size());
  AssertCommandEqualsData(data[0], commands[0]);
  AssertCommandEqualsData(data[1], commands[2]);

  EXPECT_EQ(big_id, commands[1]->id());
  ASSERT_EQ(big_size, commands[1]->size());
  EXPECT_EQ('a', reinterpret_cast<char*>(commands[1]->contents())[0]);
  EXPECT_EQ('z',
            reinterpret_cast<char*>(commands[1]->contents())[big_size - 1]);
  STLDeleteElements(&commands);
}

TEST_F(SessionBackendTest, EmptyCommand) {
  TestData empty_command;
  empty_command.command_id = 1;
  scoped_refptr<SessionBackend> backend(
      new SessionBackend(BaseSessionService::SESSION_RESTORE, path_));
  std::vector<SessionCommand*>* empty_commands =
      new std::vector<SessionCommand*>();
  empty_commands->push_back(CreateCommandFromData(empty_command));
  backend->AppendCommands(empty_commands, true);
  backend->MoveCurrentSessionToLastSession();

  std::vector<SessionCommand*> commands;
  backend->ReadLastSessionCommandsImpl(&commands);
  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(empty_command, commands[0]);
  STLDeleteElements(&commands);
}

// Writes a command, appends another command with reset to true, then reads
// making sure we only get back the second command.
TEST_F(SessionBackendTest, Truncate) {
  scoped_refptr<SessionBackend> backend(
      new SessionBackend(BaseSessionService::SESSION_RESTORE, path_));
  struct TestData first_data = { 1,  "a" };
  std::vector<SessionCommand*> commands;
  commands.push_back(CreateCommandFromData(first_data));
  backend->AppendCommands(new SessionCommands(commands), false);
  commands.clear();

  // Write another command, this time resetting the file when appending.
  struct TestData second_data = { 2,  "b" };
  commands.push_back(CreateCommandFromData(second_data));
  backend->AppendCommands(new SessionCommands(commands), true);
  commands.clear();

  // Read it back in.
  backend = NULL;
  backend = new SessionBackend(BaseSessionService::SESSION_RESTORE, path_);
  backend->ReadLastSessionCommandsImpl(&commands);

  // And make sure we get back the expected data.
  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(second_data, commands[0]);

  STLDeleteElements(&commands);
}
