// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/src/broker_services.h"

#include "base/logging.h"
#include "base/platform_thread.h"
#include "sandbox/src/sandbox_policy_base.h"
#include "sandbox/src/sandbox.h"
#include "sandbox/src/target_process.h"
#include "sandbox/src/win2k_threadpool.h"
#include "sandbox/src/win_utils.h"

namespace {

// Utility function to associate a completion port to a job object.
bool AssociateCompletionPort(HANDLE job, HANDLE port, void* key) {
  JOBOBJECT_ASSOCIATE_COMPLETION_PORT job_acp = { key, port };
  return ::SetInformationJobObject(job,
                                   JobObjectAssociateCompletionPortInformation,
                                   &job_acp, sizeof(job_acp))? true : false;
}

// Utility function to do the cleanup necessary when something goes wrong
// while in SpawnTarget and we must terminate the target process.
sandbox::ResultCode SpawnCleanup(sandbox::TargetProcess* target, DWORD error) {
  if (0 == error)
    error = ::GetLastError();

  target->Terminate();
  delete target;
  ::SetLastError(error);
  return sandbox::SBOX_ERROR_GENERIC;
}

// the different commands that you can send to the worker thread that
// executes TargetEventsThread().
enum {
  THREAD_CTRL_NONE,
  THREAD_CTRL_QUIT,
  THREAD_CTRL_LAST
};

}

namespace sandbox {

BrokerServicesBase::BrokerServicesBase()
    : thread_pool_(NULL), job_port_(NULL), no_targets_(NULL),
      job_thread_(NULL) {
}

// The broker uses a dedicated worker thread that services the job completion
// port to perform policy notifications and associated cleanup tasks.
ResultCode BrokerServicesBase::Init() {
  if ((NULL != job_port_) || (NULL != thread_pool_))
    return SBOX_ERROR_UNEXPECTED_CALL;

  ::InitializeCriticalSection(&lock_);

  job_port_ = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
  if (NULL == job_port_)
    return SBOX_ERROR_GENERIC;

  no_targets_ = ::CreateEventW(NULL, TRUE, FALSE, NULL);

  job_thread_ = ::CreateThread(NULL, 0,  // Default security and stack.
                               TargetEventsThread, this, NULL, NULL);
  if (NULL == job_thread_)
    return SBOX_ERROR_GENERIC;

  return SBOX_ALL_OK;
}

// The destructor should only be called when the Broker process is terminating.
// Since BrokerServicesBase is a singleton, this is called from the CRT
// termination handlers, if this code lives on a DLL it is called during
// DLL_PROCESS_DETACH in other words, holding the loader lock, so we cannot
// wait for threads here.
BrokerServicesBase::~BrokerServicesBase() {
  // If there is no port Init() was never called successfully.
  if (!job_port_)
    return;
  // Closing the port causes, that no more Job notifications are delivered to
  // the worker thread and also causes the thread to exit. This is what we
  // want to do since we are going to close all outstanding Jobs and notifying
  // the policy objects ourselves.
  ::PostQueuedCompletionStatus(job_port_, 0, THREAD_CTRL_QUIT, FALSE);
  ::CloseHandle(job_port_);

  if (WAIT_TIMEOUT == ::WaitForSingleObject(job_thread_, 1000)) {
    // Cannot clean broker services.
    NOTREACHED();
    return;
  }

  JobTrackerList::iterator it;
  for (it = tracker_list_.begin(); it != tracker_list_.end(); ++it) {
    JobTracker* tracker = (*it);
    FreeResources(tracker);
    delete tracker;
  }
  ::CloseHandle(job_thread_);
  delete thread_pool_;
  ::CloseHandle(no_targets_);
  // If job_port_ isn't NULL, assumes that the lock has been initialized.
  if (job_port_)
    ::DeleteCriticalSection(&lock_);
}

TargetPolicy* BrokerServicesBase::CreatePolicy() {
  // If you change the type of the object being created here you must also
  // change the downcast to it in SpawnTarget().
  return new PolicyBase;
}

void BrokerServicesBase::FreeResources(JobTracker* tracker) {
  if (NULL != tracker->policy) {
    BOOL res = ::TerminateJobObject(tracker->job, SBOX_ALL_OK);
    DCHECK(res);
    res = ::CloseHandle(tracker->job);
    DCHECK(res);
    tracker->policy->OnJobEmpty(tracker->job);
    tracker->policy->Release();
    tracker->policy = NULL;
  }
}

// The worker thread stays in a loop waiting for asynchronous notifications
// from the job objects. Right now we only care about knowing when the last
// process on a job terminates, but in general this is the place to tell
// the policy about events.
DWORD WINAPI BrokerServicesBase::TargetEventsThread(PVOID param) {
  if (NULL == param)
    return 1;

  PlatformThread::SetName("BrokerEventThread");

  BrokerServicesBase* broker = reinterpret_cast<BrokerServicesBase*>(param);
  HANDLE port = broker->job_port_;
  HANDLE no_targets = broker->no_targets_;

  int target_counter = 0;
  ::ResetEvent(no_targets);

  while (true) {
    DWORD events = 0;
    ULONG_PTR key = 0;
    LPOVERLAPPED ovl = NULL;

    if (!::GetQueuedCompletionStatus(port, &events, &key, &ovl, INFINITE))
      // this call fails if the port has been closed before we have a
      // chance to service the last packet which is 'exit' anyway so
      // this is not an error.
      return 1;

    if (key > THREAD_CTRL_LAST) {
      // The notification comes from a job object. There are nine notifications
      // that jobs can send and some of them depend on the job attributes set.
      JobTracker* tracker = reinterpret_cast<JobTracker*>(key);

      switch (events) {
        case JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO: {
          // The job object has signaled that the last process associated
          // with it has terminated. Assuming there is no way for a process
          // to appear out of thin air in this job, it safe to assume that
          // we can tell the policy to destroy the target object, and for
          // us to release our reference to the policy object.
          FreeResources(tracker);
          break;
        }

        case JOB_OBJECT_MSG_NEW_PROCESS: {
          ++target_counter;
          if (1 == target_counter) {
            ::ResetEvent(no_targets);
          }
          break;
        }

        case JOB_OBJECT_MSG_EXIT_PROCESS:
        case JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS: {
          --target_counter;
          if (0 == target_counter)
            ::SetEvent(no_targets);

          DCHECK(target_counter >= 0);
          break;
        }

        case JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT: {
          break;
        }

        default: {
          NOTREACHED();
          break;
        }
      }

    } else if (THREAD_CTRL_QUIT == key) {
      // The broker object is being destroyed so the thread needs to exit.
      return 0;
    } else {
      // We have not implemented more commands.
      NOTREACHED();
    }
  }

  NOTREACHED();
  return 0;
}

// SpawnTarget does all the interesting sandbox setup and creates the target
// process inside the sandbox.
ResultCode BrokerServicesBase::SpawnTarget(const wchar_t* exe_path,
                                           const wchar_t* command_line,
                                           TargetPolicy* policy,
                                           PROCESS_INFORMATION* target_info) {
  if (!exe_path)
    return SBOX_ERROR_BAD_PARAMS;

  if (!policy)
    return SBOX_ERROR_BAD_PARAMS;

  AutoLock lock(&lock_);

  // This downcast is safe as long as we control CreatePolicy()
  PolicyBase* policy_base = static_cast<PolicyBase*>(policy);

  // Construct the tokens and the job object that we are going to associate
  // with the soon to be created target process.
  HANDLE lockdown_token = NULL;
  HANDLE initial_token = NULL;
  DWORD win_result = policy_base->MakeTokens(&initial_token, &lockdown_token);
  if (ERROR_SUCCESS != win_result)
    return SBOX_ERROR_GENERIC;

  HANDLE job = NULL;
  win_result = policy_base->MakeJobObject(&job);
  if (ERROR_SUCCESS != win_result)
    return SBOX_ERROR_GENERIC;

  if (ERROR_ALREADY_EXISTS == ::GetLastError())
    return SBOX_ERROR_GENERIC;

  // Construct the thread pool here in case it is expensive.
  // The thread pool is shared by all the targets
  if (NULL == thread_pool_)
    thread_pool_ = new Win2kThreadPool();

  // Create the TargetProces object and spawn the target suspended. Note that
  // Brokerservices does not own the target object. It is owned by the Policy.
  PROCESS_INFORMATION process_info = {0};
  TargetProcess* target = new TargetProcess(initial_token, lockdown_token,
                                            job, thread_pool_);

  std::wstring desktop = policy_base->GetAlternateDesktop();

  win_result = target->Create(exe_path, command_line,
                              desktop.empty() ? NULL : desktop.c_str(),
                              &process_info);
  if (ERROR_SUCCESS != win_result)
    return SpawnCleanup(target, win_result);

  if ((INVALID_HANDLE_VALUE == process_info.hProcess) ||
      (INVALID_HANDLE_VALUE == process_info.hThread))
    return SpawnCleanup(target, win_result);

  // Now the policy is the owner of the target.
  if (!policy_base->AddTarget(target)) {
    return SpawnCleanup(target, 0);
  }

  // We are going to keep a pointer to the policy because we'll call it when
  // the job object generates notifications using the completion port.
  policy_base->AddRef();
  JobTracker* tracker = new JobTracker(job, policy_base);
  if (!AssociateCompletionPort(job, job_port_, tracker))
    return SpawnCleanup(target, 0);
  // Save the tracker because in cleanup we might need to force closing
  // the Jobs.
  tracker_list_.push_back(tracker);

  // We return the caller a duplicate of the process handle so they
  // can close it at will.
  HANDLE dup_process_handle = NULL;
  if (!::DuplicateHandle(::GetCurrentProcess(), process_info.hProcess,
                         ::GetCurrentProcess(), &dup_process_handle,
                         0, FALSE, DUPLICATE_SAME_ACCESS))
    return SpawnCleanup(target, 0);

  *target_info = process_info;
  target_info->hProcess = dup_process_handle;
  return SBOX_ALL_OK;
}


ResultCode BrokerServicesBase::WaitForAllTargets() {
  ::WaitForSingleObject(no_targets_, INFINITE);
  return SBOX_ALL_OK;
}

}  // namespace sandbox
