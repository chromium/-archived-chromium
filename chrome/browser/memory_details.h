// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_DETAILS_H_
#define CHROME_BROWSER_MEMORY_DETAILS_H_

#include <vector>

#include "base/process_util.h"
#include "base/ref_counted.h"
#include "chrome/common/child_process_info.h"

class MessageLoop;

// We collect data about each browser process.  A browser may
// have multiple processes (of course!).  Even IE has multiple
// processes these days.
struct ProcessMemoryInformation {
  ProcessMemoryInformation() {
    memset(this, 0, sizeof(ProcessMemoryInformation));
  }

  // The process id.
  int pid;
  // The working set information.
  base::WorkingSetKBytes working_set;
  // The committed bytes.
  base::CommittedKBytes committed;
  // The process version
  std::wstring version;
  // The process product name.
  std::wstring product_name;
  // The number of processes which this memory represents.
  int num_processes;
  // A process is a diagnostics process if it is rendering
  // about:xxx information.
  bool is_diagnostics;
  // If this is a child process of Chrome, what type (i.e. plugin) it is.
  ChildProcessInfo::ProcessType type;
  // A collection of titles used, i.e. for a tab it'll show all the page titles.
  std::vector<std::wstring> titles;
};

typedef std::vector<ProcessMemoryInformation> ProcessMemoryInformationList;

// Browser Process Information.
struct ProcessData {
  const wchar_t* name;
  const wchar_t* process_name;
  ProcessMemoryInformationList processes;
};

// MemoryDetails fetches memory details about current running browsers.
// Because this data can only be fetched asynchronously, callers use
// this class via a callback.
//
// Example usage:
//
//    class MyMemoryDetailConsumer : public MemoryDetails {
//
//      MyMemoryDetailConsumer() : MemoryDetails(true) {
//        StartFetch();  // Starts fetching details.
//      }
//
//      // Your class stuff here
//
//      virtual void OnDetailsAvailable() {
//           // do work with memory info here
//      }
//    }
class MemoryDetails : public base::RefCountedThreadSafe<MemoryDetails> {
 public:
  // Known browsers which we collect details for.
  enum {
    CHROME_BROWSER = 0,
    IE_BROWSER,
    FIREFOX_BROWSER,
    OPERA_BROWSER,
    SAFARI_BROWSER,
    MAX_BROWSERS
  } BrowserProcess;

  // Constructor.
  MemoryDetails();
  virtual ~MemoryDetails() {}

  // Access to the process detail information.  This is an array
  // of MAX_BROWSER ProcessData structures.  This data is only available
  // after OnDetailsAvailable() has been called.
  ProcessData* processes() { return process_data_; }

  // Initiate updating the current memory details.  These are fetched
  // asynchronously because data must be collected from multiple threads.
  // OnDetailsAvailable will be called when this process is complete.
  void StartFetch();

  virtual void OnDetailsAvailable() {}

 private:
  // Collect child process information on the IO thread.  This is needed because
  // information about some child process types (i.e. plugins) can only be taken
  // on that thread.  The data will be used by about:memory.  When finished,
  // invokes back to the file thread to run the rest of the about:memory
  // functionality.
  void CollectChildInfoOnIOThread();

  // Collect current process information from the OS and store it
  // for processing.  If data has already been collected, clears old
  // data and re-collects the data.
  // Note - this function enumerates memory details from many processes
  // and is fairly expensive to run, hence it's run on the file thread.
  // The parameter holds information about processes from the IO thread.
  void CollectProcessData(std::vector<ProcessMemoryInformation>);

  // Collect child process information on the UI thread.  Information about
  // renderer processes is only available there.
  void CollectChildInfoOnUIThread();

  // Each time we take a memory sample, we do a little work to update
  // the global histograms for tracking memory usage.
  void UpdateHistograms();

  ProcessData process_data_[MAX_BROWSERS];
  MessageLoop* ui_loop_;

  DISALLOW_EVIL_CONSTRUCTORS(MemoryDetails);
};

#endif  // CHROME_BROWSER_MEMORY_DETAILS_H_
