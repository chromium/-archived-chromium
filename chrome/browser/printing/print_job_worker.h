// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_H__
#define CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_H__

#include "base/task.h"
#include "base/thread.h"
#include "chrome/browser/printing/page_number.h"
#include "chrome/browser/printing/win_printing_context.h"

namespace printing {

class PrintedDocument;
class PrintedPage;
class PrintJob;
class PrintJobWorkerOwner;

// Worker thread code. All this code, except for the constructor, is executed in
// the worker thread. It manages the PrintingContext, which can be blocking
// and/or run a message loop. This is the object that generates most
// NOTIFY_PRINT_JOB_EVENT notifications, but they are generated through a
// NotificationTask task to be executed from the right thread, the UI thread.
// PrintJob always outlives its worker instance.
class PrintJobWorker : public base::Thread {
 public:
  explicit PrintJobWorker(PrintJobWorkerOwner* owner);
  ~PrintJobWorker();

  void SetNewOwner(PrintJobWorkerOwner* new_owner);

  // Initializes the print settings. If |ask_user_for_settings| is true, a
  // Print... dialog box will be shown to ask the user his preference.
  void GetSettings(bool ask_user_for_settings,
                   HWND parent_window,
                   int document_page_count);

  // Starts the printing loop. Every pages are printed as soon as the data is
  // available. Makes sure the new_document is the right one.
  void StartPrinting(PrintedDocument* new_document);

  // Updates the printed document.
  void OnDocumentChanged(PrintedDocument* new_document);

  // Unqueues waiting pages. Called when PrintJob receives a
  // NOTIFY_PRINTED_DOCUMENT_UPDATED notification. It's time to look again if
  // the next page can be printed.
  void OnNewPage();

  // This is the only function that can be called in a thread.
  void Cancel();

  // Cancels the Print... dialog box if shown, noop otherwise.
  void DismissDialog();

  // Requests the missing pages in rendered_document_. Sends back a
  // ALL_PAGES_REQUESTED notification once done.
  void RequestMissingPages();

 protected:
  // Retrieves the context for testing only.
  PrintingContext& printing_context() { return printing_context_; }

 private:
  // The shared NotificationService service can only be accessed from the UI
  // thread, so this class encloses the necessary information to send the
  // notification from the right thread. Most NOTIFY_PRINT_JOB_EVENT
  // notifications are sent this way, except USER_INIT_DONE, USER_INIT_CANCELED
  // and DEFAULT_INIT_DONE. These three are sent through PrintJob::InitDone().
  class NotificationTask;
  friend struct RunnableMethodTraits<PrintJobWorker>;

  // Renders a page in the printer.
  void SpoolPage(PrintedPage& page);

  // Closes the job since spooling is done.
  void OnDocumentDone();

  // Discards the current document, the current page and cancels the printing
  // context.
  void OnFailure();

  // Information about the printer setting.
  PrintingContext printing_context_;

  // The printed document. Only has read-only access.
  scoped_refptr<PrintedDocument> document_;

  // The print job owning this worker thread. It is guaranteed to outlive this
  // object.
  PrintJobWorkerOwner* owner_;

  // Current page number to print.
  PageNumber page_number_;

  DISALLOW_EVIL_CONSTRUCTORS(PrintJobWorker);
};

}  // namespace printing

template <>
struct RunnableMethodTraits<printing::PrintJobWorker> {
  RunnableMethodTraits();
  void RetainCallee(printing::PrintJobWorker* obj);
  void ReleaseCallee(printing::PrintJobWorker* obj);
 private:
  scoped_refptr<printing::PrintJobWorkerOwner> owner_;
};

#endif  // CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_H__
