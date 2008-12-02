// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_TESTS_H__
#define CHROME_COMMON_IPC_TESTS_H__

// This unit test uses 3 types of child processes, a regular pipe client,
// a client reflector and a IPC server used for fuzzing tests.
enum ChildType {
  TEST_CLIENT,
  TEST_REFLECTOR,
  FUZZER_SERVER
};

// The different channel names for the child processes.
extern const wchar_t kTestClientChannel[];
extern const wchar_t kReflectorChannel[];
extern const wchar_t kFuzzerChannel[];

// Spawns a child process and then runs the code for one of the 3 possible
// child modes.
HANDLE SpawnChild(ChildType child_type);

// Runs the fuzzing server child mode. Returns true when the preset number
// of messages have been received.
bool RunFuzzServer();

#endif  // CHROME_COMMON_IPC_TESTS_H__

