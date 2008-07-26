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

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_H__
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_H__

#include <atlbase.h>
#include <atlapp.h>
#include <atlcomcli.h>
#include <atlctrls.h>
#include <oleacc.h>
#include <tom.h> // For ITextDocument, a COM interface to CRichEditCtrl

#include "base/iat_patch.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_popup.h"
#include "chrome/browser/security_style.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/views/menu.h"

class CommandController;
class Profile;
class TabContents;

// Provides the implementation of an edit control with a drop-down
// autocomplete box. The box itself is implemented in autocomplete_popup.cc
// This file implements the edit box and management for the popup.
//
// This implementation is currently appropriate for the URL bar, where the
// autocomplete dropdown is always displayed because there is always a
// default item. For web page autofill and other applications, this is
// probably not appropriate. We may want to add a flag to determine which
// of these modes we're in.
class AutocompleteEdit
    : public CWindowImpl<AutocompleteEdit,
                         CRichEditCtrl,
                         CWinTraits<WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL |
                                    ES_NOHIDESEL> >,
      public CRichEditCommands<AutocompleteEdit>,
      public Menu::Delegate {
 public:
  DECLARE_WND_CLASS(L"Chrome_AutocompleteEdit");

  // Embedders of this control must implement this class.
  class Controller {
   public:
    // When the user presses enter or selects a line with the mouse, this
    // function will get called synchronously with the url to open and
    // disposition and transition to use when opening it.
    //
    // |alternate_nav_url|, if non-empty, contains the alternate navigation URL
    // for |url|, which the controller can check for existence.  See comments on
    // AutocompleteResult::GetAlternateNavURL().
    virtual void OnAutocompleteAccept(const std::wstring& url,
        WindowOpenDisposition disposition,
        PageTransition::Type transition,
        const std::wstring& alternate_nav_url) = 0;

    // Called when anything has changed that might affect the layout or contents
    // of the views around the edit, including the text of the edit and the
    // status of any keyword- or hint-related state.
    virtual void OnChanged() = 0;

    // Called whenever the user starts or stops an input session (typing,
    // interacting with the edit, etc.).  When user input is not in progress,
    // the edit is guaranteed to be showing the permanent text.
    virtual void OnInputInProgress(bool in_progress) = 0;

    // Returns the favicon of the current page.
    virtual SkBitmap GetFavIcon() const = 0;

    // Returns the title of the current page.
    virtual std::wstring GetTitle() const = 0;
  };

  // The State struct contains enough information about the AutocompleteEdit to
  // save/restore a user's typing, caret position, etc. across tab changes.  We
  // explicitly don't preserve things like whether the popup was open as this
  // might be weird.
  struct State {
    State(const CHARRANGE& selection,
          const CHARRANGE& saved_selection_for_focus_change,
          bool user_input_in_progress,
          const std::wstring& user_text,
          const AutocompleteResult::Selection& manually_selected_match,
          const std::wstring& keyword,
          bool is_keyword_hint,
          bool disable_keyword_ui,
          bool show_search_hint)
        : selection(selection),
          saved_selection_for_focus_change(saved_selection_for_focus_change),
          user_input_in_progress(user_input_in_progress),
          user_text(user_text),
          manually_selected_match(manually_selected_match),
          keyword(keyword),
          is_keyword_hint(is_keyword_hint),
          disable_keyword_ui(disable_keyword_ui),
          show_search_hint(show_search_hint) {
    }

    const CHARRANGE selection;
    const CHARRANGE saved_selection_for_focus_change;
    bool user_input_in_progress;
    const std::wstring user_text;
    AutocompleteResult::Selection manually_selected_match;
    const std::wstring keyword;
    const bool is_keyword_hint;
    const bool disable_keyword_ui;
    const bool show_search_hint;
  };

  // The given observer is notified when the user selects an item
  AutocompleteEdit(const ChromeFont& font,
                   Controller* controller,
                   ToolbarModel* model,
                   ChromeViews::View* parent_view,
                   HWND hwnd,
                   Profile* profile,
                   CommandController* command_controller,
                   bool popup_window_mode);
  ~AutocompleteEdit();

  // Called when any LocationBarView state changes. If
  // |tab_for_state_restoring| is non-NULL, it points to a TabContents whose
  // state we should restore.
  void Update(const TabContents* tab_for_state_restoring);

  // Invoked when the profile has changed.
  void SetProfile(Profile* profile);

  // For use when switching tabs, this saves the current state onto the tab so
  // that it can be restored during a later call to Update().
  void SaveStateToTab(TabContents* tab);

  // Returns the current text of the edit control, which could be the
  // "temporary" text set by the popup, the "permanent" text set by the
  // browser, or just whatever the user has currently typed.
  std::wstring GetText() const;

  // Returns the URL. If the user has not edited the text, this returns the
  // permanent text. If the user has edited the text, this returns the default
  // match based on the current text, which may be a search URL, or keyword
  // generated URL.
  //
  // See AutocompleteEdit for a description of the args (they may be null if
  // not needed).
  std::wstring GetURLForCurrentText(PageTransition::Type* transition,
                                    bool* is_history_what_you_typed_match,
                                    std::wstring* alternate_nav_url);

  // The user text is the text the user has manually keyed in.  When present,
  // this is shown in preference to the permanent text; hitting escape will
  // revert to the permanent text.
  void SetUserText(const std::wstring& text) { SetUserText(text, text, true); }

  // Selects all the text in the edit.  Use this in place of SetSelAll() to
  // avoid selecting the "phantom newline" at the end of the edit.
  void SelectAll(bool reversed);

  // Reverts the edit and popup back to their unedited state (permanent text
  // showing, popup closed, no user input in progress).
  void RevertAll();

  // Asks the browser to load the popup's currently selected item, using the
  // supplied disposition.  This may close the popup. If |for_drop| is true,
  // it indicates the input is being accepted as part of a drop operation and
  // the transition should be treated as LINK (so that it won't trigger the
  // URL to be autocompleted).
  void AcceptInput(WindowOpenDisposition disposition,
                   bool for_drop);

  // Asks the browser to load the specified URL, which is assumed to be one of
  // the popup entries, using the supplied disposition and transition type.
  // |alternate_nav_url|, if non-empty, contains the alternate navigation URL
  // for |url|.  See comments on AutocompleteResult::GetAlternateNavURL().
  //
  // |selected_line| is passed to AutocompletePopup::LogOpenedURL(); see
  // comments there.
  //
  // If the URL was expanded from a keyword, |keyword| is that keyword.
  //
  // This may close the popup.
  void OpenURL(const std::wstring& url,
               WindowOpenDisposition disposition,
               PageTransition::Type transition,
               const std::wstring& alternate_nav_url,
               size_t selected_line,
               const std::wstring& keyword);

  // Closes the autocomplete popup, if it's open.
  void ClosePopup();

  // Accessors for keyword-related state (see comments on keyword_ and
  // is_keyword_hint_).
  std::wstring keyword() const {
    return ((is_keyword_hint_ && has_focus_) ||
            (!is_keyword_hint_ && !disable_keyword_ui_)) ?
        keyword_ : std::wstring();
  }
  bool is_keyword_hint() const { return is_keyword_hint_; }

  // True if we should show the "Type to search" hint (see comments on
  // show_search_hint_).
  bool show_search_hint() const { return has_focus_ && show_search_hint_; }

  ChromeViews::View* parent_view() const { return parent_view_; }

  // Returns true if a query to an autocomplete provider is currently
  // in progress.  This logic should in the future live in
  // AutocompleteController but resides here for now.  This method is used by
  // AutomationProvider::AutocompleteEditIsQueryInProgress.
  bool query_in_progress() const { return popup_->query_in_progress(); }

  // Returns the lastest autocomplete results.  This logic should in the future
  // live in AutocompleteController but resides here for now.  This method is
  // used by AutomationProvider::AutocompleteEditGetMatches.
  const AutocompleteResult* latest_result() const {
    return popup_->latest_result();
  }

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

  // Called before an accelerator is processed to give us a chance to override
  // it.
  bool OverrideAccelerator(const ChromeViews::Accelerator& accelerator);

  // Handler for external events passed in to us.  The View that owns us may
  // send us events that we should treat as if they were events on us.
  void HandleExternalMsg(UINT msg, UINT flags, const CPoint& screen_point);

  // Called back by the AutocompletePopup when any relevant data changes.  This
  // rolls together several separate pieces of data into one call so we can
  // update all the UI efficiently:
  //   |text| is either the new temporary text (if |is_temporary_text| is true)
  //     from the user manually selecting a different match, or the inline
  //     autocomplete text (if |is_temporary_text| is false).
  //   |previous_selected_match| is only used when changing the temporary text;
  //     it is the match that was (manually or automatically) selected before
  //     the current manual selection, and is saved to be restored later if the
  //     user hits <esc>.
  //   |can_show_search_hint| is true if the current choice is nonexistent or a
  //     search result; in these cases it may be OK to show the "Type to search"
  //     hint (see comments on show_search_hint_).
  //   |keyword| is the keyword to show a hint for if |is_keyword_hint| is true,
  //     or the currently selected keyword if |is_keyword_hint| is false (see
  //     comments on keyword_ and is_keyword_hint_).
  void OnPopupDataChanged(
      const std::wstring& text,
      bool is_temporary_text,
      const AutocompleteResult::Selection& previous_selected_match,
      const std::wstring& keyword,
      bool is_keyword_hint,
      bool can_show_search_hint);

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
    ScopedFreeze(AutocompleteEdit* edit, ITextDocument* text_object_model);
    ~ScopedFreeze();

   private:
    AutocompleteEdit* const edit_;
    ITextDocument* const text_object_model_;

    DISALLOW_EVIL_CONSTRUCTORS(ScopedFreeze);
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

    DISALLOW_EVIL_CONSTRUCTORS(ScopedSuspendUndo);
  };

  enum ControlKeyState {
    UP,                   // The control key is not depressed.
    DOWN_WITHOUT_CHANGE,  // The control key is depressed, and the edit's
                          // contents/selection have not changed since it was
                          // depressed.  This is the only state in which we
                          // do the "ctrl-enter" behavior when the user hits
                          // enter.
    DOWN_WITH_CHANGE,     // The control key is depressed, and the edit's
                          // contents/selection have changed since it was
                          // depressed.  If the user now hits enter, we assume
                          // he simply hasn't released the key, rather than that
                          // he intended to hit "ctrl-enter".
  };

  enum PasteState {
    NONE,           // Most recent edit was not a paste that replaced all text.
    REPLACED_ALL,   // Most recent edit was a paste that replaced all text.
    REPLACING_ALL,  // In the middle of doing a paste that replaces all
                    // text.  We need this intermediate state because OnPaste()
                    // does the actual detection of such pastes, but
                    // OnAfterPossibleChange() has to update the paste state
                    // for every edit.  If OnPaste() set the state directly to
                    // REPLACED_ALL, OnAfterPossibleChange() wouldn't know
                    // whether that represented the current edit or a past one.
  };

  // Replacement word-breaking proc for the rich edit control.
  static int CALLBACK WordBreakProc(LPTSTR edit_text,
                                    int current_pos,
                                    int num_bytes,
                                    int action);

  // Returns true if |edit_text| starting at |current_pos| is "://".
  static bool SchemeEnd(LPTSTR edit_text, int current_pos, int length);

  // Intercepts.  See OnPaint().
  static HDC WINAPI BeginPaintIntercept(HWND hWnd, LPPAINTSTRUCT lpPaint);
  static BOOL WINAPI EndPaintIntercept(HWND hWnd, CONST PAINTSTRUCT* lpPaint);

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

  // Helper function for OnChar() and OnKeyDown() that handles keystrokes that
  // could change the text in the edit.
  void HandleKeystroke(UINT message, TCHAR key, UINT repeat_count, UINT flags);

  // Helper functions for OnKeyDown() that handle accelerators applicable when
  // we're not read-only and all the time, respectively.  These return true if
  // they handled the key.
  bool OnKeyDownOnlyWritable(TCHAR key, UINT repeat_count, UINT flags);
  bool OnKeyDownAllModes(TCHAR key, UINT repeat_count, UINT flags);

  // Every piece of code that can change the edit should call these functions
  // before and after the change.  These functions determine if anything
  // meaningful changed, and do any necessary updating and notification.
  void OnBeforePossibleChange();
  // OnAfterPossibleChange() returns true if there was a change that caused it
  // to call UpdatePopup().
  bool OnAfterPossibleChange();

  // Implementation for SetUserText(wstring). SetUserText(wstring) invokes this
  // with |update_popup| set to false.
  void SetUserText(const std::wstring& text, const std::wstring& display_text,
                   bool update_popup);

  // The inline autocomplete text is shown, selected, appended to the user's
  // current input.  For example, if the user types in "go", and the
  // autocomplete system determines that the best match is "google", this
  // routine might be called with |text| = "ogle".
  // Returns true if the text in the edit changed.
  bool SetInlineAutocompleteText(const std::wstring& text);

  // The temporary text is set when the user arrows around the autocomplete
  // popup.  When present, this is shown in preference to the user text;
  // hitting escape will revert to the user text.
  // |previous_selected_match| is used to update the member variable
  // |original_selected_match_| if there wasn't already any temporary text.
  // Returns true if the text in the edit changed.
  bool SetTemporaryText(
      const std::wstring& text,
      const AutocompleteResult::Selection& previous_selected_match);

  // Called whenever user_text_ should change.
  void InternalSetUserText(const std::wstring& text);

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
  void PlaceCaretAt(std::wstring::size_type pos);

  // Returns true if |sel| represents a forward or backward selection of all the
  // text.
  bool IsSelectAll(const CHARRANGE& sel) const;

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

  // When the user is pressing the control key, we interpret this as requesting
  // us to add the top-level domain "com" to the contents of the edit control.
  // Some users expect this behavior because it is present in other browsers.
  std::wstring GetDesiredTLD() const;

  // Updates the autocomplete popup and other state after the text has been
  // changed by the user.
  void UpdatePopup();

  // Internally invoked whenever the text changes in some way.
  void TextChanged();

  // Conversion between user text and display text. User text is the text the
  // user has input. Display text is the text being shown in the edit. The
  // two are different if a keyword is selected.
  std::wstring DisplayTextFromUserText(const std::wstring& text) const;
  std::wstring UserTextFromDisplayText(const std::wstring& text) const;

  // Returns the current clipboard contents as a string that can be pasted in.
  // In addition to just getting CF_UNICODETEXT out, this can also extract URLs
  // from bookmarks on the clipboard.
  std::wstring AutocompleteEdit::GetClipboardText() const;

  // Determines whether the user can "paste and go", given the specified text.
  // This also updates the internal paste-and-go-related state variables as
  // appropriate so that the controller doesn't need to be repeatedly queried
  // for the same text in every clipboard-related function.
  bool CanPasteAndGo(const std::wstring& text) const;

  // Navigates to the destination last supplied to CanPasteAndGo.
  void PasteAndGo();

  // Sets the state of user_input_in_progress_, and notifies the observer if
  // that state has changed.
  void SetInputInProgress(bool in_progress);

  // As necessary, sends out notification that the user is accepting a URL in
  // the edit.  If the accepted URL is from selecting a keyword, |keyword| is
  // the selected keyword.
  // If |selected_line| is kNoMatch, the currently selected line is used for the
  // metrics log record; otherwise, the provided value is used as the selected
  // line.  This is used when the user opens a URL without actually selecting
  // its entry, such as middle-clicking it.
  void SendOpenNotification(size_t selected_line, const std::wstring& keyword);

  // Getter for the text_object_model_, used by the ScopedXXX classes.  Note
  // that the pointer returned here is only valid as long as the
  // AutocompleteEdit is still alive.
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

  Controller* controller_;

  // The Popup itself.
  scoped_ptr<AutocompletePopup> popup_;

  // When true, the location bar view is read only and also is has a slightly
  // different presentation (font size / color). This is used for popups.
  bool popup_window_mode_;

  // Whether the edit has focus.
  bool has_focus_;

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

  // The URL of the currently displayed page.
  std::wstring permanent_text_;

  // This flag is true when the user has modified the contents of the edit, but
  // not yet accepted them.  We use this to determine when we need to save
  // state (on switching tabs) and whether changes to the page URL should be
  // immediately displayed.
  // This flag will be true in a superset of the cases where the popup is open.
  bool user_input_in_progress_;

  // The text that the user has entered.  This does not include inline
  // autocomplete text that has not yet been accepted.
  std::wstring user_text_;

  // When the user closes the popup, we need to remember the URL for their
  // desired choice, so that if they hit enter without reopening the popup we
  // know where to go.  We could simply rerun autocomplete in this case, but
  // we'd need to either wait for all results to come in (unacceptably slow) or
  // do the wrong thing when the user had chosen some provider whose results
  // were not returned instantaneously.
  //
  // This variable is only valid when user_input_in_progress_ is true, since
  // when it is false the user has either never input anything (so there won't
  // be a value here anyway) or has canceled their input, which should be
  // treated the same way.  Also, since this is for preserving a desired URL
  // after the popup has been closed, we ignore this if the popup is open, and
  // simply ask the popup for the desired URL directly.  As a result, the
  // contents of this variable only need to be updated when the popup is closed
  // but user_input_in_progress_ is not being cleared.
  std::wstring url_for_remembered_user_selection_;

  // Inline autocomplete is allowed if the user has not just deleted text, and
  // no temporary text is showing.  In this case, inline_autocomplete_text_ is
  // appended to the user_text_ and displayed selected (at least initially).
  //
  // NOTE: When the popup is closed there should never be inline autocomplete
  // text (actions that close the popup should either accept the text, convert
  // it to a normal selection, or change the edit entirely).
  bool just_deleted_text_;
  std::wstring inline_autocomplete_text_;

  // Used by Set/RevertTemporaryText to keep track of whether there
  // is currently a temporary text.  The original_selection_ is only valid when
  // there is temporary text.
  //
  // Example of use: If the user types "goog", then arrows down in the
  // autocomplete popup until, say, "google.com" appears in the edit box, then
  // the user_text_ is still "goog", and "google.com" is "temporary text".
  // When the user hits <esc>, the edit box reverts to "goog".  Hit <esc> again
  // and the popup is closed and "goog" is replaced by the permanent_text_,
  // which is the URL of the current page.
  //
  // original_url_ is valid in the same cases as original_selection_, and is
  // used as the unique identifier of the originally selected item.  Thus, if
  // the user arrows to a different item with the same text, we can still
  // distinguish them and not revert all the way to the permanent_text_.
  //
  // original_selected_match_, which is valid in the same cases as the other
  // two original_* members, is the manually selected match to revert the popup
  // to, if any.  This can be non-empty when the user has selected a keyword
  // (by hitting <tab> when applicable), or when the user has manually selected
  // a match and then continued to edit it.
  bool has_temporary_text_;
  CHARRANGE original_selection_;
  std::wstring original_url_;
  AutocompleteResult::Selection original_selected_match_;

  // When the user's last action was to paste and replace all the text, we
  // disallow inline autocomplete (on the theory that the user is trying to
  // paste in a new URL or part of one, and in either case inline autocomplete
  // would get in the way).
  PasteState paste_state_;

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
  bool select_all_before_change_;

  // Holds the user's selection across focus changes.  cpMin holds -1 when
  // there is no saved selection.
  CHARRANGE saved_selection_for_focus_change_;

  // The context menu for the edit.
  scoped_ptr<Menu> context_menu_;

  // Whether the control key is depressed.  We track this to avoid calling
  // UpdatePopup() repeatedly if the user holds down the key, and to know
  // whether to trigger "ctrl-enter" behavior.
  ControlKeyState control_key_state_;

  // The object that handles additional command functionality exposed on the
  // edit, such as invoking the keyword editor.
  CommandController* command_controller_;

  // The parent view for the edit, used to align the popup and for
  // accessibility.
  ChromeViews::View* parent_view_;

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

  // The keyword associated with the current match.  The user may have an actual
  // selected keyword, or just some input text that looks like a keyword (so we
  // can show a hint to press <tab>).  This is the keyword in either case;
  // is_keyword_hint_ (below) distinguishes the two cases.
  std::wstring keyword_;

  // True if the keyword associated with this match is merely a hint, i.e. the
  // user hasn't actually selected a keyword yet.  When this is true, we can use
  // keyword_ to show a "Press <tab> to search" sort of hint.
  bool is_keyword_hint_;

  // In some cases, such as when the user is editing in the middle of the input
  // string, the input might look like a keyword, but we don't want to display
  // the keyword UI, so as not to interfere with the user's editing.
  bool disable_keyword_ui_;

  // True when it's safe to show a "Type to search" hint to the user (when the
  // edit is empty, or the user is in the process of searching).
  bool show_search_hint_;

  // Security UI-related data.
  COLORREF background_color_;
  ToolbarModel::SecurityLevel scheme_security_level_;

  // Paste And Go-related state.  See CanPasteAndGo().
  mutable std::wstring paste_and_go_url_;
  mutable PageTransition::Type paste_and_go_transition_;
  mutable std::wstring paste_and_go_alternate_nav_url_;

  // This interface is useful for accessing the CRichEditCtrl at a low level.
  mutable CComQIPtr<ITextDocument> text_object_model_;

  ToolbarModel* model_;

  Profile* profile_;

  // This contains the scheme char start and stop indexes that should be
  // striken-out when displaying an insecure scheme.
  url_parse::Component insecure_scheme_component_;

  // Instance of accessibility information and handling.
  mutable CComPtr<IAccessible> autocomplete_accessibility_;

  DISALLOW_EVIL_CONSTRUCTORS(AutocompleteEdit);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_H__
