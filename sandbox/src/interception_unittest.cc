// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains unit tests for InterceptionManager.
// The tests require private information so the whole interception.cc file is
// included from this file.

#include "base/scoped_ptr.h"
#include "sandbox/src/interception.cc"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// Walks the settings buffer, verifying that the values make sense and counting
// objects.
// Arguments:
// buffer (in): the buffer to walk.
// size (in): buffer size
// num_dlls (out): count of the dlls on the buffer.
// num_function (out): count of intercepted functions.
// num_names (out): count of named interceptor functions.
void WalkBuffer(void* buffer, size_t size, int* num_dlls, int* num_functions,
                int* num_names) {
  ASSERT_TRUE(NULL != buffer);
  ASSERT_TRUE(NULL != num_functions);
  ASSERT_TRUE(NULL != num_names);
  *num_dlls = *num_functions = *num_names = 0;
  SharedMemory *memory = reinterpret_cast<SharedMemory*>(buffer);

  ASSERT_GT(size, sizeof(SharedMemory));
  DllPatchInfo *dll = &memory->dll_list[0];

  for (int i = 0; i < memory->num_intercepted_dlls; i++) {
    ASSERT_NE(0, wcslen(dll->dll_name));
    ASSERT_EQ(0, dll->record_bytes % sizeof(size_t));
    ASSERT_EQ(0, dll->offset_to_functions % sizeof(size_t));
    ASSERT_NE(0, dll->num_functions);

    FunctionInfo *function = reinterpret_cast<FunctionInfo*>(
      reinterpret_cast<char*>(dll) + dll->offset_to_functions);

    for (int j = 0; j < dll->num_functions; j++) {
      ASSERT_EQ(0, function->record_bytes % sizeof(size_t));

      char* name = function->function;
      size_t length = strlen(name);
      ASSERT_NE(0, length);
      name += length + 1;

      // look for overflows
      ASSERT_GT(reinterpret_cast<char*>(buffer) + size, name + strlen(name));

      // look for a named interceptor
      if (strlen(name)) {
        (*num_names)++;
        EXPECT_TRUE(NULL == function->interceptor_address);
      } else {
        EXPECT_TRUE(NULL != function->interceptor_address);
      }

      (*num_functions)++;
      function = reinterpret_cast<FunctionInfo*>(
        reinterpret_cast<char*>(function) + function->record_bytes);
    }

    (*num_dlls)++;
    dll = reinterpret_cast<DllPatchInfo*>(reinterpret_cast<char*>(dll) +
                                          dll->record_bytes);
  }
}

TEST(InterceptionManagerTest, BufferLayout1) {
  wchar_t exe_name[MAX_PATH];
  ASSERT_NE(0, GetModuleFileName(NULL, exe_name, MAX_PATH - 1));

  TargetProcess *target = MakeTestTargetProcess(::GetCurrentProcess(),
                                                ::GetModuleHandle(exe_name));

  InterceptionManager interceptions(target, true);

  // Any pointer will do for a function pointer.
  void* function = &interceptions;

  interceptions.AddToPatchedFunctions(L"ntdll.dll", "NtCreateFile",
    INTERCEPTION_SERVICE_CALL, function);
  interceptions.AddToPatchedFunctions(L"kernel32.dll", "CreateFileEx",
    INTERCEPTION_EAT, function);
  interceptions.AddToPatchedFunctions(L"kernel32.dll", "SomeFileEx",
    INTERCEPTION_SMART_SIDESTEP, function);
  interceptions.AddToPatchedFunctions(L"user32.dll", "FindWindow",
    INTERCEPTION_EAT, function);
  interceptions.AddToPatchedFunctions(L"kernel32.dll", "CreateMutex",
    INTERCEPTION_EAT, function);
  interceptions.AddToPatchedFunctions(L"user32.dll", "PostMsg",
    INTERCEPTION_EAT, function);
  interceptions.AddToPatchedFunctions(L"user32.dll", "PostMsg",
    INTERCEPTION_EAT, "replacement");
  interceptions.AddToPatchedFunctions(L"comctl.dll", "SaveAsDlg",
    INTERCEPTION_EAT, function);
  interceptions.AddToPatchedFunctions(L"ntdll.dll", "NtClose",
    INTERCEPTION_SERVICE_CALL, function);
  interceptions.AddToPatchedFunctions(L"ntdll.dll", "NtOpenFile",
    INTERCEPTION_SIDESTEP, function);
  interceptions.AddToPatchedFunctions(L"some.dll", "Superfn",
    INTERCEPTION_EAT, function);
  interceptions.AddToPatchedFunctions(L"comctl.dll", "SaveAsDlg",
    INTERCEPTION_EAT, "a");
  interceptions.AddToPatchedFunctions(L"comctl.dll", "SaveAsDlg",
    INTERCEPTION_SIDESTEP, "ab");
  interceptions.AddToPatchedFunctions(L"comctl.dll", "SaveAsDlg",
    INTERCEPTION_EAT, "abc");
  interceptions.AddToPatchedFunctions(L"a.dll", "p",
    INTERCEPTION_EAT, function);
  interceptions.AddToPatchedFunctions(L"b.dll",
    "TheIncredibleCallToSaveTheWorld", INTERCEPTION_EAT, function);
  interceptions.AddToPatchedFunctions(L"a.dll", "BIsLame",
    INTERCEPTION_EAT, function);
  interceptions.AddToPatchedFunctions(L"a.dll", "ARules",
    INTERCEPTION_EAT, function);

  // Verify that all interceptions were added
  ASSERT_EQ(18, interceptions.interceptions_.size());

  size_t buffer_size = interceptions.GetBufferSize();
  scoped_ptr<BYTE> local_buffer(new BYTE[buffer_size]);

  ASSERT_TRUE(interceptions.SetupConfigBuffer(local_buffer.get(),
                                              buffer_size));

  // At this point, the interceptions should have been separated into two
  // groups: one group with the local ("cold") interceptions, consisting of
  // everything from ntdll and stuff set as INTRECEPTION_SERVICE_CALL, and
  // another group with the interceptions belonging to dlls that will be "hot"
  // patched on the client. The second group lives on local_buffer, and the
  // first group remains on the list of interceptions (inside the object
  // "interceptions"). There are 3 local interceptions (of ntdll); the
  // other 15 have to be sent to the child to be performed "hot".
  EXPECT_EQ(3, interceptions.interceptions_.size());

  int num_dlls, num_functions, num_names;
  WalkBuffer(local_buffer.get(), buffer_size, &num_dlls, &num_functions,
             &num_names);

  // The 15 interceptions on the buffer (to the child) should be grouped on 6
  // dlls. Only four interceptions are using an explicit name for the
  // interceptor function.
  EXPECT_EQ(6, num_dlls);
  EXPECT_EQ(15, num_functions);
  EXPECT_EQ(4, num_names);
}

TEST(InterceptionManagerTest, BufferLayout2) {
  wchar_t exe_name[MAX_PATH];
  ASSERT_NE(0, GetModuleFileName(NULL, exe_name, MAX_PATH - 1));

  TargetProcess *target = MakeTestTargetProcess(::GetCurrentProcess(),
                                                ::GetModuleHandle(exe_name));

  InterceptionManager interceptions(target, true);

  // Any pointer will do for a function pointer.
  void* function = &interceptions;

  interceptions.AddToUnloadModules(L"some01.dll");
  interceptions.AddToPatchedFunctions(L"ntdll.dll", "NtCreateFile",
    INTERCEPTION_SERVICE_CALL, function);
  interceptions.AddToPatchedFunctions(L"kernel32.dll", "CreateFileEx",
    INTERCEPTION_EAT, function);
  interceptions.AddToUnloadModules(L"some02.dll");
  interceptions.AddToPatchedFunctions(L"kernel32.dll", "SomeFileEx",
    INTERCEPTION_SMART_SIDESTEP, function);

  // Verify that all interceptions were added
  ASSERT_EQ(5, interceptions.interceptions_.size());

  size_t buffer_size = interceptions.GetBufferSize();
  scoped_ptr<BYTE> local_buffer(new BYTE[buffer_size]);

  ASSERT_TRUE(interceptions.SetupConfigBuffer(local_buffer.get(),
                                              buffer_size));

  // At this point, the interceptions should have been separated into two
  // groups: one group with the local ("cold") interceptions, and another
  // group with the interceptions belonging to dlls that will be "hot"
  // patched on the client. The second group lives on local_buffer, and the
  // first group remains on the list of interceptions, in this case just one.
  EXPECT_EQ(1, interceptions.interceptions_.size());

  int num_dlls, num_functions, num_names;
  WalkBuffer(local_buffer.get(), buffer_size, &num_dlls, &num_functions,
             &num_names);

  EXPECT_EQ(3, num_dlls);
  EXPECT_EQ(4, num_functions);
  EXPECT_EQ(0, num_names);
}

}  // namespace sandbox

