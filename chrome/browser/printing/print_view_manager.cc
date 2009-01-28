// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/printed_document.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/gfx/emf.h"
#include "chrome/common/l10n_util.h"

#include "generated_resources.h"

using base::TimeDelta;

namespace printing {

PrintViewManager::PrintViewManager(WebContents& owner)
    : owner_(owner),
      waiting_to_print_(false),
      inside_inner_message_loop_(false),
      waiting_to_show_print_dialog_(false) {
  memset(&print_params_, 0, sizeof(print_params_));
}

PrintViewManager::~PrintViewManager() {
}

void PrintViewManager::Destroy() {
  DisconnectFromCurrentPrintJob();
}

void PrintViewManager::Stop() {
  // Cancel the current job, wait for the worker to finish.
  TerminatePrintJob(true);
}

void PrintViewManager::ShowPrintDialog() {
  if (!CreateNewPrintJob(NULL))
    return;

  // Retrieve default settings. PrintJob will send back a
  // NOTIFY_PRINT_JOB_EVENT with either INIT_DONE, INIT_CANCELED or FAILED.
  // On failure, simply back off. Otherwise, request the number of pages to
  // the renderer. Wait for its response (DidGetPrintedPagesCount), which will
  // give the value used to initialize the Print... dialog. PrintJob will send
  // back (again) a NOTIFY_PRINT_JOB_EVENT with either INIT_DONE, INIT_CANCELED
  // or FAILED. The result is to call PrintNowInternal or to back off.
  waiting_to_show_print_dialog_ = true;
  print_job_->GetSettings(PrintJob::DEFAULTS, NULL);
}

bool PrintViewManager::PrintNow() {
  if (!CreateNewPrintJob(NULL))
    return false;

  // Retrieve default settings. PrintJob will send back a NOTIFY_PRINT_JOB_EVENT
  // with either DEFAULT_INIT_DONE or FAILED. On failure, simply back off.
  // Otherwise, call PrintNowInternal() again to start the print job.
  waiting_to_print_ = true;
  print_job_->GetSettings(PrintJob::DEFAULTS, NULL);
  return true;
}

bool PrintViewManager::OnRendererGone(RenderViewHost* render_view_host) {
  if (!print_job_.get())
    return true;

  if (render_view_host != owner_.render_view_host())
    return false;

  scoped_refptr<PrintedDocument> document(print_job_->document());
  if (document) {
    // If IsComplete() returns false, the document isn't completely renderered.
    // Since our renderer is gone, there's nothing to do, cancel it. Otherwise,
    // the print job may finish without problem.
    TerminatePrintJob(!document->IsComplete());
  }
  return true;
}

void PrintViewManager::DidGetPrintedPagesCount(int cookie, int number_pages) {
  DCHECK_GT(cookie, 0);
  if (!OpportunisticallyCreatePrintJob(cookie))
    return;

  PrintedDocument* document = print_job_->document();
  if (!document || cookie != document->cookie()) {
    // Out of sync. It may happens since we are completely asynchronous. Old
    // spurious message can happen if one of the processes is overloaded.
    return;
  }

  // Time to inform our print job. Make sure it is for the right document.
  if (!document->page_count()) {
    document->set_page_count(number_pages);
    if (waiting_to_show_print_dialog_) {
      // Ask for user settings. There's a timing issue since we may not have
      // received the INIT_DONE notification yet. If so, the dialog will be
      // shown in Observe() since the page count arrived before the settings.
      print_job_->GetSettings(PrintJob::ASK_USER,
                              ::GetParent(owner_.GetContainerHWND()));
      waiting_to_show_print_dialog_ = false;
    }
  }
}

void PrintViewManager::DidPrintPage(
    const ViewHostMsg_DidPrintPage_Params& params) {
  DCHECK(!inside_inner_message_loop_);
  if (!OpportunisticallyCreatePrintJob(params.document_cookie))
    return;

  PrintedDocument* document = print_job_->document();
  if (!document || params.document_cookie != document->cookie()) {
    // Out of sync. It may happens since we are completely asynchronous. Old
    // spurious message can happen if one of the processes is overloaded.
    return;
  }

  // http://msdn2.microsoft.com/en-us/library/ms535522.aspx
  // Windows 2000/XP: When a page in a spooled file exceeds approximately 350
  // MB, it can fail to print and not send an error message.
  if (params.data_size && params.data_size >= 350*1024*1024) {
    NOTREACHED() << "size:" << params.data_size;
    TerminatePrintJob(true);
    owner_.Stop();
    return;
  }

  base::SharedMemory shared_buf(params.emf_data_handle, true);
  if (!shared_buf.Map(params.data_size)) {
    NOTREACHED() << "couldn't map";
    owner_.Stop();
    return;
  }

  gfx::Emf* emf = new gfx::Emf;
  if (!emf->CreateFromData(shared_buf.memory(), params.data_size)) {
    NOTREACHED() << "Invalid EMF header";
    delete emf;
    owner_.Stop();
    return;
  }

  // Update the rendered document. It will send notifications to the listener.
  document->SetPage(params.page_number, emf, params.actual_shrink);
  ShouldQuitFromInnerMessageLoop();
}

void PrintViewManager::RenderOnePrintedPage(PrintedDocument* document,
                                            int page_number) {
  // Currently a no-op. Rationale: printing is now completely synchronous and is
  // handled by PrintAllPages. The reason is that PrintPreview is not used
  // anymore and to make sure to not corrupt the screen, the whole generation is
  // done synchronously. To make this work completely asynchronously, a
  // duplicate copy of RenderView must be made to have an "innert" web page.
  // Once this object is created, we'll have all the leasure to do whatever we
  // want.
}

std::wstring PrintViewManager::RenderSourceName() {
  std::wstring name(owner_.GetTitle());
  if (name.empty())
    name = l10n_util::GetString(IDS_DEFAULT_PRINT_DOCUMENT_TITLE);
  return name;
}

GURL PrintViewManager::RenderSourceUrl() {
  NavigationEntry* entry = owner_.controller()->GetActiveEntry();
  if (entry)
    return entry->display_url();
  else
    return GURL();
}

void PrintViewManager::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  switch (type) {
    case NOTIFY_PRINT_JOB_EVENT: {
      OnNotifyPrintJobEvent(*Details<JobEventDetails>(details).ptr());
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void PrintViewManager::OnNotifyPrintJobEvent(
    const JobEventDetails& event_details) {
  switch (event_details.type()) {
    case JobEventDetails::FAILED: {
      // TODO(maruel):  bug 1123882 Show some kind of notification.
      TerminatePrintJob(true);
      break;
    }
    case JobEventDetails::USER_INIT_DONE:
    case JobEventDetails::DEFAULT_INIT_DONE:
    case JobEventDetails::USER_INIT_CANCELED: {
      OnNotifyPrintJobInitEvent(event_details);
      break;
    }
    case JobEventDetails::ALL_PAGES_REQUESTED: {
      ShouldQuitFromInnerMessageLoop();
      break;
    }
    case JobEventDetails::NEW_DOC:
    case JobEventDetails::NEW_PAGE:
    case JobEventDetails::PAGE_DONE: {
      // Don't care about the actual printing process.
      break;
    }
    case JobEventDetails::DOC_DONE: {
      waiting_to_print_ = false;
      break;
    }
    case JobEventDetails::JOB_DONE: {
      // Printing is done, we don't need it anymore.
      // print_job_->is_job_pending() may still be true, depending on the order
      // of object registration.
      ReleasePrintJob();
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void PrintViewManager::OnNotifyPrintJobInitEvent(
    const JobEventDetails& event_details) {
  ViewMsg_Print_Params old_print_params(print_params_);

  // Backup the print settings relevant to the renderer.
  DCHECK_EQ(print_job_->document(), event_details.document());
  event_details.document()->settings().RenderParams(&print_params_);
  print_params_.document_cookie = event_details.document()->cookie();
  DCHECK_GT(print_params_.document_cookie, 0);

  // If settings changed
  DCHECK(owner_.render_view_host());
  // Equals() doesn't compare the cookie value.
  if (owner_.render_view_host() &&
      owner_.render_view_host()->IsRenderViewLive() &&
      (!old_print_params.Equals(print_params_) ||
       !event_details.document()->page_count())) {
    // This will generate a DidGetPrintedPagesCount() callback.
    if (!owner_.render_view_host()->GetPrintedPagesCount(print_params_)) {
      NOTREACHED();
      if (inside_inner_message_loop_) {
        MessageLoop::current()->Quit();
        return;
      }
    }
  }

  // Continue even if owner_.render_view_host() is dead because we may already
  // have buffered all the necessary pages.
  switch (event_details.type()) {
    case JobEventDetails::USER_INIT_DONE: {
      // The user clicked the "Print" button in the Print... dialog.
      // Time to print.
      DCHECK_EQ(waiting_to_print_, false);
      DCHECK_EQ(waiting_to_show_print_dialog_, false);
      waiting_to_print_ = true;
      PrintNowInternal();
      break;
    }
    case JobEventDetails::USER_INIT_CANCELED: {
      DCHECK(!waiting_to_show_print_dialog_);
      // The print dialog box has been dismissed (Cancel button or the X).
      TerminatePrintJob(false);
      break;
    }
    case JobEventDetails::DEFAULT_INIT_DONE: {
      // Init(false) returned.
      if (waiting_to_print_) {
        // PrintNow() is pending.
        DCHECK_EQ(waiting_to_show_print_dialog_, false);
        PrintNowInternal();
      } else if (waiting_to_show_print_dialog_ &&
                 event_details.document()->page_count()) {
        // Time to ask the user for the print settings.
        print_job_->GetSettings(PrintJob::ASK_USER,
                                ::GetParent(owner_.GetContainerHWND()));
        waiting_to_show_print_dialog_ = false;
      } else {
        // event_details.document()->page_count() is false. This simply means
        // that the renderer was slower to calculate the number of pages than
        // the print_job_ to initialize the default settings. If so, the dialog
        // will be shown in DidGetPrintedPagesCount() since the settings arrived
        // before the page count.
        DCHECK_EQ(waiting_to_show_print_dialog_, true);
      }
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

bool PrintViewManager::RenderAllMissingPagesNow() {
  if (waiting_to_show_print_dialog_) {
    // TODO(maruel):  http://b/1186708 This happens in one of these case:
    // - javascript:window.print();window.close(); which closes the window very
    //   fast.
    // - The worker thread is hung, like the network printer failed, the network
    //   print server failed or the network cable is disconnected.
    // In the first case we want to wait, but not on the second case.

    if (!RunInnerMessageLoop()) {
      // This function is always called from DisconnectFromCurrentPrintJob()
      // so we know that the job will be stopped/canceled in any case.
      return false;
    }
  }

  if (!print_job_.get() || !print_job_->is_job_pending()) {
    DCHECK_EQ(waiting_to_print_, false);
    return false;
  }

  // We can't print if there is no renderer.
  if (!owner_.render_view_host() ||
      !owner_.render_view_host()->IsRenderViewLive()) {
    waiting_to_print_ = false;
    return false;
  }

  // Is the document already complete?
  if (print_job_->document() && print_job_->document()->IsComplete()) {
    waiting_to_print_ = false;
    return true;
  }

  // WebContents is either dying or a second consecutive request to print
  // happened before the first had time to finish. We need to render all the
  // pages in an hurry if a print_job_ is still pending. No need to wait for it
  // to actually spool the pages, only to have the renderer generate them. Run
  // a message loop until we get our signal that the print job is satisfied.
  // PrintJob will send a ALL_PAGES_REQUESTED after having received all the
  // pages it needs. MessageLoop::current()->Quit() will be called as soon as
  // print_job_->document()->IsComplete() is true on either ALL_PAGES_REQUESTED
  // or in DidPrintPage(). The check is done in
  // ShouldQuitFromInnerMessageLoop().
  // BLOCKS until all the pages are received. (Need to enable recursive task)
  if (!RunInnerMessageLoop()) {
    // This function is always called from DisconnectFromCurrentPrintJob() so we
    // know that the job will be stopped/canceled in any case.
    return false;
  }
  return true;
}

void PrintViewManager::ShouldQuitFromInnerMessageLoop() {
  // Look at the reason.
  DCHECK(print_job_->document());
  if (print_job_->document() &&
      print_job_->document()->IsComplete() &&
      inside_inner_message_loop_) {
    // We are in a message loop created by RenderAllMissingPagesNow. Quit from
    // it.
    MessageLoop::current()->Quit();
    inside_inner_message_loop_ = false;
    waiting_to_print_ = false;
  }
}

bool PrintViewManager::CreateNewPrintJob(PrintJobWorkerOwner* job) {
  DCHECK(!inside_inner_message_loop_);
  if (waiting_to_print_ || waiting_to_show_print_dialog_) {
    // We can't help; we are waiting for a print job initialization. The user is
    // button bashing. The only thing we could do is to batch up the requests.
    return false;
  }

  // Disconnect the current print_job_.
  DisconnectFromCurrentPrintJob();

  // We can't print if there is no renderer.
  if (!owner_.render_view_host() ||
      !owner_.render_view_host()->IsRenderViewLive()) {
    return false;
  }

  // Ask the renderer to generate the print preview, create the print preview
  // view and switch to it, initialize the printer and show the print dialog.
  DCHECK(!print_job_.get());
  if (job) {
    print_job_ = new PrintJob();
    print_job_->Initialize(job, this);
  } else {
    print_job_ = new PrintJob(this);
  }
  NotificationService::current()->
      AddObserver(this,
                  NOTIFY_PRINT_JOB_EVENT,
                  Source<PrintJob>(print_job_.get()));
  return true;
}

void PrintViewManager::DisconnectFromCurrentPrintJob() {
  // Make sure all the necessary rendered page are done. Don't bother with the
  // return value.
  bool result = RenderAllMissingPagesNow();

  // Verify that assertion.
  if (print_job_.get() &&
      print_job_->document() &&
      !print_job_->document()->IsComplete()) {
    DCHECK(!result);
    // That failed.
    TerminatePrintJob(true);
  } else {
    // DO NOT wait for the job to finish.
    ReleasePrintJob();
  }
}

void PrintViewManager::TerminatePrintJob(bool cancel) {
  if (!print_job_.get())
    return;

  if (cancel) {
    // We don't need the EMF data anymore because the printing is canceled.
    print_job_->Cancel();
    waiting_to_print_ = false;
    waiting_to_show_print_dialog_ = false;
    inside_inner_message_loop_ = false;
  } else {
    DCHECK(!inside_inner_message_loop_);
    DCHECK(!waiting_to_show_print_dialog_);
    DCHECK(!print_job_->document() || print_job_->document()->IsComplete() ||
           !waiting_to_print_);

    // WebContents is either dying or navigating elsewhere. We need to render
    // all the pages in an hurry if a print job is still pending. This does the
    // trick since it runs a blocking message loop:
    print_job_->Stop();
  }
  ReleasePrintJob();
}

void PrintViewManager::ReleasePrintJob() {
  DCHECK_EQ(waiting_to_print_, false);
  if (!print_job_.get())
    return;
  NotificationService::current()->RemoveObserver(
      this,
      NOTIFY_PRINT_JOB_EVENT,
      Source<PrintJob>(print_job_.get()));

  print_job_->DisconnectSource();
  // Don't close the worker thread.
  print_job_ = NULL;
  memset(&print_params_, 0, sizeof(print_params_));
}

void PrintViewManager::PrintNowInternal() {
  DCHECK(waiting_to_print_);

  // Settings are already loaded. Go ahead. This will set
  // print_job_->is_job_pending() to true.
  print_job_->StartPrinting();

  if (!print_job_->document() ||
      !print_job_->document()->IsComplete()) {
    ViewMsg_PrintPages_Params params;
    params.params = print_params_;
    params.pages = PageRange::GetPages(print_job_->settings().ranges);
    owner_.render_view_host()->PrintPages(params);
  }
}

bool PrintViewManager::RunInnerMessageLoop() {
  // This value may actually be too low:
  //
  // - If we're looping because of printer settings initializaton, the premise
  // here is that some poor users have their print server away on a VPN over
  // dialup. In this situation, the simple fact of opening the printer can be
  // dead slow. On the other side, we don't want to die infinitely for a real
  // network error. Give the printer 60 seconds to comply.
  //
  // - If we're looping because of renderer page generation, the renderer could
  // be cpu bound, the page overly complex/large or the system just
  // memory-bound.
  static const int kPrinterSettingsTimeout = 60000;
  base::OneShotTimer<MessageLoop> quit_timer;
  quit_timer.Start(TimeDelta::FromMilliseconds(kPrinterSettingsTimeout),
                   MessageLoop::current(), &MessageLoop::Quit);

  inside_inner_message_loop_ = true;

  // Need to enable recursive task.
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  MessageLoop::current()->Run();
  // Restore task state.
  MessageLoop::current()->SetNestableTasksAllowed(old_state);

  bool success = true;
  if (inside_inner_message_loop_) {
    // Ok we timed out. That's sad.
    inside_inner_message_loop_ = false;
    success = false;
  }

  return success;
}

bool PrintViewManager::OpportunisticallyCreatePrintJob(int cookie) {
  if (print_job_.get())
    return true;

  if (!cookie) {
    // Out of sync. It may happens since we are completely asynchronous. Old
    // spurious message can happen if one of the processes is overloaded.
    return false;
  }

  // The job was initiated by a script. Time to get the corresponding worker
  // thread.
  scoped_refptr<PrinterQuery> queued_query;
  g_browser_process->print_job_manager()->PopPrinterQuery(cookie,
                                                          &queued_query);
  DCHECK(queued_query.get());
  if (!queued_query.get())
    return false;

  if (!CreateNewPrintJob(queued_query.get())) {
    // Don't kill anything.
    return false;
  }

  // Settings are already loaded. Go ahead. This will set
  // print_job_->is_job_pending() to true.
  print_job_->StartPrinting();
  return true;
}

}  // namespace printing

