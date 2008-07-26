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

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_OPTIONS_GROUP_VIEW_H__
#define CHROME_BROWSER_VIEWS_OPTIONS_OPTIONS_GROUP_VIEW_H__

#include "chrome/views/view.h"

namespace ChromeViews {
class Label;
class Separator;
};

///////////////////////////////////////////////////////////////////////////////
// OptionsGroupView
//
//  A helper View that gathers related options into groups with a title and
//  optional description.
//
class OptionsGroupView : public ChromeViews::View {
 public:
  OptionsGroupView(ChromeViews::View* contents,
                   const std::wstring& title,
                   const std::wstring& description,
                   bool show_separator);
  virtual ~OptionsGroupView() {}

  // Sets the group as being highlighted to attract attention.
  void SetHighlighted(bool highlighted);

  // Retrieves the width of the ContentsView. Used to help size wrapping items.
  int GetContentsWidth() const;

  class ContentsView : public ChromeViews::View {
   public:
    virtual ~ContentsView() {}

    // ChromeViews::View overrides:
    virtual void DidChangeBounds(const CRect& prev_bounds,
                                 const CRect& next_bounds) {
      Layout();
    }

   private:
    DISALLOW_EVIL_CONSTRUCTORS(ContentsView);
  };

 protected:
  // ChromeViews::View overrides:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void ViewHierarchyChanged(bool is_add,
                                    ChromeViews::View* parent,
                                    ChromeViews::View* child);

 private:
  void Init();

  ChromeViews::View* contents_;
  ChromeViews::Label* title_label_;
  ChromeViews::Label* description_label_;
  ChromeViews::Separator* separator_;

  // True if we should show a separator line below the contents of this
  // section.
  bool show_separator_;

  // True if this section should have a highlighted treatment to draw the
  // user's attention.
  bool highlighted_;

  DISALLOW_EVIL_CONSTRUCTORS(OptionsGroupView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_OPTIONS_OPTIONS_GROUP_VIEW_H__
