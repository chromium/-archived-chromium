// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_WIDGET_AERO_TOOLTIP_MANAGER_H_
#define CHROME_VIEWS_WIDGET_AERO_TOOLTIP_MANAGER_H_

#include "base/ref_counted.h"
#include "base/task.h"
#include "chrome/views/widget/tooltip_manager.h"

namespace views {

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
class AeroTooltipManager : public TooltipManager {
 public:
  AeroTooltipManager(Widget* widget, HWND parent);
  virtual ~AeroTooltipManager();

  virtual void OnMouse(UINT u_msg, WPARAM w_param, LPARAM l_param);
  virtual void OnMouseLeave();

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

}  // namespace views

#endif  // #ifndef CHROME_VIEWS_WIDGET_AERO_TOOLTIP_MANAGER_H_
