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

#ifndef CHROME_VIEWS_ACCELERATOR_HANDLER_H__
#define CHROME_VIEWS_ACCELERATOR_HANDLER_H__

#include "base/message_loop.h"

namespace ChromeViews {

// This class delegates WM_KEYDOWN and WM_SYSKEYDOWN messages to
// the associated FocusManager class for the window that is receiving
// these messages for accelerator processing. The BrowserProcess object
// holds a singleton instance of this class which can be used by other
// custom message loop dispatcher objects to implement default accelerator
// handling.
class AcceleratorHandler : public MessageLoop::Dispatcher {
 public:
   AcceleratorHandler();
  // Dispatcher method. This returns true if an accelerator was
  // processed by the focus manager
  virtual bool Dispatch(const MSG& msg);
 private:
  DISALLOW_EVIL_CONSTRUCTORS(AcceleratorHandler);
};
}
#endif  // CHROME_VIEWS_ACCELERATOR_HANDLER_H__
