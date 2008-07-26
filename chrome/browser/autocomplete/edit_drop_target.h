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

#ifndef CHROME_BROWSER_AUTOCOMPLETE_EDIT_DROP_TARGET_H__
#define CHROME_BROWSER_AUTOCOMPLETE_EDIT_DROP_TARGET_H__

#include "base/base_drop_target.h"

class AutocompleteEdit;

// EditDropTarget is the IDropTarget implementation installed on
// AutocompleteEdit. EditDropTarget prefers URL over plain text. A drop of a URL
// replaces all the text of the edit and navigates immediately to the URL. A
// drop of plain text from the same edit either copies or moves the selected
// text, and a drop of plain text from a source other than the edit does a paste
// and go.
class EditDropTarget : public BaseDropTarget {
 public:
  explicit EditDropTarget(AutocompleteEdit* edit);

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
  AutocompleteEdit* edit_;

  // If true, the drag session contains a URL.
  bool drag_has_url_;

  // If true, the drag session contains a string. If drag_has_url_ is true,
  // this is false regardless of whether the clipboard has a string.
  bool drag_has_string_;

  DISALLOW_EVIL_CONSTRUCTORS(EditDropTarget);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_EDIT_DROP_TARGET_H__
