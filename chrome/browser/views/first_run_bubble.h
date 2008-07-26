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

#ifndef CHROME_BROWSER_VIEWS_FIRST_RUN_BUBBLE_H__
#define CHROME_BROWSER_VIEWS_FIRST_RUN_BUBBLE_H__

#include "base/task.h"
#include "chrome/browser/views/info_bubble.h"

class FirstRunBubble : public InfoBubble,
                       public InfoBubbleDelegate {
 public:
  static FirstRunBubble* Show(HWND parent_hwnd,
                              const gfx::Rect& position_relative_to);

  FirstRunBubble()
      : enable_window_method_factory_(this),
        has_been_activated_(false) {
  }

  virtual ~FirstRunBubble() {
    // We should have called RevokeAll on the method factory already.
    DCHECK(enable_window_method_factory_.empty());
    enable_window_method_factory_.RevokeAll();
  }

  // Overridden from InfoBubble:
  virtual void OnActivate(UINT action, BOOL minimized, HWND window);

  // InfoBubbleDelegate.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble);
  virtual bool CloseOnEscape() { return true; }

 private:
  // Re-enable the parent window.
  void EnableParent();

  // Whether we have already been activated.
  bool has_been_activated_;

  ScopedRunnableMethodFactory<FirstRunBubble> enable_window_method_factory_;

  DISALLOW_EVIL_CONSTRUCTORS(FirstRunBubble);
};

#endif  // CHROME_BROWSER_VIEWS_FIRST_RUN_BUBBLE_H__
