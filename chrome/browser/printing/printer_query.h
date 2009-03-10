// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINTER_QUERY_H_
#define CHROME_BROWSER_PRINTING_PRINTER_QUERY_H_

#include "base/scoped_ptr.h"
#include "base/ref_counted.h"
#include "chrome/browser/printing/print_job_worker_owner.h"

class CancelableTask;
class MessageLoop;

namespace base {
class Thread;
}

namespace printing {

class PrintJobWorker;

// Query the printer for settings.
class PrinterQuery : public base::RefCountedThreadSafe<PrinterQuery>,
                     public PrintJobWorkerOwner {
 public:
  // GetSettings() UI parameter.
  enum GetSettingsAskParam {
    DEFAULTS,
    ASK_USER,
  };

  PrinterQuery();
  virtual ~PrinterQuery();

  // PrintJobWorkerOwner
  virtual void AddRef() {
    return base::RefCountedThreadSafe<PrinterQuery>::AddRef();
  }
  virtual void Release() {
    return base::RefCountedThreadSafe<PrinterQuery>::Release();
  }
  virtual void GetSettingsDone(const PrintSettings& new_settings,
                               PrintingContext::Result result);
  virtual PrintJobWorker* DetachWorker(PrintJobWorkerOwner* new_owner);
  virtual MessageLoop* message_loop() {
    return ui_message_loop_;
  }
  virtual const PrintSettings& settings() const { return settings_; }

  virtual int cookie() const { return cookie_; }

  // Initializes the printing context. It is fine to call this function multiple
  // times to reinitialize the settings. |parent_window| parameter will be the
  // owner of the print setting dialog box. It is unused when
  // |ask_for_user_settings| is DEFAULTS.
  void GetSettings(GetSettingsAskParam ask_user_for_settings,
                   HWND parent_window,
                   int expected_page_count,
                   CancelableTask* callback);

  // Stops the worker thread since the client is done with this object.
  void StopWorker();

  // Returns true if the Print... dialog box is currently displayed.
  bool is_print_dialog_box_shown() const;

  // Returns true if a GetSettings() call is pending completion.
  bool is_callback_pending() const;

  PrintingContext::Result last_status() const { return last_status_; }

  // Returns if a worker thread is still associated to this instance.
  bool is_valid() const;

 private:
  // Main message loop reference. Used to send notifications in the right
  // thread.
  MessageLoop* const ui_message_loop_;

  // All the UI is done in a worker thread because many Win32 print functions
  // are blocking and enters a message loop without your consent. There is one
  // worker thread per print job.
  scoped_ptr<PrintJobWorker> worker_;

  // Cache of the print context settings for access in the UI thread.
  PrintSettings settings_;

  // Is the Print... dialog box currently shown.
  bool is_print_dialog_box_shown_;

  // Cookie that make this instance unique.
  int cookie_;

  // Results from the last GetSettingsDone() callback.
  PrintingContext::Result last_status_;

  // Task waiting to be executed.
  scoped_ptr<CancelableTask> callback_;

  DISALLOW_COPY_AND_ASSIGN(PrinterQuery);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINTER_QUERY_H_
