// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_JOB_MANAGER_H_
#define CHROME_BROWSER_PRINTING_PRINT_JOB_MANAGER_H_

#include <string>
#include <vector>

#include "base/lock.h"
#include "base/ref_counted.h"
#include "chrome/common/notification_observer.h"

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

  // Sets a path where to dump EMF data files. This enables debug behavior where
  // every rendered pages are dumped as-is. By default the path is empty, which
  // disables the dumping.
  // TODO(maruel): Remove me once printing is awesome.
  void set_debug_dump_path(const std::wstring& debug_dump_path) {
    debug_dump_path_ = debug_dump_path;
  }

  const std::wstring& debug_dump_path() const {
    return debug_dump_path_;
  }

 private:
  typedef std::vector<scoped_refptr<PrintJob> > PrintJobs;
  typedef std::vector<scoped_refptr<PrinterQuery> > PrinterQueries;

  // Processes a NOTIFY_PRINT_JOB_EVENT notification.
  void OnPrintJobEvent(PrintJob* print_job,
                       const JobEventDetails& event_details);

  // Processes a NOTIFY_PRINTED_DOCUMENT_UPDATED notification. When
  // debug_dump_path_ is not empty, it is processed to detect newly rendered
  // pages and to dump their EMF buffer.
  void OnPrintedDocumentUpdated(const PrintedDocument& document,
                                const PrintedPage& page);

  // Used to serialize access to queued_workers_.
  Lock lock_;

  PrinterQueries queued_queries_;

  // Current print jobs that are active.
  PrintJobs current_jobs_;

  // Path where debug dump of EMF buffer are saved. Empty by default. When
  // empty, EMF dumping is disabled.
  // TODO(maruel): Remove me once printing is awesome.
  std::wstring debug_dump_path_;

  DISALLOW_EVIL_CONSTRUCTORS(PrintJobManager);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_JOB_MANAGER_H_
