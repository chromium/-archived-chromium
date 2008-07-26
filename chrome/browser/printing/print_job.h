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

#ifndef CHROME_BROWSER_PRINTING_PRINT_JOB_H__
#define CHROME_BROWSER_PRINTING_PRINT_JOB_H__

#include "base/ref_counted.h"
#include "chrome/browser/printing/print_job_worker_owner.h"
#include "chrome/common/notification_service.h"

class ChromeFont;
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
                 public PrintJobWorkerOwner {
 public:
  // GetSettings() UI parameter.
  enum GetSettingsAskParam {
    DEFAULTS,
    ASK_USER,
  };

  // Create a standalone PrintJob. When initializing with this constructor,
  // Initialize() must not be called.
  PrintJob(PrintedPagesSource* source);
  // Create a empty PrintJob. When initializing with this constructor,
  // post-constructor initialization must be done with Initialize().
  PrintJob();
  virtual ~PrintJob();

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

  // Initializes the printing context. This can be done synchronously or not. It
  // is fine to call this function multiple times to reinitialize the settings.
  // |parent_window| parameter will be the owner of the print setting dialog
  // box. It is unused when |ask_for_user_settings| is DEFAULTS. No-op if a
  // print job is active.
  void GetSettings(GetSettingsAskParam ask_user_for_settings,
                   HWND parent_window);

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

  // Requests all the missing pages in the PrintedDocument. Returns true if at
  // least one page has been requested. Returns false if there was not enough
  // information to request the missing pages, i.e.
  // document()->document_page_count() is not initialized or no page
  // has been requested.
  bool RequestMissingPages();

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

  // Main message loop reference. Used to send notifications in the right
  // thread.
  MessageLoop* const ui_message_loop_;

  // Source that generates the PrintedPage's (i.e. a WebContents). It will be
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

  // Is GetSettings() blocking to act like a synchronous function?
  bool is_blocking_;

  // Is Canceling? If so, try to not cause recursion if on FAILED notification,
  // the notified calls Cancel() again.
  bool is_canceling_;

  DISALLOW_EVIL_CONSTRUCTORS(PrintJob);
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

  DISALLOW_EVIL_CONSTRUCTORS(JobEventDetails);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_JOB_H__
