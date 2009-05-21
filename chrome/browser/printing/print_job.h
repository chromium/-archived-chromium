// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_JOB_H_
#define CHROME_BROWSER_PRINTING_PRINT_JOB_H_

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "chrome/browser/printing/print_job_worker_owner.h"
#include "chrome/common/notification_registrar.h"

class GURL;
class Thread;

namespace printing {

// See definition below.
class JobEventDetails;

class PrintedDocument;
class PrintedPage;
class PrintedPagesSource;
class PrintJobWorker;
class PrinterQuery;

// Manages the print work for a specific document. Talks to the printer through
// PrintingContext though PrintJob::Worker. Hides access to PrintingContext in a
// worker thread so the caller never blocks. PrintJob will send notifications on
// any state change. While printing, the PrintJobManager instance keeps a
// reference to the job to be sure it is kept alive. All the code in this class
// runs in the UI thread.
class PrintJob : public base::RefCountedThreadSafe<PrintJob>,
                 public NotificationObserver,
                 public PrintJobWorkerOwner,
                 public MessageLoop::DestructionObserver {
 public:
  // Create a empty PrintJob. When initializing with this constructor,
  // post-constructor initialization must be done with Initialize().
  PrintJob();
  virtual ~PrintJob();

  // Grabs the ownership of the PrintJobWorker from another job, which is
  // usually a PrinterQuery.
  void Initialize(PrintJobWorkerOwner* job, PrintedPagesSource* source);

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // PrintJobWorkerOwner
  virtual void AddRef() {
    return base::RefCountedThreadSafe<PrintJob>::AddRef();
  }
  virtual void Release() {
    return base::RefCountedThreadSafe<PrintJob>::Release();
  }

  virtual void GetSettingsDone(const PrintSettings& new_settings,
                               PrintingContext::Result result);
  virtual PrintJobWorker* DetachWorker(PrintJobWorkerOwner* new_owner);
  virtual MessageLoop* message_loop() { return ui_message_loop_; }
  virtual const PrintSettings& settings() const { return settings_; }
  virtual int cookie() const;

  // DestructionObserver
  virtual void WillDestroyCurrentMessageLoop();

  // Starts the actual printing. Signals the worker that it should begin to
  // spool as soon as data is available.
  void StartPrinting();

  // Waits for the worker thread to finish its queued tasks and disconnects the
  // delegate object. The PrintJobManager will remove it reference. This may
  // have the side-effect of destroying the object if the caller doesn't have a
  // handle to the object.
  void Stop();

  // Cancels printing job and stops the worker thread. Takes effect immediately.
  void Cancel();

  // Synchronously wait for the job to finish. It is mainly useful when the
  // process is about to be shut down and we're waiting for the spooler to eat
  // our data.
  bool FlushJob(int timeout_ms);

  // Disconnects the PrintedPage source (PrintedPagesSource). It is done when
  // the source is being destroyed.
  void DisconnectSource();

  // Returns true if the print job is pending, i.e. between a StartPrinting()
  // and the end of the spooling.
  bool is_job_pending() const;

  // Returns true if the Print... dialog box is currently displayed.
  bool is_print_dialog_box_shown() const;

  // Access the current printed document. Warning: may be NULL.
  PrintedDocument* document() const;


 private:
  // Updates document_ to a new instance.
  void UpdatePrintedDocument(PrintedDocument* new_document);

  // Processes a NOTIFY_PRINT_JOB_EVENT notification.
  void OnNotifyPrintJobEvent(const JobEventDetails& event_details);

  // Releases the worker thread by calling Stop(), then broadcasts a JOB_DONE
  // notification.
  void OnDocumentDone();

  // Terminates the worker thread in a very controlled way, to work around any
  // eventual deadlock.
  void ControlledWorkerShutdown();

  NotificationRegistrar registrar_;

  // Main message loop reference. Used to send notifications in the right
  // thread.
  MessageLoop* const ui_message_loop_;

  // Source that generates the PrintedPage's (i.e. a TabContents). It will be
  // set back to NULL if the source is deleted before this object.
  PrintedPagesSource* source_;

  // All the UI is done in a worker thread because many Win32 print functions
  // are blocking and enters a message loop without your consent. There is one
  // worker thread per print job.
  scoped_ptr<PrintJobWorker> worker_;

  // Cache of the print context settings for access in the UI thread.
  PrintSettings settings_;

  // The printed document.
  scoped_refptr<PrintedDocument> document_;

  // Is the worker thread printing.
  bool is_job_pending_;

  // Is the Print... dialog box currently shown.
  bool is_print_dialog_box_shown_;

  // Is Canceling? If so, try to not cause recursion if on FAILED notification,
  // the notified calls Cancel() again.
  bool is_canceling_;

  DISALLOW_COPY_AND_ASSIGN(PrintJob);
};

// Details for a NOTIFY_PRINT_JOB_EVENT notification. The members may be NULL.
class JobEventDetails : public base::RefCountedThreadSafe<JobEventDetails> {
 public:
  // Event type.
  enum Type {
    // Print... dialog box has been closed with OK button.
    USER_INIT_DONE,

    // Print... dialog box has been closed with CANCEL button.
    USER_INIT_CANCELED,

    // An automated initialization has been done, e.g. Init(false, NULL).
    DEFAULT_INIT_DONE,

    // A new document started printing.
    NEW_DOC,

    // A new page started printing.
    NEW_PAGE,

    // A page is done printing.
    PAGE_DONE,

    // A document is done printing. The worker thread is still alive. Warning:
    // not a good moment to release the handle to PrintJob.
    DOC_DONE,

    // The worker thread is finished. A good moment to release the handle to
    // PrintJob.
    JOB_DONE,

    // All missing pages have been requested.
    ALL_PAGES_REQUESTED,

    // An error occured. Printing is canceled.
    FAILED,
  };

  JobEventDetails(Type type, PrintedDocument* document, PrintedPage* page);
  ~JobEventDetails();

  // Getters.
  PrintedDocument* document() const;
  PrintedPage* page() const;
  Type type() const {
    return type_;
  }

 private:
  scoped_refptr<PrintedDocument> document_;
  scoped_refptr<PrintedPage> page_;
  const Type type_;

  DISALLOW_COPY_AND_ASSIGN(JobEventDetails);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_JOB_H_
