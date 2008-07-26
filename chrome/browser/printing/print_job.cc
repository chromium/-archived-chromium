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

#include "chrome/browser/printing/print_job.h"

#include "base/message_loop.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "chrome/browser/printing/printed_document.h"
#include "chrome/browser/printing/printed_page.h"

#ifdef _MSC_VER
#pragma warning(disable:4355)  // 'this' : used in base member initializer list
#endif

namespace printing {

PrintJob::PrintJob(PrintedPagesSource* source)
    : ui_message_loop_(MessageLoop::current()),
      worker_(new PrintJobWorker(this)),
      source_(source),
      is_job_pending_(false),
      is_print_dialog_box_shown_(false),
      is_blocking_(false),
      is_canceling_(false) {
}

PrintJob::PrintJob()
    : ui_message_loop_(MessageLoop::current()),
      worker_(),
      source_(NULL),
      settings_(),
      is_job_pending_(false),
      is_print_dialog_box_shown_(false),
      is_blocking_(false),
      is_canceling_(false) {
}

PrintJob::~PrintJob() {
  // The job should be finished (or at least canceled) when it is destroyed.
  DCHECK(!is_job_pending_);
  DCHECK(!is_print_dialog_box_shown_);
  DCHECK(!is_blocking_);
  DCHECK(!is_canceling_);
  DCHECK(worker_->message_loop() == NULL);
  DCHECK_EQ(ui_message_loop_, MessageLoop::current());
}

void PrintJob::Initialize(PrintJobWorkerOwner* job,
                          PrintedPagesSource* source) {
  DCHECK(!source_);
  DCHECK(!worker_.get());
  DCHECK(!is_job_pending_);
  DCHECK(!is_print_dialog_box_shown_);
  DCHECK(!is_blocking_);
  DCHECK(!is_canceling_);
  DCHECK(!document_.get());
  source_ = source;
  worker_.reset(job->DetachWorker(this));
  settings_ = job->settings();

  UpdatePrintedDocument(new PrintedDocument(settings_, source_, job->cookie()));

  // Don't forget to register to our own messages.
  NotificationService::current()->AddObserver(
      this, NOTIFY_PRINT_JOB_EVENT, Source<PrintJob>(this));
}

void PrintJob::Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
  DCHECK_EQ(ui_message_loop_, MessageLoop::current());
  switch (type) {
    case NOTIFY_PRINTED_DOCUMENT_UPDATED: {
      DCHECK(Source<PrintedDocument>(source).ptr() ==
             document_.get());

      // This notification may happens even if no job is started (i.e. print
      // preview)
      if (is_job_pending_ == true &&
          Source<PrintedDocument>(source).ptr() == document_.get() &&
          Details<PrintedPage>(details).ptr() != NULL) {
        // Are we waiting for a page to print? The worker will know.
        worker_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
            worker_.get(), &PrintJobWorker::OnNewPage));
      }
      break;
    }
    case NOTIFY_PRINT_JOB_EVENT: {
      OnNotifyPrintJobEvent(*Details<JobEventDetails>(details).ptr());
      break;
    }
    default: {
      break;
    }
  }
}

void PrintJob::GetSettingsDone(const PrintSettings& new_settings,
                               PrintingContext::Result result) {
  DCHECK(!is_job_pending_);
  DCHECK(!is_blocking_);

  if (!source_ || result == PrintingContext::FAILED) {
    // The source is gone, there's nothing to do.
    Cancel();
    return;
  }

  // Only create a new PrintedDocument if the settings have changed or if
  // there was no printed document.
  if (!document_.get() || !new_settings.Equals(settings_)) {
    UpdatePrintedDocument(new PrintedDocument(new_settings, source_,
                                              PrintSettings::NewCookie()));
  }

  JobEventDetails::Type type;
  if (is_print_dialog_box_shown_) {
    type = (result == PrintingContext::OK) ?
               JobEventDetails::USER_INIT_DONE :
               JobEventDetails::USER_INIT_CANCELED;
    // Dialog box is not shown anymore.
    is_print_dialog_box_shown_ = false;
  } else {
    DCHECK_EQ(result, PrintingContext::OK);
    type = JobEventDetails::DEFAULT_INIT_DONE;
  }
  scoped_refptr<JobEventDetails> details(
      new JobEventDetails(type, document_.get(), NULL));
  NotificationService::current()->Notify(
      NOTIFY_PRINT_JOB_EVENT,
      Source<PrintJob>(this),
      Details<JobEventDetails>(details.get()));
}

PrintJobWorker* PrintJob::DetachWorker(PrintJobWorkerOwner* new_owner) {
  NOTREACHED();
  return NULL;
}

int PrintJob::cookie() const {
  if (!document_.get())
    // Always use an invalid cookie in this case.
    return 0;
  return document_->cookie();
}

void PrintJob::GetSettings(GetSettingsAskParam ask_user_for_settings,
                           HWND parent_window) {
  DCHECK_EQ(ui_message_loop_, MessageLoop::current());
  DCHECK(!is_job_pending_);
  DCHECK(!is_print_dialog_box_shown_);
  DCHECK(!is_blocking_);
  // Is not reentrant.
  if (is_job_pending_ || is_blocking_)
    return;

  // Lazy create the worker thread. There is one worker thread per print job.
  if (!worker_->message_loop()) {
    if (!worker_->Start())
      return;

    // Don't re-register if we were already registered.
    NotificationService::current()->AddObserver(
        this, NOTIFY_PRINT_JOB_EVENT, Source<PrintJob>(this));
  }

  int page_count = 0;
  if (document_.get()) {
    page_count = document_->page_count();
  }

  // Real work is done in PrintJobWorker::Init().
  is_print_dialog_box_shown_ = ask_user_for_settings == ASK_USER;
  worker_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      worker_.get(), &PrintJobWorker::GetSettings, is_print_dialog_box_shown_,
      parent_window, page_count));
}

void PrintJob::StartPrinting() {
  DCHECK_EQ(ui_message_loop_, MessageLoop::current());
  DCHECK(worker_->message_loop());
  DCHECK(!is_job_pending_);
  DCHECK(!is_print_dialog_box_shown_);
  DCHECK(!is_blocking_);
  if (!worker_->message_loop() || is_job_pending_ || is_blocking_)
    return;

  // Real work is done in PrintJobWorker::StartPrinting().
  worker_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      worker_.get(), &PrintJobWorker::StartPrinting, document_));
  // Set the flag right now.
  is_job_pending_ = true;

  // Tell everyone!
  scoped_refptr<JobEventDetails> details(
      new JobEventDetails(JobEventDetails::NEW_DOC, document_.get(), NULL));
  NotificationService::current()->Notify(
      NOTIFY_PRINT_JOB_EVENT,
      Source<PrintJob>(this),
      Details<JobEventDetails>(details.get()));
}

void PrintJob::Stop() {
  DCHECK_EQ(ui_message_loop_, MessageLoop::current());

  // Be sure to live long enough.
  scoped_refptr<PrintJob> handle(this);

  MessageLoop* worker_loop = worker_->message_loop();
  if (worker_loop) {
    if (is_print_dialog_box_shown_) {
      // Make sure there is no dialog box.
      worker_loop->PostTask(FROM_HERE, NewRunnableMethod(
          worker_.get(), &PrintJobWorker::DismissDialog));
      is_print_dialog_box_shown_ = false;
    }
    // It will wait infinitely for the worker thread to quit.
    worker_->NonBlockingStop();
    is_job_pending_ = false;
    NotificationService::current()->RemoveObserver(
      this, NOTIFY_PRINT_JOB_EVENT, Source<PrintJob>(this));
  }
  // Flush the cached document.
  UpdatePrintedDocument(NULL);

  if (is_blocking_) {
    // Make sure we don't get stuck in an inner message loop.
    MessageLoop::current()->Quit();
    is_blocking_ = false;
  }
}

void PrintJob::Cancel() {
  if (is_canceling_)
    return;
  is_canceling_ = true;

  // Be sure to live long enough.
  scoped_refptr<PrintJob> handle(this);

  DCHECK_EQ(ui_message_loop_, MessageLoop::current());
  MessageLoop* worker_loop = worker_.get() ? worker_->message_loop() : NULL;
  if (worker_loop) {
    // Call this right now so it renders the context invalid. Do not use
    // InvokeLater since it would take too much time.
    worker_->Cancel();
  }
  // Make sure a Cancel() is broadcast.
  scoped_refptr<JobEventDetails> details(
      new JobEventDetails(JobEventDetails::FAILED, NULL, NULL));
  NotificationService::current()->Notify(
      NOTIFY_PRINT_JOB_EVENT,
      Source<PrintJob>(this),
      Details<JobEventDetails>(details.get()));
  Stop();
  is_canceling_ = false;
}

bool PrintJob::RequestMissingPages() {
  DCHECK_EQ(ui_message_loop_, MessageLoop::current());
  DCHECK(!is_print_dialog_box_shown_);
  DCHECK(!is_blocking_);
  if (!is_job_pending_ || is_print_dialog_box_shown_ || is_blocking_)
    return false;

  MessageLoop* worker_loop = worker_.get() ? worker_->message_loop() : NULL;
  if (!worker_loop)
    return false;

  worker_loop->PostTask(FROM_HERE, NewRunnableMethod(
      worker_.get(), &PrintJobWorker::RequestMissingPages));
  return true;
}

bool PrintJob::FlushJob(int timeout_ms) {
  if (!RequestMissingPages())
    return false;

  // Make sure the object outlive this message loop.
  scoped_refptr<PrintJob> handle(this);

  MessageLoop::QuitTask timeout_task;
  scoped_ptr<Timer> timeout;
  if (timeout_ms) {
    timeout.reset(MessageLoop::current()->timer_manager()->StartTimer(
        timeout_ms,
        &timeout_task,
        false));
  }

  is_blocking_ = true;
  // Stop() will eventually be called, which will get out of the inner message
  // loop. But, don't take it for granted and set a timer in case something goes
  // wrong.

  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  MessageLoop::current()->Run();
  // Restore task state.
  MessageLoop::current()->SetNestableTasksAllowed(old_state);

  if (timeout.get()) {
    MessageLoop::current()->timer_manager()->StopTimer(timeout.get());
  }
  return true;
}

void PrintJob::DisconnectSource() {
  source_ = NULL;
  if (document_.get())
    document_->DisconnectSource();
}

bool PrintJob::is_job_pending() const {
  return is_job_pending_;
}

bool PrintJob::is_print_dialog_box_shown() const {
  return is_print_dialog_box_shown_;
}

PrintedDocument* PrintJob::document() const {
  return document_.get();
}

void PrintJob::UpdatePrintedDocument(PrintedDocument* new_document) {
  if (document_.get() == new_document)
    return;
  // Unregisters.
  if (document_.get()) {
    NotificationService::current()->
        RemoveObserver(this,
                       NOTIFY_PRINTED_DOCUMENT_UPDATED,
                       Source<PrintedDocument>(document_.get()));
  }
  document_ = new_document;

  // Registers.
  if (document_.get()) {
    NotificationService::current()->
        AddObserver(this,
                    NOTIFY_PRINTED_DOCUMENT_UPDATED,
                    Source<PrintedDocument>(document_.get()));
    settings_ = document_->settings();
  }

  if (worker_.get() && worker_->message_loop()) {
    DCHECK(!is_job_pending_);
    // Sync the document with the worker.
    worker_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        worker_.get(), &PrintJobWorker::OnDocumentChanged, document_));
  }
}

void PrintJob::OnNotifyPrintJobEvent(const JobEventDetails& event_details) {
  switch (event_details.type()) {
    case JobEventDetails::FAILED: {
      settings_.Clear();
      // Update internal state.
      is_print_dialog_box_shown_ = false;
      // No need to cancel since the worker already canceled itself.
      Stop();
      break;
    }
    case JobEventDetails::USER_INIT_DONE:
    case JobEventDetails::DEFAULT_INIT_DONE:
    case JobEventDetails::USER_INIT_CANCELED: {
      DCHECK_EQ(event_details.document(), document_.get());
      break;
    }
    case JobEventDetails::NEW_DOC:
    case JobEventDetails::NEW_PAGE:
    case JobEventDetails::PAGE_DONE:
    case JobEventDetails::JOB_DONE:
    case JobEventDetails::ALL_PAGES_REQUESTED: {
      // Don't care.
      break;
    }
    case JobEventDetails::DOC_DONE: {
      // This will call Stop() and broadcast a JOB_DONE message.
      MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
          this, &PrintJob::OnDocumentDone));
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void PrintJob::OnDocumentDone() {
  // Be sure to live long enough. The instance could be destroyed by the
  // JOB_DONE broadcast.
  scoped_refptr<PrintJob> handle(this);

  // Stop the worker thread.
  Stop();

  scoped_refptr<JobEventDetails> details(
      new JobEventDetails(JobEventDetails::JOB_DONE, document_.get(), NULL));
  NotificationService::current()->Notify(
      NOTIFY_PRINT_JOB_EVENT,
      Source<PrintJob>(this),
      Details<JobEventDetails>(details.get()));
}

// Takes settings_ ownership and will be deleted in the receiving thread.
JobEventDetails::JobEventDetails(Type type,
                                 PrintedDocument* document,
                                 PrintedPage* page)
    : document_(document),
      page_(page),
      type_(type) {
}

JobEventDetails::~JobEventDetails() {
}

PrintedDocument* JobEventDetails::document() const {
  return document_;
}

PrintedPage* JobEventDetails::page() const {
  return page_;
}

}  // namespace printing
