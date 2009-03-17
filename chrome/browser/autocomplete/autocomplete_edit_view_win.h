// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_WIN_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_WIN_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atlmisc.h>
#include <tom.h>  // For ITextDocument, a COM interface to CRichEditCtrl.

#include "base/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/views/controls/menu/menu.h"
#include "webkit/glue/window_open_disposition.h"

class AutocompletePopupModel;
class CommandUpdater;
class Profile;
class TabContents;
namespace views {
class View;
}

class AutocompleteEditController;
class AutocompleteEditModel;
class AutocompleteEditView;
class AutocompletePopupViewWin;

// Provides the implementation of an edit control with a drop-down
// autocomplete box. The box itself is implemented in autocomplete_popup.cc
// This file implements the edit box and management for the popup.
class AutocompleteEditViewWin
    : public CWindowImpl<AutocompleteEditViewWin,
                         CRichEditCtrl,
                         CWinTraits<WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL |
                                    ES_NOHIDESEL> >,
      public CRichEditCommands<AutocompleteEditViewWin>,
      public Menu::Delegate,
      public AutocompleteEditView {
 public:
  struct State {
    State(const CHARRANGE& selection,
          const CHARRANGE& saved_selection_for_focus_change)
        : selection(selection),
          saved_selection_for_focus_change(saved_selection_for_focus_change) {
    }

    const CHARRANGE selection;
    const CHARRANGE saved_selection_for_focus_change;
  };

  DECLARE_WND_CLASS(L"Chrome_AutocompleteEditView");

  AutocompleteEditViewWin(const ChromeFont& font,
                          AutocompleteEditController* controller,
                          ToolbarModel* toolbar_model,
                          views::View* parent_view,
                          HWND hwnd,
                          Profile* profile,
                          CommandUpdater* command_updater,
                          bool popup_window_mode);
  ~AutocompleteEditViewWin();

  views::View* parent_view() const { return parent_view_; }

  // Implement the AutocompleteEditView interface.
  virtual AutocompleteEditModel* model() { return model_.get(); }
  virtual const AutocompleteEditModel* model() const { return model_.get(); }

  virtual void SaveStateToTab(TabContents* tab);

  virtual void Update(const TabContents* tab_for_state_restoring);

  virtual void OpenURL(const GURL& url,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition,
                       const GURL& alternate_nav_url,
                       size_t selected_line,
                       const std::wstring& keyword);

  virtual std::wstring GetText() const;

  virtual void SetUserText(const std::wstring& text) {
    SetUserText(text, text, true);
  }
  virtual void SetUserText(const std::wstring& text,
                           const std::wstring& display_text,
                           bool update_popup);

  virtual void SetWindowTextAndCaretPos(const std::wstring& text,
                                        size_t caret_pos);

  virtual bool IsSelectAll();
  virtual void SelectAll(bool reversed);
  virtual void RevertAll();

  virtual void UpdatePopup();
  virtual void ClosePopup();

  virtual void OnTemporaryTextMaybeChanged(const std::wstring& display_text,
                                           bool save_original_selection);
  virtual bool OnInlineAutocompleteTextMaybeChanged(
      const std::wstring& display_text, size_t user_text_length);
  virtual void OnRevertTemporaryText();
  virtual void OnBeforePossibleChange();
  virtual bool OnAfterPossibleChange();

  // Exposes custom IAccessible implementation to the overall MSAA hierarchy.
  IAccessible* GetIAccessible();

  void SetDropHighlightPosition(int position);
  int drop_highlight_position() const { return drop_highlight_position_; }

  // Returns true if a drag a drop session was initiated by this edit.
  bool in_drag() const { return in_drag_; }

  // Moves the selected text to the specified position.
  void MoveSelectedText(int new_position);

  // Inserts the text at the specified position.
  void InsertText(int position, const std::wstring& text);

  // Invokes CanPasteAndGo with the specified text, and if successful navigates
  // to the appropriate URL. The behavior of this is the same as if the user
  // typed in the specified text and pressed enter.
  void PasteAndGo(const std::wstring& text);

  void set_force_hidden(bool force_hidden) { force_hidden_ = force_hidden; }

  // Called before an accelerator is processed to give us a chance to override
  // it.
  bool OverrideAccelerator(const views::Accelerator& accelerator);

  // Handler for external events passed in to us.  The View that owns us may
  // send us events that we should treat as if they were events on us.
  void HandleExternalMsg(UINT msg, UINT flags, const CPoint& screen_point);

  // CWindowImpl
  BEGIN_MSG_MAP(AutocompleteEdit)
    MSG_WM_CHAR(OnChar)
    MSG_WM_CONTEXTMENU(OnContextMenu)
    MSG_WM_COPY(OnCopy)
    MSG_WM_CUT(OnCut)
    MESSAGE_HANDLER_EX(WM_GETOBJECT, OnGetObject)
    MESSAGE_HANDLER_EX(WM_IME_COMPOSITION, OnImeComposition)
    MSG_WM_KEYDOWN(OnKeyDown)
    MSG_WM_KEYUP(OnKeyUp)
    MSG_WM_KILLFOCUS(OnKillFocus)
    MSG_WM_LBUTTONDBLCLK(OnLButtonDblClk)
    MSG_WM_LBUTTONDOWN(OnLButtonDown)
    MSG_WM_LBUTTONUP(OnLButtonUp)
    MSG_WM_MBUTTONDOWN(OnNonLButtonDown)
    MSG_WM_MBUTTONUP(OnNonLButtonUp)
    MSG_WM_MOUSEACTIVATE(OnMouseActivate)
    MSG_WM_MOUSEMOVE(OnMouseMove)
    MSG_WM_PAINT(OnPaint)
    MSG_WM_PASTE(OnPaste)
    MSG_WM_RBUTTONDOWN(OnNonLButtonDown)
    MSG_WM_RBUTTONUP(OnNonLButtonUp)
    MSG_WM_SETFOCUS(OnSetFocus)
    MSG_WM_SYSCHAR(OnSysChar)  // WM_SYSxxx == WM_xxx with ALT down
    MSG_WM_SYSKEYDOWN(OnKeyDown)
    MSG_WM_SYSKEYUP(OnKeyUp)
    MSG_WM_WINDOWPOSCHANGING(OnWindowPosChanging)
    DEFAULT_REFLECTION_HANDLER()  // avoids black margin area
  END_MSG_MAP()

  // Menu::Delegate
  virtual bool IsCommandEnabled(int id) const;
  virtual bool GetContextualLabel(int id, std::wstring* out) const;
  virtual void ExecuteCommand(int id);

 private:
  // This object freezes repainting of the edit until the object is destroyed.
  // Some methods of the CRichEditCtrl draw synchronously to the screen.  If we
  // don't freeze, the user will see a rapid series of calls to these as
  // flickers.
  //
  // Freezing the control while it is already frozen is permitted; the control
  // will unfreeze once both freezes are released (the freezes stack).
  class ScopedFreeze {
   public:
    ScopedFreeze(AutocompleteEditViewWin* edit,
                 ITextDocument* text_object_model);
    ~ScopedFreeze();

   private:
    AutocompleteEditViewWin* const edit_;
    ITextDocument* const text_object_model_;

    DISALLOW_COPY_AND_ASSIGN(ScopedFreeze);
  };

  // This object suspends placing any operations on the edit's undo stack until
  // the object is destroyed.  If we don't do this, some of the operations we
  // perform behind the user's back will be undoable by the user, which feels
  // bizarre and confusing.
  class ScopedSuspendUndo {
   public:
    explicit ScopedSuspendUndo(ITextDocument* text_object_model);
    ~ScopedSuspendUndo();

   private:
    ITextDocument* const text_object_model_;

    DISALLOW_COPY_AND_ASSIGN(ScopedSuspendUndo);
  };

  // Replacement word-breaking proc for the rich edit control.
  static int CALLBACK WordBreakProc(LPTSTR edit_text,
                                    int current_pos,
                                    int num_bytes,
                                    int action);

  // Returns true if |edit_text| starting at |current_pos| is "://".
  static bool SchemeEnd(LPTSTR edit_text, int current_pos, int length);

  // Message handlers
  void OnChar(TCHAR ch, UINT repeat_count, UINT flags);
  void OnContextMenu(HWND window, const CPoint& point);
  void OnCopy();
  void OnCut();
  LRESULT OnGetObject(UINT uMsg, WPARAM wparam, LPARAM lparam);
  LRESULT OnImeComposition(UINT message, WPARAM wparam, LPARAM lparam);
  void OnKeyDown(TCHAR key, UINT repeat_count, UINT flags);
  void OnKeyUp(TCHAR key, UINT repeat_count, UINT flags);
  void OnKillFocus(HWND focus_wnd);
  void OnLButtonDblClk(UINT keys, const CPoint& point);
  void OnLButtonDown(UINT keys, const CPoint& point);
  void OnLButtonUp(UINT keys, const CPoint& point);
  LRESULT OnMouseActivate(HWND window, UINT hit_test, UINT mouse_message);
  void OnMouseMove(UINT keys, const CPoint& point);
  void OnNonLButtonDown(UINT keys, const CPoint& point);
  void OnNonLButtonUp(UINT keys, const CPoint& point);
  void OnPaint(HDC bogus_hdc);
  void OnPaste();
  void OnSetFocus(HWND focus_wnd);
  void OnSysChar(TCHAR ch, UINT repeat_count, UINT flags);
  void OnWindowPosChanging(WINDOWPOS* window_pos);

  // Helper function for OnChar() and OnKeyDown() that handles keystrokes that
  // could change the text in the edit.
  void HandleKeystroke(UINT message, TCHAR key, UINT repeat_count, UINT flags);

  // Helper functions for OnKeyDown() that handle accelerators applicable when
  // we're not read-only and all the time, respectively.  These return true if
  // they handled the key.
  bool OnKeyDownOnlyWritable(TCHAR key, UINT repeat_count, UINT flags);
  bool OnKeyDownAllModes(TCHAR key, UINT repeat_count, UINT flags);

  // Like GetSel(), but returns a range where |cpMin| will be larger than
  // |cpMax| if the cursor is at the start rather than the end of the selection
  // (in other words, tracks selection direction as well as offsets).
  // Note the non-Google-style "non-const-ref" argument, which matches GetSel().
  void GetSelection(CHARRANGE& sel) const;

  // Returns the currently selected text of the edit control.
  std::wstring GetSelectedText() const;

  // Like SetSel(), but respects the selection direction implied by |start| and
  // |end|: if |end| < |start|, the effective cursor will be placed at the
  // beginning of the selection.
  void SetSelection(LONG start, LONG end);

  // Like SetSelection(), but takes a CHARRANGE.
  void SetSelectionRange(const CHARRANGE& sel) {
    SetSelection(sel.cpMin, sel.cpMax);
  }

  // Places the caret at the given position.  This clears any selection.
  void PlaceCaretAt(size_t pos);

  // Returns true if |sel| represents a forward or backward selection of all the
  // text.
  bool IsSelectAllForRange(const CHARRANGE& sel) const;

  // Given an X coordinate in client coordinates, returns that coordinate
  // clipped to be within the horizontal bounds of the visible text.
  //
  // This is used in our mouse handlers to work around quirky behaviors of the
  // underlying CRichEditCtrl like not supporting triple-click when the user
  // doesn't click on the text itself.
  //
  // |is_triple_click| should be true iff this is the third click of a triple
  // click.  Sadly, we need to clip slightly differently in this case.
  LONG ClipXCoordToVisibleText(LONG x, bool is_triple_click) const;

  // Parses the contents of the control for the scheme and the host name.
  // Highlights the scheme in green or red depending on it security level.
  // If a host name is found, it makes it visually stronger.
  void EmphasizeURLComponents();

  // Erases the portion of the selection in the font's y-adjustment area.  For
  // some reason the edit draws the selection rect here even though it's not
  // part of the font.
  void EraseTopOfSelection(CDC* dc,
                           const CRect& client_rect,
                           const CRect& paint_clip_rect);

  // Draws a slash across the scheme if desired.
  void DrawSlashForInsecureScheme(HDC hdc,
                                  const CRect& client_rect,
                                  const CRect& paint_clip_rect);

  // Renders the drop highlight.
  void DrawDropHighlight(HDC hdc,
                         const CRect& client_rect,
                         const CRect& paint_clip_rect);

  // Internally invoked whenever the text changes in some way.
  void TextChanged();

  // Returns the current clipboard contents as a string that can be pasted in.
  // In addition to just getting CF_UNICODETEXT out, this can also extract URLs
  // from bookmarks on the clipboard.
  std::wstring GetClipboardText() const;

  // Determines whether the user can "paste and go", given the specified text.
  bool CanPasteAndGo(const std::wstring& text) const;

  // Getter for the text_object_model_, used by the ScopedXXX classes.  Note
  // that the pointer returned here is only valid as long as the
  // AutocompleteEdit is still alive.  Also, if the underlying call fails, this
  // may return NULL.
  ITextDocument* GetTextObjectModel() const;

  // Invoked during a mouse move. As necessary starts a drag and drop session.
  void StartDragIfNecessary(const CPoint& point);

  // Invoked during a mouse down. If the mouse location is over the selection
  // this sets possible_drag_ to true to indicate a drag should start if the
  // user moves the mouse far enough to start a drag.
  void OnPossibleDrag(const CPoint& point);

  // Invoked when a mouse button is released. If none of the buttons are still
  // down, this sets possible_drag_ to false.
  void UpdateDragDone(UINT keys);

  // Redraws the necessary region for a drop highlight at the specified
  // position. This does nothing if position is beyond the bounds of the
  // text.
  void RepaintDropHighlight(int position);

  scoped_ptr<AutocompleteEditModel> model_;

  scoped_ptr<AutocompletePopupViewWin> popup_view_;

  AutocompleteEditController* controller_;

  // The parent view for the edit, used to align the popup and for
  // accessibility.
  views::View* parent_view_;

  ToolbarModel* toolbar_model_;

  // The object that handles additional command functionality exposed on the
  // edit, such as invoking the keyword editor.
  CommandUpdater* command_updater_;

  // When true, the location bar view is read only and also is has a slightly
  // different presentation (font size / color). This is used for popups.
  bool popup_window_mode_;

  // True if we should prevent attempts to make the window visible when we
  // handle WM_WINDOWPOSCHANGING.  While toggling fullscreen mode, the main
  // window is hidden, and if the edit is shown it will draw over the main
  // window when that window reappears.
  bool force_hidden_;

  // Non-null when the edit is gaining focus from a left click.  This is only
  // needed between when WM_MOUSEACTIVATE and WM_LBUTTONDOWN get processed.  It
  // serves two purposes: first, by communicating to OnLButtonDown() that we're
  // gaining focus from a left click, it allows us to work even with the
  // inconsistent order in which various Windows messages get sent (see comments
  // in OnMouseActivate()).  Second, by holding the edit frozen, it ensures that
  // when we process WM_SETFOCUS the edit won't first redraw itself with the
  // caret at the beginning, and then have it blink to where the mouse cursor
  // really is shortly afterward.
  scoped_ptr<ScopedFreeze> gaining_focus_;

  // When the user clicks to give us focus, we watch to see if they're clicking
  // or dragging.  When they're clicking, we select nothing until mouseup, then
  // select all the text in the edit.  During this process, tracking_click_ is
  // true and mouse_down_point_ holds the original click location.  At other
  // times, tracking_click_ is false, and the contents of mouse_down_point_
  // should be ignored.
  bool tracking_click_;
  CPoint mouse_down_point_;

  // We need to know if the user triple-clicks, so track double click points
  // and times so we can see if subsequent clicks are actually triple clicks.
  bool tracking_double_click_;
  CPoint double_click_point_;
  DWORD double_click_time_;

  // Used to discard unnecessary WM_MOUSEMOVE events after the first such
  // unnecessary event.  See detailed comments in OnMouseMove().
  bool can_discard_mousemove_;

  // Variables for tracking state before and after a possible change.
  std::wstring text_before_change_;
  CHARRANGE sel_before_change_;

  // Set at the same time the model's original_* members are set, and valid in
  // the same cases.
  CHARRANGE original_selection_;

  // Holds the user's selection across focus changes.  cpMin holds -1 when
  // there is no saved selection.
  CHARRANGE saved_selection_for_focus_change_;

  // The context menu for the edit.
  scoped_ptr<Menu> context_menu_;

  // Font we're using.  We keep a reference to make sure the font supplied to
  // the constructor doesn't go away before we do.
  ChromeFont font_;

  // Metrics about the font, which we keep so we don't need to recalculate them
  // every time we paint.  |font_y_adjustment_| is the number of pixels we need
  // to shift the font vertically in order to make its baseline be at our
  // desired baseline in the edit.
  int font_ascent_;
  int font_descent_;
  int font_x_height_;
  int font_y_adjustment_;

  // If true, indicates the mouse is down and if the mouse is moved enough we
  // should start a drag.
  bool possible_drag_;

  // If true, we're in a call to DoDragDrop.
  bool in_drag_;

  // If true indicates we've run a drag and drop session. This is used to
  // avoid starting two drag and drop sessions if the drag is canceled while
  // the mouse is still down.
  bool initiated_drag_;

  // Position of the drop highlight.  If this is -1, there is no drop highlight.
  int drop_highlight_position_;

  // Security UI-related data.
  COLORREF background_color_;
  ToolbarModel::SecurityLevel scheme_security_level_;

  // This interface is useful for accessing the CRichEditCtrl at a low level.
  mutable CComQIPtr<ITextDocument> text_object_model_;

  // This contains the scheme char start and stop indexes that should be
  // striken-out when displaying an insecure scheme.
  url_parse::Component insecure_scheme_component_;

  // Instance of accessibility information and handling.
  mutable CComPtr<IAccessible> autocomplete_accessibility_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteEditViewWin);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_WIN_H_
