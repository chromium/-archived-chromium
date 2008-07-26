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

#include "chrome/browser/autocomplete/autocomplete_popup.h"

#include <cmath>

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/template_url.h"
#include "chrome/browser/template_url_model.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/resource_bundle.h"
#include "third_party/icu38/public/common/unicode/ubidi.h"

namespace {
// The amount of time we'll wait after a provider returns before updating,
// in order to coalesce results.
const int kPopupCoalesceMs = 100;

// The maximum time we'll allow the popup to go without updating.
const int kPopupUpdateMaxDelayMs = 300;

// Padding between text and the star indicator, in pixels.
const int kStarPadding = 4;

};

// A simple wrapper class for the bidirectional iterator of ICU.
// This class uses the bidirctional iterator of ICU to split a line of
// bidirectional texts into visual runs in its display order.
class BiDiLineIterator {
 public:
  BiDiLineIterator();
  ~BiDiLineIterator();

  // Initialize the bidirectional iterator with the specified text.
  // Parameters
  //   * text [in] (const std::wstring&)
  //     Represents the text to be iterated with this iterator.
  //   * right_to_left [in] (bool)
  //     Represents whether or not the default text direction is right-to-left.
  //     Possible parameters are listed below:
  //     - true, the default text direction is right-to-left.
  //     - false, the default text direction is left-to-right.
  //   * url [in] (bool)
  //     Represents whether or not this text is a URL.
  // Return values
  //   * true
  //     The bidirectional iterator is created successfully.
  //   * false
  //     An error occured while creating the bidirectional iterator.
  UBool Open(const std::wstring& text, bool right_to_left, bool url);

  // Retrieve the number of visual runs in the text.
  // Return values
  //   * A positive value
  //     Represents the number of visual runs.
  //   * 0
  //     Represents an error.
  int CountRuns();

  // Get the logical offset, length, and direction of the specified visual run.
  // Parameters
  //   * index [in] (int)
  //     Represents the index of the visual run. This value must be less than
  //     the return value of the CountRuns() function.
  //   * start [out] (int*)
  //     Represents the index of the specified visual run, in characters.
  //   * length [out] (int*)
  //     Represents the length of the specified visual run, in characters.
  // Return values
  //   * UBIDI_LTR
  //     Represents this run should be rendered in the left-to-right reading
  //     order.
  //   * UBIDI_RTL
  //     Represents this run should be rendered in the right-to-left reading
  //     order.
  UBiDiDirection GetVisualRun(int index, int* start, int* length);

 private:
  UBiDi* bidi_;

  DISALLOW_EVIL_CONSTRUCTORS(BiDiLineIterator);
};

BiDiLineIterator::BiDiLineIterator()
    : bidi_(NULL) {
}

BiDiLineIterator::~BiDiLineIterator() {
  if (bidi_) {
    ubidi_close(bidi_);
    bidi_ = NULL;
  }
}

UBool BiDiLineIterator::Open(const std::wstring& text,
                             bool right_to_left,
                             bool url) {
  DCHECK(bidi_ == NULL);
  UErrorCode error = U_ZERO_ERROR;
  bidi_ = ubidi_openSized(static_cast<int>(text.length()), 0, &error);
  if (U_FAILURE(error))
    return false;
  if (right_to_left && url)
    ubidi_setReorderingMode(bidi_, UBIDI_REORDER_RUNS_ONLY);
  ubidi_setPara(bidi_, text.data(), static_cast<int>(text.length()),
                right_to_left ? UBIDI_DEFAULT_RTL : UBIDI_DEFAULT_LTR,
                NULL, &error);
  return U_SUCCESS(error);
}

int BiDiLineIterator::CountRuns() {
  DCHECK(bidi_ != NULL);
  UErrorCode error = U_ZERO_ERROR;
  const int runs = ubidi_countRuns(bidi_, &error);
  return U_SUCCESS(error) ? runs : 0;
}

UBiDiDirection BiDiLineIterator::GetVisualRun(int index,
                                              int* start,
                                              int* length) {
  DCHECK(bidi_ != NULL);
  return ubidi_getVisualRun(bidi_, index, start, length);
}

// This class implements a utility used for mirroring x-coordinates when the
// application language is a right-to-left one.
class MirroringContext {
 public:
  MirroringContext();
  ~MirroringContext();

  // Initializes the bounding region used for mirroring coordinates.
  // This class uses the center of this region as an axis for calculating
  // mirrored coordinates.
  void Initialize(int min_x, int max_x, bool enabled);

  // Return the "left" side of the specified region.
  // When the application language is a right-to-left one, this function
  // calculates the mirrored coordinates of the input region and returns the
  // left side of the mirrored region.
  // The input region must be in the bounding region specified in the
  // Initialize() function.
  int GetLeft(int min_x, int max_x) const;

  // Returns whether or not we are mirroring the x coordinate.
  bool enabled() const {
    return enabled_;
  }

 private:
  int min_x_;
  int center_x_;
  int max_x_;
  bool enabled_;

  DISALLOW_EVIL_CONSTRUCTORS(MirroringContext);
};

MirroringContext::MirroringContext()
    : min_x_(0),
      center_x_(0),
      max_x_(0),
      enabled_(false) {
}

MirroringContext::~MirroringContext() {
}

void MirroringContext::Initialize(int min_x, int max_x, bool enabled) {
  min_x_ = min_x;
  max_x_ = max_x;
  if (min_x_ > max_x_)
    std::swap(min_x_, max_x_);
  center_x_ = min_x + (max_x - min_x) / 2;
  enabled_ = enabled;
}

int MirroringContext::GetLeft(int min_x, int max_x) const {
  if (!enabled_)
    return min_x;
  int mirrored_min_x = center_x_ + (center_x_ - min_x);
  int mirrored_max_x = center_x_ + (center_x_ - max_x);
  return std::min(mirrored_min_x, mirrored_max_x);
}

const wchar_t AutocompletePopup::DrawLineInfo::ellipsis_str[] = L"\x2026";

AutocompletePopup::AutocompletePopup(const ChromeFont& font,
                                     AutocompleteEdit* editor,
                                     Profile* profile)
    : editor_(editor),
      controller_(new AutocompleteController(this, profile)),
      profile_(profile),
      line_info_(font),
      query_in_progress_(false),
      update_pending_(false),
      // Creating the Timers directly instead of using StartTimer() ensures
      // they won't actually start running until we use ResetTimer().
      coalesce_timer_(new Timer(kPopupCoalesceMs, this, false)),
      max_delay_timer_(new Timer(kPopupUpdateMaxDelayMs, this, true)),
      hovered_line_(kNoMatch),
      selected_line_(kNoMatch),
      mirroring_context_(new MirroringContext()),
      star_(ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_CONTENT_STAR_ON)) {
}

AutocompletePopup::~AutocompletePopup() {
  StopAutocomplete();
}

void AutocompletePopup::SetProfile(Profile* profile) {
  DCHECK(profile);
  profile_ = profile;
  controller_->SetProfile(profile);
}

void AutocompletePopup::StartAutocomplete(
    const std::wstring& text,
    const std::wstring& desired_tld,
    bool prevent_inline_autocomplete) {
  // The user is interacting with the edit, so stop tracking hover.
  SetHoveredLine(kNoMatch);

  // See if we can avoid rerunning autocomplete when the query hasn't changed
  // much.  If the popup isn't open, we threw the past results away, so no
  // shortcuts are possible.
  const AutocompleteInput input(text, desired_tld, prevent_inline_autocomplete);
  bool minimal_changes = false;
  if (is_open()) {
    // When the user hits escape with a temporary selection, the edit asks us
    // to update, but the text it supplies hasn't changed since the last query.
    // Instead of stopping or rerunning the last query, just do an immediate
    // repaint with the new (probably NULL) provider affinity.
    if (input_.Equals(input)) {
      SetDefaultMatchAndUpdate(true);
      return;
    }

    // When the user presses or releases the ctrl key, the desired_tld changes,
    // and when the user finishes an IME composition, inline autocomplete may
    // no longer be prevented.  In both these cases the text itself hasn't
    // changed since the last query, and some providers can do much less work
    // (and get results back more quickly).  Taking advantage of this reduces
    // flicker.
    if (input_.text() == text)
      minimal_changes = true;
  }
  input_ = input;

  // If we're starting a brand new query, stop caring about any old query.
  TimerManager* const tm = MessageLoop::current()->timer_manager();
  if (!minimal_changes && query_in_progress_) {
    update_pending_ = false;
    tm->StopTimer(coalesce_timer_.get());
  }

  // Start the new query.
  query_in_progress_ = !controller_->Start(input_, minimal_changes, false);
  controller_->GetResult(&latest_result_);

  // If we're not ready to show results and the max update interval timer isn't
  // already running, start it now.
  if (query_in_progress_ && !tm->IsTimerRunning(max_delay_timer_.get()))
    tm->ResetTimer(max_delay_timer_.get());

  SetDefaultMatchAndUpdate(!query_in_progress_);
}

void AutocompletePopup::StopAutocomplete() {
  // Close any old query.
  if (query_in_progress_) {
    controller_->Stop();
    query_in_progress_ = false;
    update_pending_ = false;
  }

  // Reset results.  This will force the popup to close.
  latest_result_.Reset();
  CommitLatestResults(true);

  // Clear input_ to make sure we don't try and use any of these results for
  // the next query we receive.  Strictly speaking this isn't necessary, since
  // the popup isn't open, but it keeps our internal state consistent and
  // serves as future-proofing in case the code in StartAutocomplete() changes.
  input_.Clear();
}

std::wstring AutocompletePopup::URLsForCurrentSelection(
    PageTransition::Type* transition,
    bool* is_history_what_you_typed_match,
    std::wstring* alternate_nav_url) const {
  // The popup may be out of date, and we always want the latest match. The
  // most common case of this is when the popup is open, the user changes the
  // contents of the edit, and then presses enter before any results have been
  // displayed, but still wants to choose the would-be-default action.
  //
  // Can't call CommitLatestResults(), because
  // latest_result_.default_match_index() may not match selected_line_, and
  // we want to preserve the user's selection.
  if (latest_result_.empty())
    return std::wstring();

  const AutocompleteResult* result;
  AutocompleteResult::const_iterator match;
  if (update_pending_) {
    // The default match on the latest result should be up-to-date. If the user
    // changed the selection since that result was generated using the arrow
    // keys, Move() will have force updated the popup.
    result = &latest_result_;
    match = result->default_match();
  } else {
    result = &result_;
    DCHECK(selected_line_ < result_.size());
    match = result->begin() + selected_line_;
  }
  if (transition)
    *transition = match->transition;
  if (is_history_what_you_typed_match)
    *is_history_what_you_typed_match = match->is_history_what_you_typed_match;
  if (alternate_nav_url && manually_selected_match_.empty())
    *alternate_nav_url = result->GetAlternateNavURL(input_, match);
  return match->destination_url;
}

std::wstring AutocompletePopup::URLsForDefaultMatch(
    const std::wstring& text,
    const std::wstring& desired_tld,
    PageTransition::Type* transition,
    bool* is_history_what_you_typed_match,
    std::wstring* alternate_nav_url) {
  // Cancel any existing query.
  StopAutocomplete();

  // Run the new query and get only the synchronously available results.
  const AutocompleteInput input(text, desired_tld, true);
  const bool done = controller_->Start(input, false, true);
  DCHECK(done);
  controller_->GetResult(&result_);
  if (result_.empty())
    return std::wstring();

  // Get the URLs for the default match.
  result_.SetDefaultMatch(manually_selected_match_);
  const AutocompleteResult::const_iterator match = result_.default_match();
  const std::wstring url(match->destination_url);  // Need to copy since we
                                                     // reset result_ below.
  if (transition)
    *transition = match->transition;
  if (is_history_what_you_typed_match)
    *is_history_what_you_typed_match = match->is_history_what_you_typed_match;
  if (alternate_nav_url && manually_selected_match_.empty())
    *alternate_nav_url = result_.GetAlternateNavURL(input, match);
  result_.Reset();

  return url;
}

AutocompleteLog* AutocompletePopup::GetAutocompleteLog() {
  return new AutocompleteLog(input_.text(), selected_line_, 0, result_);
}

void AutocompletePopup::Move(int count) {
  // The user is using the keyboard to change the selection, so stop tracking
  // hover.
  SetHoveredLine(kNoMatch);

  // Force the popup to open/update, so the user is interacting with the
  // latest results.
  CommitLatestResults(false);
  if (result_.empty())
    return;

  // Clamp the new line to [0, result_.count() - 1].
  const size_t new_line = selected_line_ + count;
  SetSelectedLine(((count < 0) && (new_line >= selected_line_)) ?
      0 : std::min(new_line, result_.size() - 1));
}

void AutocompletePopup::TryDeletingCurrentItem() {
  // We could use URLsForCurrentSelection() here, but it seems better to try
  // and shift-delete the actual selection, rather than any "in progress, not
  // yet visible" one.
  if (selected_line_ == kNoMatch)
    return;
  const AutocompleteMatch& match = result_.match_at(selected_line_);
  if (match.deletable) {
    query_in_progress_ = true;
    size_t selected_line = selected_line_;
    match.provider->DeleteMatch(match);  // This will synchronously call back
                                         // to OnAutocompleteUpdate()
    CommitLatestResults(false);
    if (!result_.empty()) {
      // Move the selection to the next choice after the deleted one, but clear
      // the manual selection so this choice doesn't act "sticky".
      //
      // It might also be correct to only call Clear() here when
      // manually_selected_match_ didn't already have a provider() (i.e. when
      // there was no existing manual selection).  It's not clear what the user
      // wants when they shift-delete something they've arrowed to.  If they
      // arrowed down just to shift-delete it, we should probably Clear(); if
      // they arrowed to do something else, then got a bad match while typing,
      // we probably shouldn't.
      SetSelectedLine(std::min(result_.size() - 1, selected_line));
      manually_selected_match_.Clear();
    }
  }
}

void AutocompletePopup::OnAutocompleteUpdate(bool updated_result,
                                             bool query_complete) {
  DCHECK(query_in_progress_);
  if (updated_result)
    controller_->GetResult(&latest_result_);
  query_in_progress_ = !query_complete;

  SetDefaultMatchAndUpdate(query_complete);
}

void AutocompletePopup::Run() {
  CommitLatestResults(false);
}

void AutocompletePopup::OnLButtonDown(UINT keys, const CPoint& point) {
  const size_t new_hovered_line = PixelToLine(point.y);
  SetHoveredLine(new_hovered_line);
  SetSelectedLine(new_hovered_line);
}

void AutocompletePopup::OnMButtonDown(UINT keys, const CPoint& point) {
  SetHoveredLine(PixelToLine(point.y));
}

void AutocompletePopup::OnLButtonUp(UINT keys, const CPoint& point) {
  OnButtonUp(point, CURRENT_TAB);
}

void AutocompletePopup::OnMButtonUp(UINT keys, const CPoint& point) {
  OnButtonUp(point, NEW_BACKGROUND_TAB);
}

LRESULT AutocompletePopup::OnMouseActivate(HWND window,
                                           UINT hit_test,
                                           UINT mouse_message) {
  return MA_NOACTIVATE;
}

void AutocompletePopup::OnMouseLeave() {
  // The mouse has left the window, so no line is hovered.
  SetHoveredLine(kNoMatch);
}

void AutocompletePopup::OnMouseMove(UINT keys, const CPoint& point) {
  // Track hover when
  // (a) The left or middle button is down (the user is interacting via the
  //     mouse)
  // (b) The user moves the mouse from where we last stopped tracking hover
  // (c) We started tracking previously due to (a) or (b) and haven't stopped
  //     yet (user hasn't used the keyboard to interact again)
  const bool action_button_pressed = !!(keys & (MK_MBUTTON | MK_LBUTTON));
  CPoint screen_point(point);
  ClientToScreen(&screen_point);
  if (action_button_pressed || (last_hover_coordinates_ != screen_point) ||
      (hovered_line_ != kNoMatch)) {
    // Determine the hovered line from the y coordinate of the event.  We don't
    // need to check whether the x coordinates are within the window since if
    // they weren't someone else would have received the WM_MOUSEMOVE.
    const size_t new_hovered_line = PixelToLine(point.y);
    SetHoveredLine(new_hovered_line);

    // When the user has the left button down, update their selection
    // immediately (don't wait for mouseup).
    if (keys & MK_LBUTTON)
      SetSelectedLine(new_hovered_line);
  }
}

void AutocompletePopup::OnPaint(HDC other_dc) {
  DCHECK(!result_.empty());  // Shouldn't be drawing an empty popup

  CPaintDC dc(m_hWnd);

  RECT rc;
  GetClientRect(&rc);
  mirroring_context_->Initialize(rc.left, rc.right,
      l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT);
  DrawBorder(rc, dc);

  bool all_descriptions_empty = true;
  for (AutocompleteResult::const_iterator i(result_.begin());
       i != result_.end(); ++i) {
    if (!i->description.empty()) {
      all_descriptions_empty = false;
      break;
    }
  }

  // Only repaint the invalid lines.
  const size_t first_line = PixelToLine(dc.m_ps.rcPaint.top);
  const size_t last_line = PixelToLine(dc.m_ps.rcPaint.bottom);
  for (size_t i = first_line; i <= last_line; ++i) {
    DrawLineInfo::LineStatus status;
    // Selection should take precedence over hover.
    if (i == selected_line_)
      status = DrawLineInfo::SELECTED;
    else if (i == hovered_line_)
      status = DrawLineInfo::HOVERED;
    else
      status = DrawLineInfo::NORMAL;
    DrawEntry(dc, rc, i, status, all_descriptions_empty,
              result_.match_at(i).starred);
  }
}

void AutocompletePopup::OnButtonUp(const CPoint& point,
                                   WindowOpenDisposition disposition) {
  const size_t selected_line = PixelToLine(point.y);
  const AutocompleteMatch& match = result_.match_at(selected_line);
  // OpenURL() may close the popup, which will clear the result set and, by
  // extension, |match| and its contents.  So copy the relevant strings out to
  // make sure they stay alive until the call completes.
  const std::wstring url(match.destination_url);
  std::wstring keyword;
  const bool is_keyword_hint = GetKeywordForMatch(match, &keyword);
  editor_->OpenURL(url, disposition, match.transition, std::wstring(),
                   selected_line, is_keyword_hint ? std::wstring() : keyword);
}

void AutocompletePopup::SetDefaultMatchAndUpdate(bool immediately) {
  if (!latest_result_.SetDefaultMatch(manually_selected_match_) &&
      !query_in_progress_) {
    // We don't clear the provider affinity because the user didn't do
    // something to indicate that they want a different provider, we just
    // couldn't find the specific match they wanted.
    manually_selected_match_.destination_url.clear();
    manually_selected_match_.is_history_what_you_typed_match = false;
  }

  if (immediately) {
    CommitLatestResults(true);
  } else if (!update_pending_) {
    // Coalesce the results for the next kPopupCoalesceMs milliseconds.
    update_pending_ = true;
    MessageLoop::current()->timer_manager()->ResetTimer(coalesce_timer_.get());
  }

  // Update the edit with the possibly new data for this match.
  // NOTE: This must be done after the code above, so that our internal state
  // will be consistent when the edit calls back to URLsForCurrentSelection().
  std::wstring inline_autocomplete_text;
  std::wstring keyword;
  bool is_keyword_hint = false;
  bool can_show_search_hint = true;
  const AutocompleteResult::const_iterator match(
      latest_result_.default_match());
  if (match != latest_result_.end()) {
    if ((match->inline_autocomplete_offset != std::wstring::npos) &&
        (match->inline_autocomplete_offset < match->fill_into_edit.length())) {
      inline_autocomplete_text =
          match->fill_into_edit.substr(match->inline_autocomplete_offset);
    }
    // Warm up DNS Prefetch Cache.
    chrome_browser_net::DnsPrefetchUrlString(match->destination_url);
    // We could prefetch the alternate nav URL, if any, but because there can be
    // many of these as a user types an initial series of characters, the OS DNS
    // cache could suffer eviction problems for minimal gain.

    is_keyword_hint = GetKeywordForMatch(*match, &keyword);
    can_show_search_hint = (match->type == AutocompleteMatch::SEARCH);
  }
  editor_->OnPopupDataChanged(inline_autocomplete_text, false,
      manually_selected_match_ /* ignored */, keyword, is_keyword_hint,
      can_show_search_hint);
}

void AutocompletePopup::CommitLatestResults(bool force) {
  if (!force && !update_pending_)
    return;

  update_pending_ = false;

  // If we're going to trim the window size to no longer include the hovered
  // line, turn hover off first.  We need to do this before changing result_
  // since SetHover() should be able to trust that the old and new hovered
  // lines are valid.
  //
  // Practically, this only currently happens when we're closing the window by
  // setting latest_result_ to an empty list.
  if ((hovered_line_ != kNoMatch) && (latest_result_.size() <= hovered_line_))
    SetHoveredLine(kNoMatch);

  result_.CopyFrom(latest_result_);
  selected_line_ = (result_.default_match() == result_.end()) ?
      kNoMatch : (result_.default_match() - result_.begin());

  UpdatePopupAppearance();

  // The max update interval timer either needs to be reset (if more updates
  // are to come) or stopped (when we're done with the query).  The coalesce
  // timer should always just be stopped.
  TimerManager* const tm = MessageLoop::current()->timer_manager();
  tm->StopTimer(coalesce_timer_.get());
  if (query_in_progress_)
    tm->ResetTimer(max_delay_timer_.get());
  else
    tm->StopTimer(max_delay_timer_.get());
}

void AutocompletePopup::UpdatePopupAppearance() {
  if (result_.empty()) {
    // No results, close any existing popup.
    if (m_hWnd) {
      DestroyWindow();
      m_hWnd = NULL;
    }
    return;
  }

  // Figure the coordinates of the popup:
  // Get the coordinates of the location bar view; these are returned relative
  // to its parent.
  CRect rc;
  editor_->parent_view()->GetBounds(&rc);
  // Subtract the top left corner to make the coordinates relative to the
  // location bar view itself, and convert to screen coordinates.
  CPoint top_left(-rc.TopLeft());
  ChromeViews::View::ConvertPointToScreen(editor_->parent_view(), &top_left);
  rc.OffsetRect(top_left);
  // Expand by one pixel on each side since that's the amount the location bar
  // view is inset from the divider line that edges the adjacent buttons.
  // Deflate the top and bottom by the height of the extra graphics around the
  // edit.
  // TODO(pkasting): http://b/972786 This shouldn't be hardcoded to rely on
  // LocationBarView constants.  Instead we should just make the edit be "at the
  // right coordinates", or something else generic.
  rc.InflateRect(1, -LocationBarView::kTextVertMargin);
  // Now rc is the exact width we want and is positioned like the edit would
  // be, so shift the top and bottom downwards so the new top is where the old
  // bottom is and the rect has the height we need for all our entries, plus a
  // one-pixel border on top and bottom.
  rc.top = rc.bottom;
  rc.bottom += static_cast<int>(result_.size()) * line_info_.line_height + 2;

  if (!m_hWnd) {
    // To prevent this window from being activated, we create an invisible
    // window and manually show it without activating it.
    Create(editor_->m_hWnd, rc, AUTOCOMPLETEPOPUP_CLASSNAME, WS_POPUP,
           WS_EX_TOOLWINDOW);
    // When an IME is attached to the rich-edit control, retrieve its window
    // handle and show this popup window under the IME windows.
    // Otherwise, show this popup window under top-most windows.
    // TODO(hbono): http://b/1111369 if we exclude this popup window from the
    // display area of IME windows, this workaround becomes unnecessary.
    HWND ime_window = ImmGetDefaultIMEWnd(editor_->m_hWnd);
    SetWindowPos(ime_window ? ime_window : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
  } else {
    // Already open, just resize the window.  This is a bit tricky; we want to
    // repaint the whole window, since the contents may have changed, but
    // MoveWindow() won't repaint portions that haven't moved or been
    // added/removed.  So we first call InvalidateRect(), so the next repaint
    // paints the whole window, then tell MoveWindow() to do the actual
    // repaint, which will also properly repaint Windows formerly under the
    // popup.
    InvalidateRect(NULL, false);
    MoveWindow(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, true);
  }

  // TODO(pkasting): http://b/1111369 We should call ImmSetCandidateWindow() on
  // the editor_'s IME context here, and exclude ourselves from its display
  // area.  Not clear what to pass for the lpCandidate->ptCurrentPos member,
  // though...
}

int AutocompletePopup::LineTopPixel(size_t line) const {
  DCHECK(line <= result_.size());
  // The popup has a 1 px top border.
  return line_info_.line_height * static_cast<int>(line) + 1;
}

size_t AutocompletePopup::PixelToLine(int y) const {
  const size_t line = std::max(y - 1, 0) / line_info_.line_height;
  return (line < result_.size()) ? line : (result_.size() - 1);
}

void AutocompletePopup::SetHoveredLine(size_t line) {
  const bool is_disabling = (line == kNoMatch);
  DCHECK(is_disabling || (line < result_.size()));

  if (line == hovered_line_)
    return;  // Nothing to do

  const bool is_enabling = (hovered_line_ == kNoMatch);
  if (is_enabling || is_disabling) {
    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    if (is_disabling) {
      // Save the current mouse position to check against for re-enabling.
      GetCursorPos(&last_hover_coordinates_);  // Returns screen coordinates

      // Cancel existing registration for WM_MOUSELEAVE notifications.
      tme.dwFlags = TME_CANCEL | TME_LEAVE;
    } else {
      // Register for WM_MOUSELEAVE notifications.
      tme.dwFlags = TME_LEAVE;
    }
    tme.hwndTrack = m_hWnd;
    tme.dwHoverTime = HOVER_DEFAULT;  // Not actually used
    TrackMouseEvent(&tme);
  }

  // Make sure the old hovered line is redrawn.  No need to redraw the selected
  // line since selection overrides hover so the appearance won't change.
  if (!is_enabling && (hovered_line_ != selected_line_))
    InvalidateLine(hovered_line_);

  // Change the hover to the new line and make sure it's redrawn.
  hovered_line_ = line;
  if (!is_disabling && (hovered_line_ != selected_line_))
    InvalidateLine(hovered_line_);
}

void AutocompletePopup::SetSelectedLine(size_t line) {
  DCHECK(line < result_.size());
  if (result_.empty())
    return;

  if (line == selected_line_)
    return;  // Nothing to do

  // Update the edit with the new data for this match.
  const AutocompleteMatch& match = result_.match_at(line);
  std::wstring keyword;
  const bool is_keyword_hint = GetKeywordForMatch(match, &keyword);
  editor_->OnPopupDataChanged(match.fill_into_edit, true,
                              manually_selected_match_, keyword,
                              is_keyword_hint,
                              (match.type == AutocompleteMatch::SEARCH));

  // Track the user's selection until they cancel it.
  manually_selected_match_.destination_url = match.destination_url;
  manually_selected_match_.provider_affinity = match.provider;
  manually_selected_match_.is_history_what_you_typed_match =
      match.is_history_what_you_typed_match;

  // Repaint old and new selected lines immediately, so that the edit doesn't
  // appear to update [much] faster than the popup.  We must not update
  // |selected_line_| before calling OnPopupDataChanged() (since the edit may
  // call us back to get data about the old selection), and we must not call
  // UpdateWindow() before updating |selected_line_| (since the paint routine
  // relies on knowing the correct selected line).
  InvalidateLine(selected_line_);
  selected_line_ = line;
  InvalidateLine(selected_line_);
  UpdateWindow();
}

void AutocompletePopup::InvalidateLine(size_t line) {
  DCHECK(line < result_.size());

  RECT rc;
  GetClientRect(&rc);
  rc.top = LineTopPixel(line);
  rc.bottom = rc.top + line_info_.line_height;
  InvalidateRect(&rc, false);
}

// Draws a light border around the inside of the window with the given client
// rectangle and DC.
void AutocompletePopup::DrawBorder(const RECT& rc, HDC dc) {
  HPEN hpen = CreatePen(PS_SOLID, 1, RGB(199, 202, 206));
  HGDIOBJ old_pen = SelectObject(dc, hpen);

  int width = rc.right - rc.left - 1;
  int height = rc.bottom - rc.top - 1;

  MoveToEx(dc, 0, 0, NULL);
  LineTo(dc, 0, height);
  LineTo(dc, width, height);
  LineTo(dc, width, 0);
  LineTo(dc, 0, 0);

  SelectObject(dc, old_pen);
  DeleteObject(hpen);
}

int AutocompletePopup::DrawString(HDC dc,
                                  int x,
                                  int y,
                                  int max_x,
                                  const wchar_t* text,
                                  int length,
                                  int style,
                                  const DrawLineInfo::LineStatus status,
                                  const MirroringContext* context,
                                  bool text_direction_is_rtl) const {
  if (length <= 0)
    return 0;

  // Set up the text decorations.
  SelectObject(dc, (style & ACMatchClassification::MATCH) ?
      line_info_.bold_font.hfont() : line_info_.regular_font.hfont());
  const COLORREF foreground = (style & ACMatchClassification::URL) ?
      line_info_.url_colors[status] : line_info_.text_colors[status];
  const COLORREF background = line_info_.background_colors[status];
  SetTextColor(dc, (style & ACMatchClassification::DIM) ?
      DrawLineInfo::AlphaBlend(foreground, background, 0xAA) : foreground);

  // Retrieve the width of the decorated text and display it. When we cannot
  // display this fragment in the given width, we trim the fragment and add an
  // ellipsis.
  //
  // TODO(hbono): http:///b/1222425 We should change the following eliding code
  // with more aggressive one.
  int text_x = x;
  int max_length = 0;
  SIZE text_size = {0};
  GetTextExtentExPoint(dc, text, length,
                       max_x - line_info_.ellipsis_width - text_x, &max_length,
                       NULL, &text_size);

  if (max_length < length)
    GetTextExtentPoint32(dc, text, max_length, &text_size);

  const int mirrored_x = context->GetLeft(text_x, text_x + text_size.cx);
  RECT text_bounds = {mirrored_x,
                      0,
                      mirrored_x + text_size.cx,
                      line_info_.line_height};

  int flags = DT_SINGLELINE | DT_NOPREFIX;
  if (text_direction_is_rtl)
    // In order to make sure RTL text is displayed correctly (for example, a
    // trailing space should be displayed on the left and not on the right), we
    // pass the flag DT_RTLREADING.
    flags |= DT_RTLREADING;

  DrawText(dc, text, length, &text_bounds, flags);
  text_x += text_size.cx;

  // Draw the ellipsis. Note that since we use the mirroring context, the
  // ellipsis are drawn either to the right or to the left of the text.
  if (max_length < length) {
    TextOut(dc, context->GetLeft(text_x, text_x + line_info_.ellipsis_width),
            0, line_info_.ellipsis_str, arraysize(line_info_.ellipsis_str) - 1);
    text_x += line_info_.ellipsis_width;
  }

  return text_x - x;
}

void AutocompletePopup::DrawMatchFragments(
    HDC dc,
    const std::wstring& text,
    const ACMatchClassifications& classifications,
    int x,
    int y,
    int max_x,
    DrawLineInfo::LineStatus status) const {
  if (!text.length())
    return;

  // Check whether or not this text is a URL string.
  // A URL string is basically in English with possible included words in
  // Arabic or Hebrew. For such case, ICU provides a special algorithm and we
  // should use it.
  bool url = false;
  for (ACMatchClassifications::const_iterator i = classifications.begin();
       i != classifications.end(); ++i) {
    if (i->style & ACMatchClassification::URL)
      url = true;
  }

  // Initialize a bidirectional line iterator of ICU and split the text into
  // visual runs. (A visual run is consecutive characters which have the same
  // display direction and should be displayed at once.)
  BiDiLineIterator bidi_line;
  if (!bidi_line.Open(text, mirroring_context_->enabled(), url))
    return;
  const int runs = bidi_line.CountRuns();

  // Draw the visual runs.
  // This loop splits each run into text fragments with the given
  // classifications and draws the text fragments.
  // When the direction of a run is right-to-left, we have to mirror the
  // x-coordinate of this run and render the fragments in the right-to-left
  // reading order. To handle this display order independently from the one of
  // this popup window, this loop renders a run with the steps below:
  // 1. Create a local display context for each run;
  // 2. Render the run into the local display context, and;
  // 3. Copy the local display context to the one of the popup window.
  int run_x = x;
  for (int run = 0; run < runs; ++run) {
    int run_start = 0;
    int run_length = 0;

    // The index we pass to GetVisualRun corresponds to the position of the run
    // in the displayed text. For example, the string "Google in HEBREW" (where
    // HEBREW is text in the Hebrew language) has two runs: "Google in " which
    // is an LTR run, and "HEBREW" which is an RTL run. In an LTR context, the
    // run "Google in " has the index 0 (since it is the leftmost run
    // displayed). In an RTL context, the same run has the index 1 because it
    // is the rightmost run. This is why the order in which we traverse the
    // runs is different depending on the locale direction.
    //
    // Note that for URLs we always traverse the runs from lower to higher
    // indexes because the return order of runs for a URL always matches the
    // physical order of the context.
    int current_run =
        (mirroring_context_->enabled() && !url) ? (runs - run - 1) : run;
    const UBiDiDirection run_direction = bidi_line.GetVisualRun(current_run,
                                                                &run_start,
                                                                &run_length);
    const int run_end = run_start + run_length;

    // Set up a local display context for rendering this run.
    int text_x = 0;
    const int text_max_x = max_x - run_x;
    MirroringContext run_context;
    run_context.Initialize(0, text_max_x, run_direction == UBIDI_RTL);

    // In addition to creating a mirroring context for the run, we indicate
    // whether the run needs to be rendered as RTL text. The mirroring context
    // alone in not sufficient because there are cases where a mirrored RTL run
    // needs to be rendered in an LTR context (for example, an RTL run within
    // an URL).
    bool run_direction_is_rtl = (run_direction == UBIDI_RTL) && !url;
    CDC text_dc(CreateCompatibleDC(dc));
    CBitmap text_bitmap(CreateCompatibleBitmap(dc, text_max_x,
                                               line_info_.font_height));
    SelectObject(text_dc, text_bitmap);
    const RECT text_rect = {0, 0, text_max_x, line_info_.line_height};
    FillRect(text_dc, &text_rect, line_info_.brushes[status]);
    SetBkMode(text_dc, TRANSPARENT);

    // Split this run with the given classifications and draw the fragments
    // into the local display context.
    for (ACMatchClassifications::const_iterator i = classifications.begin();
         i != classifications.end(); ++i) {
      const int text_start = std::max(run_start, static_cast<int>(i->offset));
      const int text_end = std::min(run_end, (i != classifications.end() - 1) ?
          static_cast<int>((i + 1)->offset) : run_end);
      text_x += DrawString(text_dc, text_x, 0, text_max_x, &text[text_start],
                           text_end - text_start, i->style, status,
                           &run_context, run_direction_is_rtl);
    }

    // Copy the local display context to the one of the popup window and
    // delete the local display context.
    BitBlt(dc, mirroring_context_->GetLeft(run_x, run_x + text_x), y, text_x,
           line_info_.line_height, text_dc, run_context.GetLeft(0, text_x), 0,
           SRCCOPY);
    run_x += text_x;
  }
}

void AutocompletePopup::DrawEntry(HDC dc,
                                  const RECT& client_rect,
                                  size_t line,
                                  DrawLineInfo::LineStatus status,
                                  bool all_descriptions_empty,
                                  bool starred) const {
  // Calculate outer bounds of entry, and fill background.
  const int top_pixel = LineTopPixel(line);
  const RECT rc = {1, top_pixel, client_rect.right - client_rect.left - 1,
                   top_pixel + line_info_.line_height};
  FillRect(dc, &rc, line_info_.brushes[status]);

  // Calculate and display contents/description sections as follows:
  // * 2 px top margin, bottom margin is handled by line_height.
  const int y = rc.top + 2;

  // * 1 char left/right margin.
  const int side_margin = line_info_.ave_char_width;

  // * 50% of the remaining width is initially allocated to each section, with
  //   a 2 char margin followed by the star column and kStarPadding padding.
  const int content_min_x = rc.left + side_margin;
  const int description_max_x = rc.right - side_margin;
  const int mid_line = (description_max_x - content_min_x) / 2 + content_min_x;
  const int star_col_width = kStarPadding + star_->width();
  const int content_right_margin = line_info_.ave_char_width * 2;

  // * If this would make the content section display fewer than 40 characters,
  //   the content section is increased to that minimum at the expense of the
  //   description section.
  const int content_width =
      std::max(mid_line - content_min_x - content_right_margin,
               line_info_.ave_char_width * 40);
  const int description_width = description_max_x - content_min_x -
      content_width - star_col_width;

  // * If this would make the description section display fewer than 20
  //   characters, or if there are no descriptions to display or the result is
  //   the HISTORY_SEARCH shortcut, the description section is eliminated, and
  //   all the available width is used for the content section.
  int star_x;
  const AutocompleteMatch& match = result_.match_at(line);
  if ((description_width < (line_info_.ave_char_width * 20)) ||
      all_descriptions_empty ||
      (match.type == AutocompleteMatch::HISTORY_SEARCH)) {
    star_x = description_max_x - star_col_width + kStarPadding;
    DrawMatchFragments(dc, match.contents, match.contents_class, content_min_x,
                       y, star_x - kStarPadding, status);
  } else {
    star_x = description_max_x - description_width - star_col_width;
    DrawMatchFragments(dc, match.contents, match.contents_class, content_min_x,
                       y, content_min_x + content_width, status);
    DrawMatchFragments(dc, match.description, match.description_class,
                       description_max_x - description_width, y,
                       description_max_x, status);
  }
  if (starred)
    DrawStar(dc, star_x,
             (line_info_.line_height - star_->height()) / 2 + top_pixel);
}

void AutocompletePopup::DrawStar(HDC dc, int x, int y) const {
  ChromeCanvas canvas(star_->width(), star_->height(), false);
  // Make the background completely transparent.
  canvas.drawColor(SK_ColorBLACK, SkPorterDuff::kClear_Mode);
  canvas.DrawBitmapInt(*star_, 0, 0);
  canvas.getTopPlatformDevice().drawToHDC(
      dc, mirroring_context_->GetLeft(x, x + star_->width()), y, NULL);
}

bool AutocompletePopup::GetKeywordForMatch(const AutocompleteMatch& match,
                                           std::wstring* keyword) {
  // Assume we have no keyword until we find otherwise.
  keyword->clear();

  // If the current match is a keyword, return that as the selected keyword.
  if (match.template_url && match.template_url->url() &&
      match.template_url->url()->SupportsReplacement()) {
    keyword->assign(match.template_url->keyword());
    return false;
  }

  // See if the current match's fill_into_edit corresponds to a keyword.
  if (!profile_->GetTemplateURLModel())
    return false;
  profile_->GetTemplateURLModel()->Load();
  const std::wstring keyword_hint(
      TemplateURLModel::CleanUserInputKeyword(match.fill_into_edit));
  if (keyword_hint.empty())
    return false;

  // Don't provide a hint if this keyword doesn't support replacement.
  const TemplateURL* const template_url =
      profile_->GetTemplateURLModel()->GetTemplateURLForKeyword(keyword_hint);
  if (!template_url || !template_url->url() ||
      !template_url->url()->SupportsReplacement())
    return false;

  keyword->assign(keyword_hint);
  return true;
}

AutocompletePopup::DrawLineInfo::DrawLineInfo(const ChromeFont& font) {
  // Create regular and bold fonts.
  regular_font = font.DeriveFont(-1);
  bold_font = regular_font.DeriveFont(0, ChromeFont::BOLD);

  // The total padding added to each line (bottom padding is what is
  // left over after DrawEntry() specifies its top offset).
  static const int kTotalLinePadding = 5;
  font_height = std::max(regular_font.height(), bold_font.height());
  line_height = font_height + kTotalLinePadding;
  ave_char_width = regular_font.ave_char_width();
  ellipsis_width = std::max(regular_font.GetStringWidth(ellipsis_str),
                            bold_font.GetStringWidth(ellipsis_str));

  // Create background colors.
  background_colors[NORMAL] = GetSysColor(COLOR_WINDOW);
  background_colors[SELECTED] = GetSysColor(COLOR_HIGHLIGHT);
  background_colors[HOVERED] =
      AlphaBlend(background_colors[SELECTED], background_colors[NORMAL], 0x40);

  // Create text colors.
  text_colors[NORMAL] = GetSysColor(COLOR_WINDOWTEXT);
  text_colors[HOVERED] = text_colors[NORMAL];
  text_colors[SELECTED] = GetSysColor(COLOR_HIGHLIGHTTEXT);

  // Create brushes and url colors.
  const COLORREF dark_url(0x008000);
  const COLORREF light_url(0xd0ffd0);
  for (int i = 0; i < MAX_STATUS_ENTRIES; ++i) {
    // Pick whichever URL color contrasts better.
    const double dark_contrast =
        LuminosityContrast(dark_url, background_colors[i]);
    const double light_contrast =
        LuminosityContrast(light_url, background_colors[i]);
    url_colors[i] = (dark_contrast > light_contrast) ? dark_url : light_url;

    brushes[i] = CreateSolidBrush(background_colors[i]);
  }
}

AutocompletePopup::DrawLineInfo::~DrawLineInfo() {
  for (int i = 0; i < MAX_STATUS_ENTRIES; ++i)
    DeleteObject(brushes[i]);
}

// static
double AutocompletePopup::DrawLineInfo::LuminosityContrast(COLORREF color1,
                                                           COLORREF color2) {
  // This algorithm was adapted from the following text at
  // http://juicystudio.com/article/luminositycontrastratioalgorithm.php :
  //
  // "[Luminosity contrast can be calculated as] (L1+.05) / (L2+.05) where L is
  // luminosity and is defined as .2126*R + .7152*G + .0722B using linearised
  // R, G, and B values. Linearised R (for example) = (R/FS)^2.2 where FS is
  // full scale value (255 for 8 bit color channels). L1 is the higher value
  // (of text or background) and L2 is the lower value.
  //
  // The Gamma correction and RGB constants are derived from the Standard
  // Default Color Space for the Internet (sRGB), and the 0.05 offset is
  // included to compensate for contrast ratios that occur when a value is at
  // or near zero, and for ambient light effects.
  const double l1 = Luminosity(color1);
  const double l2 = Luminosity(color2);
  return (l1 > l2) ? ((l1 + 0.05) / (l2 + 0.05)) : ((l2 + 0.05) / (l1 + 0.05));
}

// static
double AutocompletePopup::DrawLineInfo::Luminosity(COLORREF color) {
  // See comments in LuminosityContrast().
  const double linearised_r =
      pow(static_cast<double>(GetRValue(color)) / 255.0, 2.2);
  const double linearised_g =
      pow(static_cast<double>(GetGValue(color)) / 255.0, 2.2);
  const double linearised_b =
      pow(static_cast<double>(GetBValue(color)) / 255.0, 2.2);
  return (0.2126 * linearised_r) + (0.7152 * linearised_g) +
      (0.0722 * linearised_b);
}

COLORREF AutocompletePopup::DrawLineInfo::AlphaBlend(COLORREF foreground,
                                                     COLORREF background,
                                                     BYTE alpha) {
  if (alpha == 0)
    return background;
  else if (alpha == 0xff)
    return foreground;

  return RGB(
    ((GetRValue(foreground) * alpha) +
     (GetRValue(background) * (0xff - alpha))) / 0xff,
    ((GetGValue(foreground) * alpha) +
     (GetGValue(background) * (0xff - alpha))) / 0xff,
    ((GetBValue(foreground) * alpha) +
     (GetBValue(background) * (0xff - alpha))) / 0xff);
}
