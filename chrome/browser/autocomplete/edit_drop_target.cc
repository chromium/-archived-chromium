// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/edit_drop_target.h"

#include "base/string_util.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/common/os_exchange_data.h"
#include "googleurl/src/gurl.h"

namespace {

// A helper method for determining a valid DROPEFFECT given the allowed
// DROPEFFECTS.  We prefer copy over link.
DWORD CopyOrLinkDropEffect(DWORD effect) {
  if (effect & DROPEFFECT_COPY)
    return DROPEFFECT_COPY;
  if (effect & DROPEFFECT_LINK)
    return DROPEFFECT_LINK;
  return DROPEFFECT_NONE;
}

}

EditDropTarget::EditDropTarget(AutocompleteEditView* edit)
    : BaseDropTarget(edit->m_hWnd),
      edit_(edit),
      drag_has_url_(false),
      drag_has_string_(false) {
}

DWORD EditDropTarget::OnDragEnter(IDataObject* data_object,
                                  DWORD key_state,
                                  POINT cursor_position,
                                  DWORD effect) {
  OSExchangeData os_data(data_object);
  drag_has_url_ = os_data.HasURL();
  drag_has_string_ = !drag_has_url_ && os_data.HasString();
  if (drag_has_url_) {
    if (edit_->in_drag()) {
      // The edit we're associated with originated the drag. No point in
      // allowing the user to drop back on us.
      drag_has_url_ = false;
    }
    // NOTE: it would be nice to visually show all the text is going to
    // be replaced by selecting all, but this caused painting problems. In
    // particular the flashing caret would appear outside the edit! For now
    // we stick with no visual indicator other than that shown own the mouse
    // cursor.
  }
  return OnDragOver(data_object, key_state, cursor_position, effect);
}

DWORD EditDropTarget::OnDragOver(IDataObject* data_object,
                                 DWORD key_state,
                                 POINT cursor_position,
                                 DWORD effect) {
  if (drag_has_url_)
    return CopyOrLinkDropEffect(effect);

  if (drag_has_string_) {
    UpdateDropHighlightPosition(cursor_position);
    if (edit_->drop_highlight_position() == -1 && edit_->in_drag())
      return DROPEFFECT_NONE;
    if (edit_->in_drag()) {
      // The edit we're associated with originated the drag.  Do the normal drag
      // behavior.
      DCHECK((effect & DROPEFFECT_COPY) && (effect & DROPEFFECT_MOVE));
      return (key_state & MK_CONTROL) ? DROPEFFECT_COPY : DROPEFFECT_MOVE;
    }
    // Our edit didn't originate the drag, only allow link or copy.
    return CopyOrLinkDropEffect(effect);
  }

  return DROPEFFECT_NONE;
}

void EditDropTarget::OnDragLeave(IDataObject* data_object) {
  ResetDropHighlights();
}

DWORD EditDropTarget::OnDrop(IDataObject* data_object,
                             DWORD key_state,
                             POINT cursor_position,
                             DWORD effect) {
  OSExchangeData os_data(data_object);

  if (drag_has_url_) {
    GURL url;
    std::wstring title;
    if (os_data.GetURLAndTitle(&url, &title)) {
      edit_->SetUserText(UTF8ToWide(url.spec()));
      edit_->model()->AcceptInput(CURRENT_TAB, true);
      return CopyOrLinkDropEffect(effect);
    }
  } else if (drag_has_string_) {
    int string_drop_position = edit_->drop_highlight_position();
    std::wstring text;
    if ((string_drop_position != -1 || !edit_->in_drag()) &&
        os_data.GetString(&text)) {
      DCHECK(string_drop_position == -1 ||
             ((string_drop_position >= 0) &&
              (string_drop_position <= edit_->GetTextLength())));
      const DWORD drop_operation =
          OnDragOver(data_object, key_state, cursor_position, effect);
      if (edit_->in_drag()) {
        if (drop_operation == DROPEFFECT_MOVE)
          edit_->MoveSelectedText(string_drop_position);
        else
          edit_->InsertText(string_drop_position, text);
      } else {
        edit_->PasteAndGo(CollapseWhitespace(text, true));
      }
      ResetDropHighlights();
      return drop_operation;
    }
  }

  ResetDropHighlights();

  return DROPEFFECT_NONE;
}

void EditDropTarget::UpdateDropHighlightPosition(
    const POINT& cursor_screen_position) {
  if (drag_has_string_) {
    POINT client_position = cursor_screen_position;
    ScreenToClient(edit_->m_hWnd, &client_position);
    int drop_position = edit_->CharFromPos(client_position);
    if (edit_->in_drag()) {
      // Our edit originated the drag, don't allow a drop if over the selected
      // region.
      LONG sel_start, sel_end;
      edit_->GetSel(sel_start, sel_end);
      if ((sel_start != sel_end) && (drop_position >= sel_start) &&
          (drop_position <= sel_end))
        drop_position = -1;
    } else {
      // A drop from a source other than the edit replaces all the text, so
      // we don't show the drop location. See comment in OnDragEnter as to why
      // we don't try and select all here.
      drop_position = -1;
    }
    edit_->SetDropHighlightPosition(drop_position);
  }
}

void EditDropTarget::ResetDropHighlights() {
  if (drag_has_string_)
    edit_->SetDropHighlightPosition(-1);
}
