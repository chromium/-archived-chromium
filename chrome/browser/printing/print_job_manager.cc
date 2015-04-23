// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job_manager.h"

#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/common/notification_service.h"
#include "printing/printed_document.h"
#include "printing/printed_page.h"

namespace printing {

PrintJobManager::PrintJobManager() {
  registrar_.Add(this, NotificationType::PRINT_JOB_EVENT,
                 NotificationService::AllSources());
}

PrintJobManager::~PrintJobManager() {
  AutoLock lock(lock_);
  queued_queries_.clear();
}

void PrintJobManager::OnQuit() {
  // Common case, no print job pending.
  if (current_jobs_.empty())
    return;
  {
    // Don't take a chance and copy the array since it can be modified in
    // transit.
    PrintJobs current_jobs(current_jobs_);
    // Wait for every jobs to finish.
    for (size_t i = 0; i < current_jobs.size(); ++i) {
      PrintJob* job = current_jobs[i];
      if (!job)
        continue;
      // Wait for 120 seconds for the print job to be spooled.
      job->FlushJob(120000);
      job->Stop();
    }
  }
  current_jobs_.clear();
  registrar_.RemoveAll();
}

void PrintJobManager::QueuePrinterQuery(PrinterQuery* job) {
  AutoLock lock(lock_);
  DCHECK(job);
  queued_queries_.push_back(job);
  DCHECK(job->is_valid());
}

void PrintJobManager::PopPrinterQuery(int document_cookie,
                                      scoped_refptr<PrinterQuery>* job) {
  AutoLock lock(lock_);
  for (PrinterQueries::iterator itr = queued_queries_.begin();
       itr != queued_queries_.end();
       ++itr) {
    PrinterQuery* current_query = *itr;
    if (current_query->cookie() == document_cookie &&
        !current_query->is_callback_pending()) {
      *job = current_query;
      queued_queries_.erase(itr);
      DCHECK(current_query->is_valid());
      return;
    }
  }
}


void PrintJobManager::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::PRINT_JOB_EVENT: {
      OnPrintJobEvent(Source<PrintJob>(source).ptr(),
                      *Details<JobEventDetails>(details).ptr());
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void PrintJobManager::OnPrintJobEvent(
    PrintJob* print_job,
    const JobEventDetails& event_details) {
  switch (event_details.type()) {
    case JobEventDetails::NEW_DOC: {
      DCHECK(current_jobs_.end() == std::find(current_jobs_.begin(),
                                              current_jobs_.end(),
                                              print_job));
      // Causes a AddRef().
      current_jobs_.push_back(print_job);
      break;
    }
    case JobEventDetails::JOB_DONE: {
      PrintJobs::iterator itr = std::find(current_jobs_.begin(),
                                          current_jobs_.end(),
                                          print_job);
      DCHECK(current_jobs_.end() != itr);
      current_jobs_.erase(itr);
      DCHECK(current_jobs_.end() == std::find(current_jobs_.begin(),
                                              current_jobs_.end(),
                                              print_job));
      break;
    }
    case JobEventDetails::FAILED: {
      PrintJobs::iterator itr = std::find(current_jobs_.begin(),
                                          current_jobs_.end(),
                                          print_job);
      // A failed job may have never started.
      if (current_jobs_.end() != itr) {
        current_jobs_.erase(itr);
        DCHECK(current_jobs_.end() ==
                  std::find(current_jobs_.begin(),
                            current_jobs_.end(),
                            print_job));
      }
      break;
    }
    case JobEventDetails::USER_INIT_DONE:
    case JobEventDetails::USER_INIT_CANCELED:
    case JobEventDetails::DEFAULT_INIT_DONE:
    case JobEventDetails::NEW_PAGE:
    case JobEventDetails::PAGE_DONE:
    case JobEventDetails::DOC_DONE:
    case JobEventDetails::ALL_PAGES_REQUESTED: {
      // Don't care.
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

}  // namespace printing
