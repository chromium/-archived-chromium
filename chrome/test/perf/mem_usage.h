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

#ifndef CHROME_TEST_PERF_MEM_USAGE_H__
#define CHROME_TEST_PERF_MEM_USAGE_H__

// Get memory information for the process with given process ID.
//
// The Windows psapi provides memory information of a process through structure
// RPROCESS_MEMORY_COUNTERS_EX. Relevant fields are:
// PagefileUsage: private (not shared) committed virtual space in process.
//                This is "VM Size" in task manager processes tab.
// PeakPagefileUsage: peak value of PagefileUsage.
// WorkingSetSize: physical memory allocated to process including shared pages.
//                 This is "Memory Usage" in task manager processes tab.
//                 Contrary to its name, this value is not much useful for
//                 tracking the memory demand of an application.
// PeakWorkingSetSize: peak value of WorkingSetSize.
//                     This is "Peak Memory Usage" in task manager processes
//                     tab.
// PrivateUsage: The current amount of memory that cannot be shared with other
//               processes. Private bytes include memory that is committed and
//               marked MEM_PRIVATE, data that is not mapped, and executable
//               pages that have been written to. It usually gives the same
//               value as PagefileUsage. No equivalent part in task manager.
//
// The measurements we use are PagefileUsage (returned by current_virtual_size)
// and PeakPagefileUsage (returned by peak_virtual_size), Working Set and
// Peak working Set.
bool GetMemoryInfo(uint32 process_id,
                   size_t *peak_virtual_size,
                   size_t *current_virtual_size,
                   size_t *peak_working_set_size,
                   size_t *current_working_set_size);

// Get and print memory usage information for running chrome processes.
void PrintChromeMemoryUsageInfo();
#endif // CHROME_TEST_PERF_MEM_USAGE_H__
