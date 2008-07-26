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

#ifndef CHROME_VIEWS_AERO_TOOLTIP_MANAGER_H__
#define CHROME_VIEWS_AERO_TOOLTIP_MANAGER_H__

#include "base/ref_counted.h"
#include "base/task.h"
#include "chrome/views/tooltip_manager.h"

namespace ChromeViews {

///////////////////////////////////////////////////////////////////////////////
// AeroTooltipManager
//
//  Default Windows tooltips are broken when using our custom window frame
//  - as soon as the tooltip receives a WM_MOUSEMOVE event, it starts spewing
//  NCHITTEST messages at its parent window (us). These messages have random
//  x/y coordinates and can't be ignored, as the DwmDefWindowProc uses
//  NCHITTEST  messages to determine how to highlight the caption buttons
//  (the buttons then flicker as the hit tests sent by the user's mouse
//  trigger different effects to those sent by the tooltip).
//
//  So instead, we have to partially implement tooltips ourselves using
//  TTF_TRACKed tooltips.
//
// TODO(glen): Resolve this with Microsoft.
class AeroTooltipManager : public ChromeViews::TooltipManager {
 public:
  AeroTooltipManager(ViewContainer* container, HWND parent);
  virtual ~AeroTooltipManager();

  void OnMouse(UINT u_msg, WPARAM w_param, LPARAM l_param);

 private:
  void Init();
  void OnTimer();

  class InitialTimer : public base::RefCounted<InitialTimer> {
   public:
    InitialTimer(AeroTooltipManager* manager, int time);
    void Disown();
    void Execute();

   private:
    AeroTooltipManager* manager_;
  };

  int initial_delay_;
  scoped_refptr<InitialTimer> initial_timer_;
};

}  // namespace ChromeViews

#endif  // #ifndef CHROME_VIEWS_AERO_TOOLTIP_MANAGER_H__
