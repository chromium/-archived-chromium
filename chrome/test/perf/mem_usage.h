// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

// Get the number of bytes currently committed by the system.
// Returns -1 on failure.
size_t GetSystemCommitCharge();

// Get and print memory usage information for running chrome processes.
void PrintChromeMemoryUsageInfo();
#endif  // CHROME_TEST_PERF_MEM_USAGE_H__
