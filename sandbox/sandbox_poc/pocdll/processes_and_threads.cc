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
#include <Tlhelp32.h>
#include "sandbox/sandbox_poc/pocdll/exports.h"
#include "sandbox/sandbox_poc/pocdll/utils.h"

// This file contains the tests used to verify the security of threads and
// processes.

void POCDLL_API TestProcesses(HANDLE log) {
  HandleToFile handle2file;
  FILE *output = handle2file.Translate(log, "w");

  HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
  if (INVALID_HANDLE_VALUE == snapshot) {
    fprintf(output, "[BLOCKED] Cannot list all processes on the system. "
                    "Error %d\r\n", ::GetLastError());
    return;
  }

  PROCESSENTRY32 process_entry = {0};
  process_entry.dwSize = sizeof(PROCESSENTRY32);

  BOOL result = ::Process32First(snapshot, &process_entry);

  while (result) {
    HANDLE process = ::OpenProcess(PROCESS_VM_READ,
                                   FALSE,  // Do not inherit handle.
                                   process_entry.th32ProcessID);
    if (NULL == process) {
      fprintf(output, "[BLOCKED] Found process %S:%d but cannot open it. "
                      "Error %d\r\n",
                      process_entry.szExeFile,
                      process_entry.th32ProcessID,
                      ::GetLastError());
    } else {
      fprintf(output, "[GRANTED] Found process %S:%d and open succeeded.\r\n",
                      process_entry.szExeFile, process_entry.th32ProcessID);
      ::CloseHandle(process);
    }

    result = ::Process32Next(snapshot, &process_entry);
  }

  DWORD err_code = ::GetLastError();
  if (ERROR_NO_MORE_FILES != err_code) {
    fprintf(output, "[ERROR] Error %d while looking at the processes on "
                    "the system\r\n", err_code);
  }

  ::CloseHandle(snapshot);
}

void POCDLL_API TestThreads(HANDLE log) {
  HandleToFile handle2file;
  FILE *output = handle2file.Translate(log, "w");

  HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, NULL);
  if (INVALID_HANDLE_VALUE == snapshot) {
    fprintf(output, "[BLOCKED] Cannot list all threads on the system. "
                    "Error %d\r\n", ::GetLastError());
    return;
  }

  THREADENTRY32 thread_entry = {0};
  thread_entry.dwSize = sizeof(THREADENTRY32);

  BOOL result = ::Thread32First(snapshot, &thread_entry);
  int nb_success = 0;
  int nb_failure = 0;

  while (result) {
    HANDLE thread = ::OpenThread(THREAD_QUERY_INFORMATION,
                                 FALSE,  // Do not inherit handles.
                                 thread_entry.th32ThreadID);
    if (NULL == thread) {
      nb_failure++;
    } else {
      nb_success++;
      fprintf(output, "[GRANTED] Found thread %d:%d and able to open it.\r\n",
                      thread_entry.th32OwnerProcessID,
                      thread_entry.th32ThreadID);
      ::CloseHandle(thread);
    }

    result = Thread32Next(snapshot, &thread_entry);
  }

  DWORD err_code = ::GetLastError();
  if (ERROR_NO_MORE_FILES != err_code) {
    fprintf(output, "[ERROR] Error %d while looking at the processes on "
                    "the system\r\n", err_code);
  }

  fprintf(output, "[INFO] Found %d threads. Able to open %d of them\r\n",
          nb_success + nb_failure, nb_success);

  ::CloseHandle(snapshot);
}
