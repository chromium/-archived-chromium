// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_OPTIONS_GROUP_VIEW_H__
#define CHROME_BROWSER_VIEWS_OPTIONS_OPTIONS_GROUP_VIEW_H__

#include "views/view.h"

namespace views {
class Label;
class Separator;
};

///////////////////////////////////////////////////////////////////////////////
// OptionsGroupView
//
//  A helper View that gathers related options into groups with a title and
//  optional description.
//
class OptionsGroupView : public views::View {
 public:
  OptionsGroupView(views::View* contents,
                   const std::wstring& title,
                   const std::wstring& description,
                   bool show_separator);
  virtual ~OptionsGroupView() {}

  // Sets the group as being highlighted to attract attention.
  void SetHighlighted(bool highlighted);

  // Retrieves the width of the ContentsView. Used to help size wrapping items.
  int GetContentsWidth() const;

 protected:
  // views::View overrides:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

 private:
  void Init();

  views::View* contents_;
  views::Label* title_label_;
  views::Label* description_label_;
  views::Separator* separator_;

  // True if we should show a separator line below the contents of this
  // section.
  bool show_separator_;

  // True if this section should have a highlighted treatment to draw the
  // user's attention.
  bool highlighted_;

  DISALLOW_EVIL_CONSTRUCTORS(OptionsGroupView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_OPTIONS_OPTIONS_GROUP_VIEW_H__
