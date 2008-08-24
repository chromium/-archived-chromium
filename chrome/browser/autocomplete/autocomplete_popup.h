// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_H__
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_H__

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

#define AUTOCOMPLETEPOPUP_CLASSNAME L"Chrome_AutocompletePopup"

class AutocompleteEdit;
class Profile;
class SkBitmap;
class MirroringContext;

// This class implements a popup window used by AutocompleteEdit to display
// autocomplete results.
class AutocompletePopup
    : public CWindowImpl<AutocompletePopup, CWindow, CControlWinTraits>,
      public ACControllerListener,
      public Task {
 public:
  DECLARE_WND_CLASS_EX(AUTOCOMPLETEPOPUP_CLASSNAME,
                       ((win_util::GetWinVersion() < win_util::WINVERSION_XP) ?
                           0 : CS_DROPSHADOW), COLOR_WINDOW)

  BEGIN_MSG_MAP(AutocompletePopup)
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

  AutocompletePopup(const ChromeFont& font,
                    AutocompleteEdit* editor,
                    Profile* profile);
  ~AutocompletePopup();

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
  //promote a match requiring inline autocomplete too highly.
  void StartAutocomplete(const std::wstring& text,
                         const std::wstring& desired_tld,
                         bool prevent_inline_autocomplete);

  // Closes the window and cancels any pending asynchronous queries
  void StopAutocomplete();

  // Returns true if no autocomplete query is currently running.
  bool query_in_progress() const { return query_in_progress_; }

  // Returns true if the popup is currently open.
  bool is_open() const { return m_hWnd != NULL; }

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

  // Returns a pointer to a heap-allocated AutocompleteLog containing the
  // current input text, selected match, and result set.  The caller is
  // responsible for deleting the object.
  AutocompleteLog* GetAutocompleteLog();

  // Immediately updates and opens the popup if necessary, then moves the
  // current selection down (|count| > 0) or up (|count| < 0), clamping to the
  // first or last result if necessary.  If |count| == 0, the selection will be
  // unchanged, but the popup will still redraw and modify the text in the
  // AutocompleteEdit.
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

  // Returns the AutocompleteController used by this popup.
  AutocompleteController* autocomplete_controller() const {
    return controller_.get();
  }

  const AutocompleteResult* latest_result() const {
    return &latest_result_;
  }

  // The match the user has manually chosen, if any.
  AutocompleteResult::Selection manually_selected_match_;

  // The token value for selected_line_, hover_line_ and functions dealing with
  // a "line number" that indicates "no line".
  static const size_t kNoMatch = -1;

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

  // Sets the correct default match in latest_result_, then updates the popup
  // appearance to match.  If |immediately| is true this update happens
  // synchronously; otherwise, it's deferred until the next scheduled update.
  void SetDefaultMatchAndUpdate(bool immediately);

  // If an update is pending or |force| is true, immediately updates result_ to
  // match latest_result_, and calls UpdatePopup() to reflect those changes
  // back to the user.
  void CommitLatestResults(bool force);

  // Redraws the popup window to match any changes in result_; this may mean
  // opening or closing the window.
  void UpdatePopupAppearance();

  // Gives the topmost y coordinate within |line|, which should be within the
  // range of valid lines.
  int LineTopPixel(size_t line) const;

  // Converts the given y-coordinate to a line.  Due to drawing slop (window
  // borders, etc.), |y| might be within the window but outside the range of
  // pixels which correspond to lines; in this case the result will be clamped,
  // i.e., the top and bottom lines will be treated as extending to the top and
  // bottom edges of the window, respectively.
  size_t PixelToLine(int y) const;

  // Call to change the hovered line.  |line| should be within the range of
  // valid lines (to enable hover) or kNoMatch (to disable hover).
  void SetHoveredLine(size_t line);

  // Call to change the selected line.  This will update all state and repaint
  // the necessary parts of the window, as well as updating the edit with the
  // new temporary text.  |line| should be within the range of valid lines.
  // NOTE: This assumes the popup is open, and thus both old and new values for
  // the selected line should not be kNoMatch.
  void SetSelectedLine(size_t line);

  // Invalidates one line of the autocomplete popup.
  void InvalidateLine(size_t line);

  // Draws a light border around the inside of the window with the given client
  // rectangle and DC.
  void DrawBorder(const RECT& rc, HDC dc);

  // Draw a string at the specified location with the specified style.
  // This function is a wrapper function of the DrawText() function to handle
  // bidirectional strings.
  // Parameters
  //   * dc [in] (HDC)
  //     Represents the display context to render the given string.
  //   * x [in] (int)
  //     Specifies the left of the bounding rectangle,
  //   * y [in] (int)
  //     Specifies the top of the bounding rectangle,
  //   * max_x [in] (int)
  //     Specifies the right of the bounding rectangle.
  //   * text [in] (const wchar_t*)
  //     Specifies the pointer to the string to be rendered.
  //   * length [in] (int)
  //     Specifies the number of characters in the string.
  //   * style [in] (int)
  //     Specifies the classifications for this string.
  //     This value is a combination of the following values:
  //       - ACMatchClassifications::NONE
  //       - ACMatchClassifications::URL
  //       - ACMatchClassifications::MATCH
  //       - ACMatchClassifications::DIM
  //   * status [in] (const DrawLineInfo::LineStatus)
  //     Specifies the classifications for this line.
  //   * context [in] (const MirroringContext*)
  //     Specifies the context used for mirroring the x-coordinates.
  //   * text_direction_is_rtl [in] (bool)
  //    Determines whether we need to render the string as an RTL string, which
  //    impacts, for example, which side leading/trailing whitespace and
  //    punctuation appear on.
  // Return Values
  //   * a positive value
  //     Represents the width of the displayed string, in pixels.
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

  // Gets the selected keyword or keyword hint for the given match.  Returns
  // true if |keyword| represents a keyword hint, or false if |keyword|
  // represents a selected keyword.  (|keyword| will always be set [though
  // possibly to the empty string], and you cannot have both a selected keyword
  // and a keyword hint simultaneously.)
  bool GetKeywordForMatch(const AutocompleteMatch& match,
                          std::wstring* keyword);

  AutocompleteEdit* editor_;
  scoped_ptr<AutocompleteController> controller_;

  // Profile for current tab.
  Profile* profile_;

  // Cached GDI information for drawing.
  DrawLineInfo line_info_;

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
  //
  // NOTE: Both coalesce_timer_ and max_delay_timer_ (below) are set up during
  // the constructor, and are guaranteed non-NULL for the life of the popup.
  scoped_ptr<Timer> coalesce_timer_;

  // Timer that tracks how long it's been since the last time we updated the
  // onscreen results.  This is used to ensure that the popup is somewhat
  // responsive even when the user types continuously.
  scoped_ptr<Timer> max_delay_timer_;

  // The line that's currently hovered.  If we're not drawing a hover rect,
  // this will be kNoMatch, even if the cursor is over the popup contents.
  size_t hovered_line_;

  // When hover_line_ is kNoMatch, this holds the screen coordinates of the
  // mouse position when hover tracking was turned off.  If the mouse moves to a
  // point over the popup that has different coordinates, hover tracking will be
  // re-enabled.  When hover_line_ is a valid line, the value here is
  // out-of-date and should be ignored.
  CPoint last_hover_coordinates_;

  // The currently selected line.  This is kNoMatch when nothing is selected,
  // which should only be true when the popup is closed.
  size_t selected_line_;

  // Bitmap for the star.  This is owned by the ResourceBundle.
  SkBitmap* star_;

  // A context used for mirroring regions.
  scoped_ptr<MirroringContext> mirroring_context_;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_H__

