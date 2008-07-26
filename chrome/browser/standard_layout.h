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

#ifndef CHROME_BROWSER_STANDARD_LAYOUT_H__
#define CHROME_BROWSER_STANDARD_LAYOUT_H__

#include "chrome/views/grid_layout.h"

//
// This file contains some constants we use to implement our standard panel
// layout.
// see: spec 21/4

// Left or right margin.
const int kPanelHorizMargin = 13;

// Top or bottom margin.
const int kPanelVertMargin = 13;

// If some UI has some sub UI. Indent horizontally by the following value.
const int kPanelHorizIndentation = 24;

// When several controls are aligned vertically, the baseline should be spaced
// by the following number of pixels.
const int kPanelVerticalSpacing = 32;

// Vertical spacing between sub UI.
const int kPanelSubVerticalSpacing = 24;

// Vertical spacing between a label and some control.
const int kLabelToControlVerticalSpacing = 8;

// Vertical spacing between controls that are logically related.
const int kRelatedControlVerticalSpacing = 8;

// Small vertical spacing between controls that are logically related.
const int kRelatedControlSmallVerticalSpacing = 4;

// Vertical spacing between controls that are logically unrelated.
const int kUnrelatedControlVerticalSpacing = 20;

// Larger vertical spacing between unrelated controls.
const int kUnrelatedControlLargeVerticalSpacing = 30;

// Small horizontal spacing between controls that are logically related.
const int kRelatedControlSmallHorizontalSpacing = 8;

// Horizontal spacing between controls that are logically related.
const int kRelatedControlHorizontalSpacing = 8;

// Horizontal spacing between controls that are logically unrelated.
const int kUnrelatedControlHorizontalSpacing = 12;

// Larger horizontal spacing between unrelated controls.
const int kUnrelatedControlLargeHorizontalSpacing = 20;

// Vertical spacing between the edge of the window and the
// top or bottom of a button.
const int kButtonVEdgeMargin = 6;

// Vertical spacing between the edge of the window and the
// left or right of a button.
const int kButtonHEdgeMargin = 7;

// Horizontal spacing between buttons that are logically related.
const int kRelatedButtonHSpacing = 6;

// Creates a GridLayout with kPanel*Margin insets.
static ChromeViews::GridLayout* CreatePanelGridLayout(ChromeViews::View* host) {
  ChromeViews::GridLayout* layout = new ChromeViews::GridLayout(host);
  layout->SetInsets(kPanelVertMargin, kPanelHorizMargin,
                    kPanelVertMargin, kPanelHorizMargin);
  return layout;
}

#endif  // CHROME_BROWSER_STANDARD_LAYOUT_H__
