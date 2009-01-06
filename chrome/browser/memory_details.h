// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_DETAILS_H_
#define CHROME_BROWSER_MEMORY_DETAILS_H_


#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/process_util.h"
#include "base/ref_counted.h"

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
};

typedef std::vector<ProcessMemoryInformation> ProcessMemoryInformationList;

// Information that we need about a plugin process.
struct PluginProcessInformation {
  int pid;
  FilePath plugin_path;
};
typedef std::vector<PluginProcessInformation> PluginProcessInformationList;

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

  // Access to the plugin details.
  const PluginProcessInformationList* plugins() const { return &plugins_; }

  // Initiate updating the current memory details.  These are fetched
  // asynchronously because data must be collected from multiple threads.
  // OnDetailsAvailable will be called when this process is complete.
  void StartFetch();

  virtual void OnDetailsAvailable() {}

 private:
  // Collect the a list of information about current plugin processes that
  // will be used by about:memory.  When finished, invokes back to the
  // return_loop to run the rest of the about:memory functionality.
  // Runs on the IO thread because the PluginProcessHost is only accessible
  // from the IO thread.
  void CollectPluginInformation();

  // Collect current process information from the OS and store it
  // for processing.  If data has already been collected, clears old
  // data and re-collects the data.
  // Note - this function enumerates memory details from many processes
  // and is fairly expensive to run.
  void CollectProcessData();

  // Collect renderer specific information.  This information is gathered
  // on the Browser thread, where the RenderHostIterator is accessible.
  void CollectRenderHostInformation();

  // Each time we take a memory sample, we do a little work to update
  // the global histograms for tracking memory usage.
  void UpdateHistograms();

  ProcessData process_data_[MAX_BROWSERS];
  PluginProcessInformationList plugins_;
  MessageLoop* ui_loop_;

  DISALLOW_EVIL_CONSTRUCTORS(MemoryDetails);
};

#endif  // CHROME_BROWSER_MEMORY_DETAILS_H_

