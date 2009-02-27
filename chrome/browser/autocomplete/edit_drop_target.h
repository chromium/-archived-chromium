// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_EDIT_DROP_TARGET_H_
#define CHROME_BROWSER_AUTOCOMPLETE_EDIT_DROP_TARGET_H_

#include "base/base_drop_target.h"

class AutocompleteEditView;

// EditDropTarget is the IDropTarget implementation installed on
// AutocompleteEditView. EditDropTarget prefers URL over plain text. A drop of a
// URL replaces all the text of the edit and navigates immediately to the URL. A
// drop of plain text from the same edit either copies or moves the selected
// text, and a drop of plain text from a source other than the edit does a paste
// and go.
class EditDropTarget : public BaseDropTarget {
 public:
  explicit EditDropTarget(AutocompleteEditView* edit);

 protected:
  virtual DWORD OnDragEnter(IDataObject* data_object,
                            DWORD key_state,
                            POINT cursor_position,
                            DWORD effect);
  virtual DWORD OnDragOver(IDataObject* data_object,
                           DWORD key_state,
                           POINT cursor_position,
                           DWORD effect);
  virtual void OnDragLeave(IDataObject* data_object);
  virtual DWORD OnDrop(IDataObject* data_object,
                       DWORD key_state,
                       POINT cursor_position,
                       DWORD effect);

 private:
  // If dragging a string, the drop highlight position of the edit is reset
  // based on the mouse position.
  void UpdateDropHighlightPosition(const POINT& cursor_screen_position);

  // Resets the visual drop indicates we install on the edit.
  void ResetDropHighlights();

  // The edit we're the drop target for.
  AutocompleteEditView* edit_;

  // If true, the drag session contains a URL.
  bool drag_has_url_;

  // If true, the drag session contains a string. If drag_has_url_ is true,
  // this is false regardless of whether the clipboard has a string.
  bool drag_has_string_;

  DISALLOW_EVIL_CONSTRUCTORS(EditDropTarget);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_EDIT_DROP_TARGET_H_
