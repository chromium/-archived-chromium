// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printer_query.h"

#include "base/message_loop.h"
#include "chrome/browser/printing/print_job_worker.h"

#ifdef _MSC_VER
#pragma warning(disable:4355)  // 'this' : used in base member initializer list
#endif

namespace printing {

PrinterQuery::PrinterQuery()
    : ui_message_loop_(MessageLoop::current()),
      worker_(new PrintJobWorker(this)),
      is_print_dialog_box_shown_(false),
      last_status_(PrintingContext::FAILED),
      cookie_(PrintSettings::NewCookie()) {
}

PrinterQuery::~PrinterQuery() {
  // The job should be finished (or at least canceled) when it is destroyed.
  DCHECK(!is_print_dialog_box_shown_);
  // If this fires, it is that this pending printer context has leaked.
  DCHECK(!worker_.get());
  if (callback_.get()) {
    // Be sure to cancel it.
    callback_->Cancel();
  }
  // It may get deleted in a different thread that the one that created it.
  // That's fine so don't DCHECK_EQ(ui_message_loop_, MessageLoop::current());
}

void PrinterQuery::GetSettingsDone(const PrintSettings& new_settings,
                                   PrintingContext::Result result) {
  is_print_dialog_box_shown_ = false;
  last_status_ = result;
  if (result != PrintingContext::FAILED) {
    settings_ = new_settings;
    cookie_ = PrintSettings::NewCookie();
  } else {
    // Failure.
    cookie_ = 0;
  }
  if (callback_.get()) {
    // This may cause reentrancy like to call StopWorker().
    callback_->Run();
    callback_.reset(NULL);
  }
}

PrintJobWorker* PrinterQuery::DetachWorker(PrintJobWorkerOwner* new_owner) {
  DCHECK(!callback_.get());
  DCHECK(worker_.get());
  if (!worker_.get())
    return NULL;
  worker_->SetNewOwner(new_owner);
  return worker_.release();
}

void PrinterQuery::GetSettings(GetSettingsAskParam ask_user_for_settings,
                               HWND parent_window,
                               int expected_page_count,
                               CancelableTask* callback) {
  DCHECK_EQ(ui_message_loop_, MessageLoop::current());
  DCHECK(!is_print_dialog_box_shown_);
  DCHECK(!callback_.get());
  DCHECK(worker_.get());
  if (!worker_.get())
    return;
  // Lazy create the worker thread. There is one worker thread per print job.
  if (!worker_->message_loop()) {
    if (!worker_->Start()) {
      if (callback) {
        callback->Cancel();
        delete callback;
      }
      NOTREACHED();
      return;
    }
  }

  callback_.reset(callback);
  // Real work is done in PrintJobWorker::Init().
  is_print_dialog_box_shown_ = ask_user_for_settings == ASK_USER;
  worker_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      worker_.get(),
      &PrintJobWorker::GetSettings,
      is_print_dialog_box_shown_,
      parent_window,
      expected_page_count));
}

void PrinterQuery::StopWorker() {
  if (worker_.get()) {
    worker_->Stop();
    worker_.reset();
  }
}

bool PrinterQuery::is_print_dialog_box_shown() const {
  return is_print_dialog_box_shown_;
}

bool PrinterQuery::is_callback_pending() const {
  return callback_.get() != NULL;
}

bool PrinterQuery::is_valid() const {
  return worker_.get() != NULL;
}

}  // namespace printing
