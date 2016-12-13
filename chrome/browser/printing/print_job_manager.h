// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_JOB_MANAGER_H_
#define CHROME_BROWSER_PRINTING_PRINT_JOB_MANAGER_H_

#include <string>
#include <vector>

#include "base/lock.h"
#include "base/ref_counted.h"
#include "chrome/common/notification_registrar.h"

namespace printing {

class JobEventDetails;
class PrintedDocument;
class PrintJob;
class PrintedPage;
class PrinterQuery;

class PrintJobManager : public NotificationObserver {
 public:
  PrintJobManager();
  ~PrintJobManager();

  // On browser quit, we should wait to have the print job finished.
  void OnQuit();

  // Queues a semi-initialized worker thread. Can be called from any thread.
  // Current use case is queuing from the I/O thread.
  // TODO(maruel):  Have them vanish after a timeout (~5 minutes?)
  void QueuePrinterQuery(PrinterQuery* job);

  // Pops a queued PrintJobWorkerOwner object that was previously queued. Can be
  // called from any thread. Current use case is poping from the browser thread.
  void PopPrinterQuery(int document_cookie, scoped_refptr<PrinterQuery>* job);

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  typedef std::vector<scoped_refptr<PrintJob> > PrintJobs;
  typedef std::vector<scoped_refptr<PrinterQuery> > PrinterQueries;

  // Processes a NOTIFY_PRINT_JOB_EVENT notification.
  void OnPrintJobEvent(PrintJob* print_job,
                       const JobEventDetails& event_details);

  NotificationRegistrar registrar_;

  // Used to serialize access to queued_workers_.
  Lock lock_;

  PrinterQueries queued_queries_;

  // Current print jobs that are active.
  PrintJobs current_jobs_;

  DISALLOW_EVIL_CONSTRUCTORS(PrintJobManager);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_JOB_MANAGER_H_
