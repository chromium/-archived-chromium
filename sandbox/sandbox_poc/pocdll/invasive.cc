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

#include <malloc.h>
#include "sandbox/sandbox_poc/pocdll/exports.h"
#include "sandbox/sandbox_poc/pocdll/utils.h"

// This file contains the tests used to verify if it's possible to DOS or crash
// the machine. All tests that can impact the stability of the machine should
// be in this file.

// Sleeps forever. this function is used to be the
// entry point for the threads created by the thread bombing function.
// This function never returns.
DWORD WINAPI MyThreadBombimgFunction(void *param) {
  UNREFERENCED_PARAMETER(param);
  Sleep(INFINITE);
  return 0;
}

void POCDLL_API TestThreadBombing(HANDLE log) {
  HandleToFile handle2file;
  FILE *output = handle2file.Translate(log, "w");

  // we stop after 5 errors in a row
  int number_errors = 0;
  for (int i = 0; i < 100000; ++i) {
    DWORD tid;
    // Create the thread and leak the handle.
    HANDLE thread = ::CreateThread(NULL,  // Default security attributes
                                   NULL,  // Stack size
                                   MyThreadBombimgFunction,
                                   NULL,  // Parameter
                                   0,     // No creation flags
                                   &tid);
    if (thread) {
      fprintf(output, "[GRANTED] Creating thread with tid 0x%X\r\n", tid);
      ::CloseHandle(thread);
      number_errors = 0;
    } else {
      fprintf(output, "[BLOCKED] Creating thread. Error %d\r\n",
              ::GetLastError());
      number_errors++;
    }

    if (number_errors >= 5) {
      break;
    }
  }
}


// Executes a complex mathematical operation forever in a loop. This function
// is used as entry point for the threads created by TestTakeAllCpu. It it
// designed to take all CPU on the processor where the thread is running.
// The return value is always 0.
DWORD WINAPI TakeAllCpu(void *param) {
  UNREFERENCED_PARAMETER(param);
  int cpt = 0;
  for (;;) {
    cpt += 2;
    cpt /= 2;
    cpt *= cpt;
    cpt = cpt % 100;
    cpt = cpt | (cpt * cpt);
  }
}

void POCDLL_API TestTakeAllCpu(HANDLE log) {
  HandleToFile handle2file;
  FILE *output = handle2file.Translate(log, "w");

  DWORD_PTR process_mask = 0;
  DWORD_PTR system_mask = 0;
  if (::GetProcessAffinityMask(::GetCurrentProcess(),
                               &process_mask,
                               &system_mask)) {
    DWORD_PTR affinity_mask = 1;

    while (system_mask) {
      DWORD tid = 0;

      HANDLE thread = ::CreateThread(NULL,  // Default security attributes.
                                     NULL,  // Stack size.
                                     TakeAllCpu,
                                     NULL,  // Parameter.
                                     0,     // No creation flags.
                                     &tid);
      ::SetThreadAffinityMask(thread, affinity_mask);

      if (::SetThreadPriority(thread, REALTIME_PRIORITY_CLASS)) {
        fprintf(output, "[GRANTED] Set thread(%d) priority to Realtime\r\n",
                tid);
      } else {
        fprintf(output, "[BLOCKED] Set thread(%d) priority to Realtime\r\n",
                tid);
      }

      ::CloseHandle(thread);

      affinity_mask = affinity_mask << 1;
      system_mask = system_mask >> 1;
    }
  } else {
    fprintf(output, "[ERROR] Cannot get affinity mask. Error %d\r\n",
           ::GetLastError());
  }
}

void POCDLL_API TestUseAllMemory(HANDLE log) {
  HandleToFile handle2file;
  FILE *output = handle2file.Translate(log, "w");

  int number_errors = 0;
  unsigned long memory_size = 0;
  for (;;) {
    DWORD *ptr_to_leak = reinterpret_cast<DWORD *>(malloc(1024*256));
    if (ptr_to_leak) {
      memory_size += (256);
      number_errors = 0;
    } else {
      number_errors++;
    }

    // check if we have more than 5 errors in a row. If so, quit.
    if (number_errors >= 5) {
      fprintf(output, "[INFO] Created %lu kb of memory\r\n", memory_size);
      return;
    }

    Sleep(5);  // 5ms to be able to see the progression easily with taskmgr.
  }
}

void POCDLL_API TestCreateObjects(HANDLE log) {
  HandleToFile handle2file;
  FILE *output = handle2file.Translate(log, "w");

  int mutexes = 0;
  int jobs = 0;
  int events = 0;
  for (int i = 0; i < 1000000; ++i) {
    if (::CreateMutex(NULL,     // Default security attributes.
                      TRUE,     // We are the initial owner.
                      NULL)) {  // No name.
      mutexes++;
    }

    if (::CreateJobObject(NULL,     // Default security attributes.
                          NULL)) {  // No name.
      jobs++;
    }

    if (::CreateEvent(NULL,     // Default security attributes.
                      TRUE,     // Manual Reset.
                      TRUE,     // Object is signaled.
                      NULL)) {  // No name.
      events++;
    }
  }

  fprintf(output, "[GRANTED] Created %d mutexes, %d jobs and %d events for "
                  "a total of %d objects out of 3 000 000\r\n", mutexes, jobs,
                  events, mutexes + jobs + events);
}

BOOL CALLBACK EnumWindowCallback(HWND hwnd, LPARAM output) {
  DWORD pid;
  ::GetWindowThreadProcessId(hwnd, &pid);
  if (pid != ::GetCurrentProcessId()) {
    wchar_t window_title[100 + 1] = {0};
    ::GetWindowText(hwnd, window_title, 100);
    fprintf(reinterpret_cast<FILE*>(output),
            "[GRANTED] Found window 0x%p with title %S\r\n", hwnd, window_title);
    ::CloseWindow(hwnd);
  }

  return TRUE;
}

// Enumerates all the windows on the system and call the function to try to
// close them. The goal of this function is to try to kill the system by
// closing all windows.
// "output" is the stream used for logging.
void POCDLL_API TestCloseHWND(HANDLE log) {
  HandleToFile handle2file;
  FILE *output = handle2file.Translate(log, "w");

  ::EnumWindows(EnumWindowCallback, PtrToLong(output));
  // TODO(nsylvain): find a way to know when the enum is finished
  // before returning.
  ::Sleep(3000);
}
