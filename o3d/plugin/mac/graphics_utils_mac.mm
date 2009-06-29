/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "plugin/mac/graphics_utils_mac.h"

// Slide a window to a different rect, asynchronously, over a period of time.
void SlideWindowToRect(WindowRef the_window,
                       CGRect destination_rect,
                       double transition_duration_seconds) {
  TransitionWindowOptions options = {0, transition_duration_seconds,
                                     NULL, NULL};
  TransitionWindowWithOptions(the_window,
                              kWindowSlideTransitionEffect,
                              kWindowMoveTransitionAction,
                              &destination_rect, true, &options);
}

// Sets a Carbon window to a custom window level, by creating a new window
// group at that level and adding the window to the group.
void SetWindowLevel(WindowRef window, int level) {
  WindowGroupRef wGroup = NULL;
  WindowGroupAttributes attrs = 0;
  CreateWindowGroup(attrs, &wGroup);
  SetWindowGroupLevel(wGroup, level);
  SetWindowGroup(window, wGroup);
}


// Given a WindowRef and an AGLContext, make the context draw in that window.
// Return Value: true if the window is successfully set, false otherwise.
bool SetWindowForAGLContext(AGLContext context, WindowRef window) {
  return (IsMacOSTenFiveOrHigher()) ?
      aglSetWindowRef(context, window) :
      aglSetDrawable(context, GetWindowPort(window));
}


// Returns whether OS is 10.5 (Leopard) or higher.
bool IsMacOSTenFiveOrHigher() {
  static bool isCached = false, result = false;

  if (!isCached) {
    SInt32 major = 0;
    SInt32 minor = 0;
    // These selectors don't exist pre 10.4 but as we check the error
    // the function will correctly return NO which is the right answer.
    result = ((::Gestalt(gestaltSystemVersionMajor,  &major) == noErr) &&
              (::Gestalt(gestaltSystemVersionMinor,  &minor) == noErr) &&
              ((major > 10) || (major == 10 && minor >= 5)));
    isCached = true;
  }
  return result;
}


Rect CGRect2Rect(const CGRect &inRect) {
  Rect outRect;
  outRect.left = inRect.origin.x;
  outRect.top = inRect.origin.y;
  outRect.right = inRect.origin.x + inRect.size.width;
  outRect.bottom = inRect.origin.y + inRect.size.height;
  return outRect;
}


CGRect Rect2CGRect(const Rect &inRect) {
  CGRect outRect;
  outRect.origin.x = inRect.left;
  outRect.origin.y = inRect.top;
  outRect.size.width = inRect.right - inRect.left;
  outRect.size.height = inRect.bottom - inRect.top;
  return outRect;
}


// Paint a round rect, with the corner radius you specify, either filled or
// stroked.
void PaintRoundedCGRect(CGContextRef context,
                        CGRect rect,
                        float radius,
                        bool fill) {
  CGFloat lx = CGRectGetMinX(rect);
  CGFloat cx = CGRectGetMidX(rect);
  CGFloat rx = CGRectGetMaxX(rect);
  CGFloat by = CGRectGetMinY(rect);
  CGFloat cy = CGRectGetMidY(rect);
  CGFloat ty = CGRectGetMaxY(rect);

  CGContextBeginPath(context);
  CGContextMoveToPoint(context, lx, cy);
  CGContextAddArcToPoint(context, lx, by, cx, by, radius);
  CGContextAddArcToPoint(context, rx, by, rx, cy, radius);
  CGContextAddArcToPoint(context, rx, ty, cx, ty, radius);
  CGContextAddArcToPoint(context, lx, ty, lx, cy, radius);
  CGContextClosePath(context);

  if (fill)
    CGContextFillPath(context);
  else
    CGContextStrokePath(context);
}
