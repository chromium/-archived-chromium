// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlmisc.h>

#include "base/task.h"
#include "base/timer.h"
#include "base/win_util.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/views/view.h"

class AutocompleteEditModel;
class AutocompleteEditView;
class Profile;
class MirroringContext;
class SkBitmap;

class AutocompletePopupModel;
class AutocompletePopupView;

// TODO(pkasting): http://b/1343512  The names and contents of the classes in
// this file are temporary.  I am in hack-and-slash mode right now.

#define AUTOCOMPLETEPOPUPVIEW_CLASSNAME L"Chrome_AutocompletePopupView"

// This class implements a popup window used to display autocomplete results.
class AutocompletePopupView
    : public CWindowImpl<AutocompletePopupView, CWindow, CControlWinTraits> {
 public:
  DECLARE_WND_CLASS_EX(AUTOCOMPLETEPOPUPVIEW_CLASSNAME,
                       ((win_util::GetWinVersion() < win_util::WINVERSION_XP) ?
                           0 : CS_DROPSHADOW), COLOR_WINDOW)

  BEGIN_MSG_MAP(AutocompletePopupView)
    MSG_WM_ERASEBKGND(OnEraseBkgnd)
    MSG_WM_LBUTTONDOWN(OnLButtonDown)
    MSG_WM_MBUTTONDOWN(OnMButtonDown)
    MSG_WM_LBUTTONUP(OnLButtonUp)
    MSG_WM_MBUTTONUP(OnMButtonUp)
    MSG_WM_MOUSEACTIVATE(OnMouseActivate)
    MSG_WM_MOUSELEAVE(OnMouseLeave)
    MSG_WM_MOUSEMOVE(OnMouseMove)
    MSG_WM_PAINT(OnPaint)
  END_MSG_MAP()

  AutocompletePopupView(AutocompletePopupModel* model,
                        const ChromeFont& font,
                        AutocompleteEditView* edit_view);

  // Returns true if the popup is currently open.
  bool is_open() const { return m_hWnd != NULL; }

  // Invalidates one line of the autocomplete popup.
  void InvalidateLine(size_t line);

  // Redraws the popup window to match any changes in result_; this may mean
  // opening or closing the window.
  void UpdatePopupAppearance();

  // Called by the model when hover is enabled or disabled.
  void OnHoverEnabledOrDisabled(bool disabled);

 private:
  // Caches GDI objects and information for drawing.
  struct DrawLineInfo {
    enum LineStatus {
      NORMAL = 0,
      HOVERED,
      SELECTED,
      MAX_STATUS_ENTRIES
    };

    explicit DrawLineInfo(const ChromeFont& font);
    ~DrawLineInfo();

    static COLORREF AlphaBlend(COLORREF foreground,
                               COLORREF background,
                               BYTE alpha);

    static const wchar_t ellipsis_str[];

    ChromeFont regular_font;  // Fonts used for rendering AutocompleteMatches.
    ChromeFont bold_font;
    int font_height;          // Height (in pixels) of a line of text w/o
                              // padding.
    int line_height;          // Height (in pixels) of a line of text w/padding.
    int ave_char_width;       // Width (in pixels) of an average character of
                              // the regular font.
    int ellipsis_width;       // Width (in pixels) of the ellipsis_str.

    // colors
    COLORREF background_colors[MAX_STATUS_ENTRIES];
    COLORREF text_colors[MAX_STATUS_ENTRIES];
    COLORREF url_colors[MAX_STATUS_ENTRIES];

    // brushes
    HBRUSH brushes[MAX_STATUS_ENTRIES];

   private:
    static double LuminosityContrast(COLORREF color1, COLORREF color2);
    static double Luminosity(COLORREF color);
  };

  // message handlers
  LRESULT OnEraseBkgnd(HDC hdc) {
    // We do all needed erasing ourselves in OnPaint, so the only thing that
    // WM_ERASEBKGND will do is cause flicker.  Disable it by just returning
    // nonzero here ("erase completed") without doing anything.
    return 1;
  }
  void OnLButtonDown(UINT keys, const CPoint& point);
  void OnMButtonDown(UINT keys, const CPoint& point);
  void OnLButtonUp(UINT keys, const CPoint& point);
  void OnMButtonUp(UINT keys, const CPoint& point);
  LRESULT OnMouseActivate(HWND window, UINT hit_test, UINT mouse_message);
  void OnMouseLeave();
  void OnMouseMove(UINT keys, const CPoint& point);
  void OnPaint(HDC hdc);

  // Called by On*ButtonUp() to do the actual work of handling a button
  // release.  Opens the item at the given coordinate, using the supplied
  // disposition.
  void OnButtonUp(const CPoint& point, WindowOpenDisposition disposition);

  // Gives the topmost y coordinate within |line|, which should be within the
  // range of valid lines.
  int LineTopPixel(size_t line) const;

  // Converts the given y-coordinate to a line.  Due to drawing slop (window
  // borders, etc.), |y| might be within the window but outside the range of
  // pixels which correspond to lines; in this case the result will be clamped,
  // i.e., the top and bottom lines will be treated as extending to the top and
  // bottom edges of the window, respectively.
  size_t PixelToLine(int y) const;

  // Draws a light border around the inside of the window with the given client
  // rectangle and DC.
  void DrawBorder(const RECT& rc, HDC dc);

  // Draws a single run of text with a particular style.  Handles both LTR and
  // RTL text as well as eliding.  Returns the width, in pixels, of the string
  // as it was actually displayed.
  int DrawString(HDC dc,
                 int x,
                 int y,
                 int max_x,
                 const wchar_t* text,
                 int length,
                 int style,
                 const DrawLineInfo::LineStatus status,
                 const MirroringContext* context,
                 bool text_direction_is_rtl) const;

  // Draws a string from the autocomplete controller which can have specially
  // marked "match" portions.
  void DrawMatchFragments(HDC dc,
                          const std::wstring& text,
                          const ACMatchClassifications& classifications,
                          int x,
                          int y,
                          int max_x,
                          DrawLineInfo::LineStatus status) const;

  // Draws one line of the text in the box.
  void DrawEntry(HDC dc,
                 const RECT& client_rect,
                 size_t line,
                 DrawLineInfo::LineStatus status,
                 bool all_descriptions_empty,
                 bool starred) const;

  // Draws the star at the specified location
  void DrawStar(HDC dc, int x, int y) const;

  AutocompletePopupModel* model_;

  AutocompleteEditView* edit_view_;

  // Cached GDI information for drawing.
  DrawLineInfo line_info_;

  // Bitmap for the star.  This is owned by the ResourceBundle.
  SkBitmap* star_;

  // A context used for mirroring regions.
  scoped_ptr<MirroringContext> mirroring_context_;

  // When hovered_line_ is kNoMatch, this holds the screen coordinates of the
  // mouse position when hover tracking was turned off.  If the mouse moves to a
  // point over the popup that has different coordinates, hover tracking will be
  // re-enabled.  When hovered_line_ is a valid line, the value here is
  // out-of-date and should be ignored.
  CPoint last_hover_coordinates_;

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupView);
};

class AutocompletePopupModel : public ACControllerListener, public Task {
 public:
  AutocompletePopupModel(const ChromeFont& font,
                         AutocompleteEditView* edit_view,
                         AutocompleteEditModel* edit_model,
                         Profile* profile);
  ~AutocompletePopupModel();

  // Invoked when the profile has changed.
  void SetProfile(Profile* profile);

  // Gets autocomplete results for the given text. If there are results, opens
  // the popup if necessary and fills it with the new data. Otherwise, closes
  // the popup if necessary.
  //
  // |prevent_inline_autocomplete| is true if the generated result set should
  // not require inline autocomplete for the default match.  This is difficult
  // to explain in the abstract; the practical use case is that after the user
  // deletes text in the edit, the HistoryURLProvider should make sure not to
  // promote a match requiring inline autocomplete too highly.
  //
  // |prefer_keyword| should be true when the keyword UI is onscreen; this will
  // bias the autocomplete results toward the keyword provider when the input
  // string is a bare keyword.
  void StartAutocomplete(const std::wstring& text,
                         const std::wstring& desired_tld,
                         bool prevent_inline_autocomplete,
                         bool prefer_keyword);

  // Closes the window and cancels any pending asynchronous queries.
  void StopAutocomplete();

  // Returns true if the popup is currently open.
  bool is_open() const { return view_->is_open(); }

  // Returns the AutocompleteController used by this popup.
  AutocompleteController* autocomplete_controller() const {
    return controller_.get();
  }

  // Returns true if no autocomplete query is currently running.
  bool query_in_progress() const { return query_in_progress_; }

  const AutocompleteResult* result() const {
    return &result_;
  }

  const AutocompleteResult* latest_result() const {
    return &latest_result_;
  }

  size_t hovered_line() const {
    return hovered_line_;
  }

  // Call to change the hovered line.  |line| should be within the range of
  // valid lines (to enable hover) or kNoMatch (to disable hover).
  void SetHoveredLine(size_t line);

  size_t selected_line() const {
    return selected_line_;
  }

  // Call to change the selected line.  This will update all state and repaint
  // the necessary parts of the window, as well as updating the edit with the
  // new temporary text.  |line| should be within the range of valid lines.
  // |reset_to_default| is true when the selection is being reset back to the
  // default match, and thus there is no temporary text (and no
  // |manually_selected_match_|).
  // NOTE: This assumes the popup is open, and thus both old and new values for
  // the selected line should not be kNoMatch.
  void SetSelectedLine(size_t line, bool reset_to_default);

  // Called when the user hits escape after arrowing around the popup.  This
  // will change the selected line back to the default match and redraw.
  void ResetToDefaultMatch() {
    SetSelectedLine(result_.default_match() - result_.begin(), true);
  }

  // Returns the URL for the selected match.  If an update is in progress,
  // "selected" means "default in the latest results".  If there are no
  // results, returns the empty string.
  //
  // If |transition_type| is non-NULL, it will be set to the appropriate
  // transition type for the selected entry (TYPED or GENERATED).
  //
  // If |is_history_what_you_typed_match| is non-NULL, it will be set based on
  // the selected entry's is_history_what_you_typed value.
  //
  // If |alternate_nav_url| is non-NULL, it will be set to the alternate
  // navigation URL for |url| if one exists, or left unchanged otherwise.  See
  // comments on AutocompleteResult::GetAlternateNavURL().
  std::wstring URLsForCurrentSelection(
      PageTransition::Type* transition,
      bool* is_history_what_you_typed_match,
      std::wstring* alternate_nav_url) const;

  // This is sort of a hybrid between StartAutocomplete() and
  // URLForCurrentSelection().  When the popup isn't open and the user hits
  // enter, we want to get the default result for the user's input immediately,
  // and not open the popup, continue running autocomplete, etc.  Therefore,
  // this does a query for only the synchronously available results for the
  // provided input parameters, sets |transition|,
  // |is_history_what_you_typed_match|, and |alternate_nav_url| (if applicable)
  // based on the default match, and returns its url. |transition|,
  // |is_history_what_you_typed_match| and/or |alternate_nav_url| may be null,
  // in which case they are not updated.
  //
  // If there are no matches for |text|, leaves the outparams unset and returns
  // the empty string.
  std::wstring URLsForDefaultMatch(const std::wstring& text,
                                   const std::wstring& desired_tld,
                                   PageTransition::Type* transition,
                                   bool* is_history_what_you_typed_match,
                                   std::wstring* alternate_nav_url);

  // Gets the selected keyword or keyword hint for the given match.  Returns
  // true if |keyword| represents a keyword hint, or false if |keyword|
  // represents a selected keyword.  (|keyword| will always be set [though
  // possibly to the empty string], and you cannot have both a selected keyword
  // and a keyword hint simultaneously.)
  bool GetKeywordForMatch(const AutocompleteMatch& match,
                          std::wstring* keyword);

  // Returns a pointer to a heap-allocated AutocompleteLog containing the
  // current input text, selected match, and result set.  The caller is
  // responsible for deleting the object.
  AutocompleteLog* GetAutocompleteLog();

  // Immediately updates and opens the popup if necessary, then moves the
  // current selection down (|count| > 0) or up (|count| < 0), clamping to the
  // first or last result if necessary.  If |count| == 0, the selection will be
  // unchanged, but the popup will still redraw and modify the text in the
  // AutocompleteEditModel.
  void Move(int count);

  // Called when the user hits shift-delete.  This should determine if the item
  // can be removed from history, and if so, remove it and update the popup.
  void TryDeletingCurrentItem();

  // ACControllerListener - called when more autocomplete data is available or
  // when the query is complete.
  //
  // When the input for the current query has a provider affinity, we try to
  // keep the current result set's default match as the new default match.
  virtual void OnAutocompleteUpdate(bool updated_result, bool query_complete);

  // Task - called when either timer fires.  Calls CommitLatestResults().
  virtual void Run();

  // The token value for selected_line_, hover_line_ and functions dealing with
  // a "line number" that indicates "no line".
  static const size_t kNoMatch = -1;

 private:
   // Stops an existing query but doesn't close the popup.
   void StopQuery();

  // Sets the correct default match in latest_result_, then updates the popup
  // appearance to match.  If |immediately| is true this update happens
  // synchronously; otherwise, it's deferred until the next scheduled update.
  void SetDefaultMatchAndUpdate(bool immediately);

  // If an update is pending or |force| is true, immediately updates result_ to
  // match latest_result_, and calls UpdatePopupAppearance() to reflect those
  // changes back to the user.
  void CommitLatestResults(bool force);

  scoped_ptr<AutocompletePopupView> view_;

  AutocompleteEditModel* edit_model_;
  scoped_ptr<AutocompleteController> controller_;

  // Profile for current tab.
  Profile* profile_;

  // The input for the current query.
  AutocompleteInput input_;

  // Data from the autocomplete query.
  AutocompleteResult result_;

  // True if an autocomplete query is currently running.
  bool query_in_progress_;

  // The latest result available from the autocomplete service.  This may be
  // different than result_ if we've gotten results from our providers that we
  // haven't yet shown the user.  If more results may be coming, we'll wait to
  // display these in hopes of minimizing flicker; see coalesce_timer_.
  AutocompleteResult latest_result_;

  // True when there are newer results in latest_result_ than in result_ and
  // the popup has not been updated to show them.
  bool update_pending_;

  // Timer that tracks how long it's been since the last provider update we
  // received.  Instead of displaying each update immediately, we batch updates
  // into groups, which reduces flicker.
  base::OneShotTimer<AutocompletePopupModel> coalesce_timer_;

  // Timer that tracks how long it's been since the last time we updated the
  // onscreen results.  This is used to ensure that the popup is somewhat
  // responsive even when the user types continuously.
  base::RepeatingTimer<AutocompletePopupModel> max_delay_timer_;

  // The line that's currently hovered.  If we're not drawing a hover rect,
  // this will be kNoMatch, even if the cursor is over the popup contents.
  size_t hovered_line_;

  // The currently selected line.  This is kNoMatch when nothing is selected,
  // which should only be true when the popup is closed.
  size_t selected_line_;

  // The match the user has manually chosen, if any.
  AutocompleteResult::Selection manually_selected_match_;

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupModel);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_H_

