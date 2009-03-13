// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains unit tests for the job object.

#include "sandbox/src/job.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// Tests the creation and destruction of the job.
TEST(JobTest, TestCreation) {
  // Scope the creation of Job.
  {
    // Create the job.
    Job job;
    ASSERT_EQ(ERROR_SUCCESS, job.Init(JOB_LOCKDOWN, L"my_test_job_name", 0));

    // check if the job exists.
    HANDLE job_handle = ::OpenJobObjectW(GENERIC_ALL, FALSE,
                                         L"my_test_job_name");
    ASSERT_NE(NULL, reinterpret_cast<ULONG_PTR>(job_handle));

    if (job_handle)
      CloseHandle(job_handle);
  }

  // Check if the job is destroyed when the object goes out of scope.
  HANDLE job_handle = ::OpenJobObjectW(GENERIC_ALL, FALSE, L"my_test_job_name");
  ASSERT_EQ(NULL, reinterpret_cast<ULONG_PTR>(job_handle));
  ASSERT_EQ(ERROR_FILE_NOT_FOUND, ::GetLastError());
}

// Tests the method "Detach".
TEST(JobTest, TestDetach) {
  HANDLE job_handle;
  // Scope the creation of Job.
  {
    // Create the job.
    Job job;
    ASSERT_EQ(ERROR_SUCCESS, job.Init(JOB_LOCKDOWN, L"my_test_job_name", 0));

    job_handle = job.Detach();
    ASSERT_NE(NULL, reinterpret_cast<ULONG_PTR>(job_handle));
  }

  // Check to be sure that the job is still alive even after the object is gone
  // out of scope.
  HANDLE job_handle_dup = ::OpenJobObjectW(GENERIC_ALL, FALSE,
                                           L"my_test_job_name");
  ASSERT_NE(NULL, reinterpret_cast<ULONG_PTR>(job_handle_dup));

  // Remove all references.
  if (job_handle_dup)
    ::CloseHandle(job_handle_dup);

  if (job_handle)
    ::CloseHandle(job_handle);

  // Check if the jbo is really dead.
  job_handle = ::OpenJobObjectW(GENERIC_ALL, FALSE, L"my_test_job_name");
  ASSERT_EQ(NULL, reinterpret_cast<ULONG_PTR>(job_handle));
  ASSERT_EQ(ERROR_FILE_NOT_FOUND, ::GetLastError());
}

// Tests the ui exceptions
TEST(JobTest, TestExceptions) {
  HANDLE job_handle;
  // Scope the creation of Job.
  {
    // Create the job.
    Job job;
    ASSERT_EQ(ERROR_SUCCESS, job.Init(JOB_LOCKDOWN, L"my_test_job_name",
                                      JOB_OBJECT_UILIMIT_READCLIPBOARD));

    job_handle = job.Detach();
    ASSERT_NE(NULL, reinterpret_cast<ULONG_PTR>(job_handle));

    JOBOBJECT_BASIC_UI_RESTRICTIONS jbur = {0};
    DWORD size = sizeof(jbur);
    BOOL result = ::QueryInformationJobObject(job_handle,
                                              JobObjectBasicUIRestrictions,
                                              &jbur, size, &size);
    ASSERT_TRUE(result);

    ASSERT_EQ(jbur.UIRestrictionsClass & JOB_OBJECT_UILIMIT_READCLIPBOARD, 0);
    ::CloseHandle(job_handle);
  }

  // Scope the creation of Job.
  {
    // Create the job.
    Job job;
    ASSERT_EQ(ERROR_SUCCESS, job.Init(JOB_LOCKDOWN, L"my_test_job_name", 0));

    job_handle = job.Detach();
    ASSERT_NE(NULL, reinterpret_cast<ULONG_PTR>(job_handle));

    JOBOBJECT_BASIC_UI_RESTRICTIONS jbur = {0};
    DWORD size = sizeof(jbur);
    BOOL result = ::QueryInformationJobObject(job_handle,
                                              JobObjectBasicUIRestrictions,
                                              &jbur, size, &size);
    ASSERT_TRUE(result);

    ASSERT_EQ(jbur.UIRestrictionsClass & JOB_OBJECT_UILIMIT_READCLIPBOARD,
              JOB_OBJECT_UILIMIT_READCLIPBOARD);
    ::CloseHandle(job_handle);
  }
}

// Tests the error case when the job is initialized twice.
TEST(JobTest, DoubleInit) {
  // Create the job.
  Job job;
  ASSERT_EQ(ERROR_SUCCESS, job.Init(JOB_LOCKDOWN, L"my_test_job_name", 0));
  ASSERT_EQ(ERROR_ALREADY_INITIALIZED, job.Init(JOB_LOCKDOWN, L"test", 0));
}

// Tests the error case when we use a method and the object is not yet
// initialized.
TEST(JobTest, NoInit) {
  Job job;
  ASSERT_EQ(ERROR_NO_DATA, job.UserHandleGrantAccess(NULL));
  ASSERT_EQ(ERROR_NO_DATA, job.AssignProcessToJob(NULL));
  ASSERT_EQ(NULL, reinterpret_cast<ULONG_PTR>(job.Detach()));
}

// Tests the initialization of the job with different security level.
TEST(JobTest, SecurityLevel) {
  Job job1;
  ASSERT_EQ(ERROR_SUCCESS, job1.Init(JOB_LOCKDOWN, L"job1", 0));

  Job job2;
  ASSERT_EQ(ERROR_SUCCESS, job2.Init(JOB_RESTRICTED, L"job2", 0));

  Job job3;
  ASSERT_EQ(ERROR_SUCCESS, job3.Init(JOB_LIMITED_USER, L"job3", 0));

  Job job4;
  ASSERT_EQ(ERROR_SUCCESS, job4.Init(JOB_INTERACTIVE, L"job4", 0));

  Job job5;
  ASSERT_EQ(ERROR_SUCCESS, job5.Init(JOB_UNPROTECTED, L"job5", 0));

  Job job6;
  ASSERT_EQ(ERROR_BAD_ARGUMENTS, job6.Init(
      static_cast<JobLevel>(JOB_UNPROTECTED+1), L"job6", 0));
}

// Tests the method "AssignProcessToJob".
TEST(JobTest, ProcessInJob) {
  // Create the job.
  Job job;
  ASSERT_EQ(ERROR_SUCCESS, job.Init(JOB_UNPROTECTED, L"job_test_process", 0));

  BOOL result = FALSE;

  wchar_t notepad[] = L"notepad";
  STARTUPINFO si = { sizeof(si) };
  PROCESS_INFORMATION pi = {0};
  result = ::CreateProcess(NULL, notepad, NULL, NULL, FALSE, 0, NULL, NULL, &si,
                           &pi);
  ASSERT_TRUE(result);
  ASSERT_EQ(ERROR_SUCCESS, job.AssignProcessToJob(pi.hProcess));

  // Get the job handle.
  HANDLE job_handle = job.Detach();

  // Check if the process is in the job.
  JOBOBJECT_BASIC_PROCESS_ID_LIST jbpidl = {0};
  DWORD size = sizeof(jbpidl);
  result = ::QueryInformationJobObject(job_handle,
                                       JobObjectBasicProcessIdList,
                                       &jbpidl, size, &size);
  EXPECT_TRUE(result);

  EXPECT_EQ(1, jbpidl.NumberOfAssignedProcesses);
  EXPECT_EQ(1, jbpidl.NumberOfProcessIdsInList);
  EXPECT_EQ(pi.dwProcessId, jbpidl.ProcessIdList[0]);

  EXPECT_TRUE(::TerminateProcess(pi.hProcess, 0));

  EXPECT_TRUE(::CloseHandle(job_handle));
}

}  // namespace sandbox
