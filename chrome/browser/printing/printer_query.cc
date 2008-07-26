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
