// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_
#define CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_

#include "base/ref_counted.h"
#include "chrome/browser/printing/printed_pages_source.h"
#include "chrome/common/notification_observer.h"

class RenderViewHost;
class WebContents;
struct ViewHostMsg_DidPrintPage_Params;

namespace printing {

class JobEventDetails;
class PrintJob;
class PrintJobWorkerOwner;

// Manages the print commands in relation to a WebContents. WebContents
// delegates a few printing related commands to this instance.
class PrintViewManager : public NotificationObserver,
                         public PrintedPagesSource {
 public:
  PrintViewManager(WebContents& owner);
  virtual ~PrintViewManager();

  // Destroys the "Print..." dialog, makes sure the pages are finished rendering
  // and release the print job.
  void Destroy();

  // Cancels the print job.
  void Stop();

  // Terminates or cancels the print job if one was pending, depending on the
  // current state. Returns false if the renderer was not valuable.
  bool OnRenderViewGone(RenderViewHost* render_view_host);

  // Received a notification from the renderer that the number of printed page
  // has just been calculated..
  void DidGetPrintedPagesCount(int cookie, int number_pages);

  // Received a notification from the renderer that a printed page page is
  // finished renderering.
  void DidPrintPage(const ViewHostMsg_DidPrintPage_Params& params);

  // PrintedPagesSource implementation.
  virtual std::wstring RenderSourceName();
  virtual GURL RenderSourceUrl();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Processes a NOTIFY_PRINT_JOB_EVENT notification.
  void OnNotifyPrintJobEvent(const JobEventDetails& event_details);

  // Requests the RenderView to render all the missing pages for the print job.
  // Noop if no print job is pending. Returns true if at least one page has been
  // requested to the renderer.
  bool RenderAllMissingPagesNow();

  // Quits the current message loop if these conditions hold true: a document is
  // loaded and is complete and waiting_for_pages_to_be_rendered_ is true. This
  // function is called in DidPrintPage() or on ALL_PAGES_REQUESTED
  // notification. The inner message loop is created was created by
  // RenderAllMissingPagesNow().
  void ShouldQuitFromInnerMessageLoop();

  // Creates a new empty print job. It has no settings loaded. If there is
  // currently a print job, safely disconnect from it. Returns false if it is
  // impossible to safely disconnect from the current print job or it is
  // impossible to create a new print job.
  bool CreateNewPrintJob(PrintJobWorkerOwner* job);

  // Makes sure the current print_job_ has all its data before continuing, and
  // disconnect from it.
  void DisconnectFromCurrentPrintJob();

  // Terminates the print job. Noop if no print job has been created. If
  // |cancel| is true, cancel it instead of waiting for the job to finish. Will
  // call ReleasePrintJob().
  void TerminatePrintJob(bool cancel);

  // Releases print_job_. Correctly deregisters from notifications. Noop if
  // no print job has been created.
  void ReleasePrintJob();

  // Prints the document. Starts the actual print job. Requests asynchronously
  // the renderered pages from the renderer. Is called once the printing context
  // is initialized, on a DEFAULT_INIT_DONE notification when waiting_to_print_
  // is true.
  void PrintNowInternal();

  // Runs an inner message loop. It will set inside_inner_message_loop_ to true
  // while the blocking inner message loop is running. This is useful in cases
  // where the RenderView is about to be destroyed while a printing job isn't
  // finished.
  bool RunInnerMessageLoop();

  // In the case of Scripted Printing, where the renderer is controlling the
  // control flow, print_job_ is initialized whenever possible. No-op is
  // print_job_ is initialized.
  bool OpportunisticallyCreatePrintJob(int cookie);

  // Manages the low-level talk to the printer.
  scoped_refptr<PrintJob> print_job_;

  // Waiting for print_job_ initialization to be completed to start printing.
  // Specifically the DEFAULT_INIT_DONE notification. Set when PrintNow() is
  // called.
  bool waiting_to_print_;

  // Running an inner message loop inside RenderAllMissingPagesNow(). This means
  // we are _blocking_ until all the necessary pages have been rendered or the
  // print settings are being loaded.
  bool inside_inner_message_loop_;

  // PrintViewManager is created as an extension of WebContent specialized for
  // printing-related behavior. Still, access to the renderer is needed so a
  // back reference is kept the the "parent object".
  WebContents& owner_;

  DISALLOW_COPY_AND_ASSIGN(PrintViewManager);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_
