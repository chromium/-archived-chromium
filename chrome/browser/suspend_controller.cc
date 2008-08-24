// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/suspend_controller.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/resource_dispatcher_host.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_tracker.h"

// Static member.
bool SuspendController::is_suspended_ = false;

// Static method.
void SuspendController::OnSuspend(Profile* profile) {
  if (is_suspended_)
    return;
  is_suspended_ = true;

  LOG(INFO) << "Received APM suspend message";

  SuspendController* controller = g_browser_process->suspend_controller();

  MessageLoop* io_loop = g_browser_process->io_thread()->message_loop();
  io_loop->PostTask(FROM_HERE,
      NewRunnableMethod(controller,
                        &SuspendController::StopRequests,
                        profile));

  ResourceDispatcherHost* rdh = g_browser_process->resource_dispatcher_host();
  SafeBrowsingService* safe_browsing_service = rdh->safe_browsing_service();
  io_loop->PostTask(FROM_HERE,
      NewRunnableMethod(safe_browsing_service,
                        &SafeBrowsingService::OnSuspend));
}

// Static method.
void SuspendController::OnResume(Profile* profile) {
  if (!is_suspended_)
    return;
  is_suspended_ = false;

  LOG(INFO) << "Received APM resume message";

  SuspendController* controller = g_browser_process->suspend_controller();

  MessageLoop* io_loop = g_browser_process->io_thread()->message_loop();
  io_loop->PostTask(FROM_HERE,
      NewRunnableMethod(controller,
                        &SuspendController::AllowNewRequests,
                        profile));

  ResourceDispatcherHost* rdh = g_browser_process->resource_dispatcher_host();
  SafeBrowsingService* safe_browsing_service = rdh->safe_browsing_service();
  io_loop->PostTask(FROM_HERE,
      NewRunnableMethod(safe_browsing_service,
                        &SafeBrowsingService::OnResume));
}

void SuspendController::StopRequests(Profile* profile) {
  // Cancel all requests and stop creating new ones.
  URLRequestJobTracker::JobIterator it = g_url_request_job_tracker.begin();
  for (;it != g_url_request_job_tracker.end(); ++it) {
    URLRequestJob* job = const_cast<URLRequestJob*>(*it);
    job->Kill();
  }

  // Close the network session.
  profile->GetRequestContext()->http_transaction_factory()->Suspend(true);
}

void SuspendController::AllowNewRequests(Profile* profile) {
  // Re-enable the network session.
  profile->GetRequestContext()->http_transaction_factory()->Suspend(false);
}

