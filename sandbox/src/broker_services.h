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

#ifndef SANDBOX_SRC_BROKER_SERVICES_H__
#define SANDBOX_SRC_BROKER_SERVICES_H__

#include <list>
#include "base/basictypes.h"
#include "sandbox/src/crosscall_server.h"
#include "sandbox/src/job.h"
#include "sandbox/src/sandbox.h"
#include "sandbox/src/sharedmem_ipc_server.h"
#include "sandbox/src/win2k_threadpool.h"
#include "sandbox/src/win_utils.h"

namespace sandbox {

class PolicyBase;

// BrokerServicesBase ---------------------------------------------------------
// Broker implementation version 0
//
// This is an implementation of the interface BrokerServices and
// of the associated TargetProcess interface. In this implementation
// TargetProcess is a friend of BrokerServices where the later manages a
// collection of the former.
class BrokerServicesBase : public BrokerServices,
                           public SingletonBase<BrokerServicesBase>  {
 public:
  BrokerServicesBase();

  ~BrokerServicesBase();

  // The next four methods are the BrokerServices interface
  virtual ResultCode Init();

  virtual TargetPolicy* CreatePolicy();

  virtual ResultCode SpawnTarget(const wchar_t* exe_path,
                                 const wchar_t* command_line,
                                 TargetPolicy* policy,
                                 PROCESS_INFORMATION* target);

  virtual ResultCode WaitForAllTargets();

 private:
  // Helper structure that allows the Broker to associate a job notification
  // with a job object and with a policy.
  struct JobTracker {
    HANDLE job;
    PolicyBase* policy;
    JobTracker(HANDLE cjob, PolicyBase* cpolicy)
        : job(cjob), policy(cpolicy) {
     }
  };

  // Releases the Job and notifies the associated Policy object to its
  // resources as well.
  static void FreeResources(JobTracker* tracker);

  // The routine that the worker thread executes. It is in charge of
  // notifications and cleanup-related tasks.
  static DWORD WINAPI TargetEventsThread(PVOID param);

  // The completion port used by the job objects to communicate events to
  // the worker thread.
  HANDLE job_port_;

  // Handle to a manual-reset event that is signaled when the total target
  // process count reaches zero.
  HANDLE no_targets_;

  // Handle to the worker thread that reacts to job notifications.
  HANDLE job_thread_;

  // Lock used to protect the list of targets from being modified by 2
  // threads at the same time.
  CRITICAL_SECTION lock_;

  // provides a pool of threads that are used to wait on the IPC calls.
  ThreadProvider* thread_pool_;

  // List of the trackers for closing and cleanup purposes.
  typedef std::list<JobTracker*> JobTrackerList;
  JobTrackerList tracker_list_;

  DISALLOW_EVIL_CONSTRUCTORS(BrokerServicesBase);
};

}  // namespace sandbox


#endif  // SANDBOX_SRC_BROKER_SERVICES_H__
