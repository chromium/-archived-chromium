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

#ifndef CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_OWNER_H__
#define CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_OWNER_H__

#include "chrome/browser/printing/win_printing_context.h"

class MessageLoop;

namespace printing {

class PrintJobWorker;
class PrintSettings;

class PrintJobWorkerOwner {
 public:
  virtual ~PrintJobWorkerOwner() {
  }
  virtual void AddRef() = 0;
  virtual void Release() = 0;

  // Finishes the initialization began by PrintJobWorker::Init(). Creates a
  // new PrintedDocument if necessary. Solely meant to be called by
  // PrintJobWorker.
  virtual void GetSettingsDone(const PrintSettings& new_settings,
                               PrintingContext::Result result) = 0;

  // Detach the PrintJobWorker associated to this object.
  virtual PrintJobWorker* DetachWorker(PrintJobWorkerOwner* new_owner) = 0;

  // Retrieves the message loop that is expected to process GetSettingsDone.
  virtual MessageLoop* message_loop() = 0;

  // Access the current settings.
  virtual const PrintSettings& settings() const = 0;

  // Cookie uniquely identifying the PrintedDocument and/or loaded settings.
  virtual int cookie() const = 0;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_OWNER_H__
