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

#include "webkit/default_plugin/plugin_install_job_monitor.h"

#include "webkit/default_plugin/plugin_impl.h"

PluginInstallationJobMonitorThread::PluginInstallationJobMonitorThread()
    : Thread("Chrome plugin install thread"),
      install_job_completion_port_(NULL),
      stop_job_monitoring_(false),
      plugin_window_(NULL),
      install_job_(NULL) {
}

PluginInstallationJobMonitorThread::~PluginInstallationJobMonitorThread() {
  if (install_job_) {
    ::CloseHandle(install_job_);
    install_job_ = NULL;
  }
}

bool PluginInstallationJobMonitorThread::Initialize() {
  DCHECK(install_job_ == NULL);

  install_job_ = ::CreateJobObject(NULL, NULL);
  if (install_job_ == NULL) {
    DLOG(ERROR) << "Failed to create plugin install job. Error = "
                << ::GetLastError();
    NOTREACHED();
    return false;
  }

  return Start();
}

void PluginInstallationJobMonitorThread::Init() {
  if (!install_job_) {
    DLOG(WARNING) << "Invalid job information";
    NOTREACHED();
    return;
  }

  DCHECK(install_job_completion_port_ == NULL);

  install_job_completion_port_ =
        ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL,
                                 reinterpret_cast<ULONG_PTR>(install_job_), 0);
  DCHECK(install_job_completion_port_ != NULL);

  JOBOBJECT_ASSOCIATE_COMPLETION_PORT job_completion_port = {0};
  job_completion_port.CompletionKey = install_job_;
  job_completion_port.CompletionPort = install_job_completion_port_;

  if (!SetInformationJobObject(install_job_,
                               JobObjectAssociateCompletionPortInformation,
                               &job_completion_port,
                               sizeof(job_completion_port))) {
    DLOG(WARNING) << "Failed to associate completion port with job object.Err "
                  << ::GetLastError();
    NOTREACHED();
    return;
  }

  while (!stop_job_monitoring_) {
    unsigned long job_event = 0;
    unsigned long completion_key = 0;
    LPOVERLAPPED overlapped = NULL;

    if (::GetQueuedCompletionStatus(
            install_job_completion_port_, &job_event,
            &completion_key, &overlapped, INFINITE)) {
      if (job_event == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO) {
        DLOG(INFO) << "All processes in the installer job have exited.";
        DLOG(INFO) << "Initiating refresh on the plugins list";
        DCHECK(::IsWindow(plugin_window_));
        PostMessageW(plugin_window_,
                     PluginInstallerImpl::kRefreshPluginsMessage, 0, 0);
      }
    }
  }
}

void PluginInstallationJobMonitorThread::Stop() {
  stop_job_monitoring_ = true;
  ::PostQueuedCompletionStatus(
      install_job_completion_port_, JOB_OBJECT_MSG_END_OF_JOB_TIME,
      reinterpret_cast<ULONG_PTR>(install_job_), NULL);
  Thread::Stop();
  ::CloseHandle(install_job_completion_port_);
  install_job_completion_port_ = NULL;
}

bool PluginInstallationJobMonitorThread::AssignProcessToJob(
    HANDLE process_handle) {
  BOOL result = AssignProcessToJobObject(install_job_, process_handle);
  return result ? true : false;
}
