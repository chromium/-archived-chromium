// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_popup_view_win.h"

// TODO(deanm): Clean up these includes, not going to fight it now.
#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlmisc.h>
#include <cmath>

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/view.h"
#include "grit/theme_resources.h"
#include "third_party/icu38/public/common/unicode/ubidi.h"

// Padding between text and the star indicator, in pixels.
static const int kStarPadding = 4;

// This class implements a utility used for mirroring x-coordinates when the
// application language is a right-to-left one.
class AutocompletePopupViewWin::MirroringContext {
 public:
  MirroringContext() : min_x_(0), center_x_(0), max_x_(0), enabled_(false) { }

  // Initializes the bounding region used for mirroring coordinates.
  // This class uses the center of this region as an axis for calculating
  // mirrored coordinates.
  void Initialize(int x1, int x2, bool enabled);

  // Return the "left" side of the specified region.
  // When the application language is a right-to-left one, this function
  // calculates the mirrored coordinates of the input region and returns the
  // left side of the mirrored region.
  // The input region must be in the bounding region specified in the
  // Initialize() function.
  int GetLeft(int x1, int x2) const;

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

void AutocompletePopupViewWin::MirroringContext::Initialize(int x1,
                                                            int x2,
                                                            bool enabled) {
  min_x_ = std::min(x1, x2);
  max_x_ = std::max(x1, x2);
  center_x_ = min_x_ + (max_x_ - min_x_) / 2;
  enabled_ = enabled;
}

int AutocompletePopupViewWin::MirroringContext::GetLeft(int x1, int x2) const {
  return enabled_ ?
      (center_x_ + (center_x_ - std::max(x1, x2))) : std::min(x1, x2);
}

const wchar_t AutocompletePopupViewWin::DrawLineInfo::ellipsis_str[] =
    L"\x2026";

AutocompletePopupViewWin::DrawLineInfo::DrawLineInfo(const ChromeFont& font) {
  // Create regular and bold fonts.
  regular_font = font.DeriveFont(-1);
  bold_font = regular_font.DeriveFont(0, ChromeFont::BOLD);

  // The total padding added to each line (bottom padding is what is
  // left over after DrawEntry() specifies its top offset).
  static const int kTotalLinePadding = 5;
  font_height = std::max(regular_font.height(), bold_font.height());
  line_height = font_height + kTotalLinePadding;
  ave_char_width = regular_font.GetExpectedTextWidth(1);
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

AutocompletePopupViewWin::DrawLineInfo::~DrawLineInfo() {
  for (int i = 0; i < MAX_STATUS_ENTRIES; ++i)
    DeleteObject(brushes[i]);
}

// static
double AutocompletePopupViewWin::DrawLineInfo::LuminosityContrast(
    COLORREF color1,
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
double AutocompletePopupViewWin::DrawLineInfo::Luminosity(COLORREF color) {
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

COLORREF AutocompletePopupViewWin::DrawLineInfo::AlphaBlend(
    COLORREF foreground,
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

AutocompletePopupViewWin::AutocompletePopupViewWin(
    const ChromeFont& font,
    AutocompleteEditViewWin* edit_view,
    AutocompleteEditModel* edit_model,
    Profile* profile)
    : model_(new AutocompletePopupModel(this, edit_model, profile)),
      edit_view_(edit_view),
      line_info_(font),
      mirroring_context_(new MirroringContext()),
      star_(ResourceBundle::GetSharedInstance().GetBitmapNamed(
          IDR_CONTENT_STAR_ON)) {
}

void AutocompletePopupViewWin::InvalidateLine(size_t line) {
  RECT rc;
  GetClientRect(&rc);
  rc.top = LineTopPixel(line);
  rc.bottom = rc.top + line_info_.line_height;
  InvalidateRect(&rc, false);
}

void AutocompletePopupViewWin::UpdatePopupAppearance() {
  const AutocompleteResult& result = model_->result();
  if (result.empty()) {
    // No matches, close any existing popup.
    if (m_hWnd) {
      DestroyWindow();
      m_hWnd = NULL;
    }
    return;
  }

  // Figure the coordinates of the popup:
  // Get the coordinates of the location bar view; these are returned relative
  // to its parent.
  // TODO(pkasting): http://b/1345937  All this use of editor accessors should
  // die once this class is a true ChromeView.
  CRect rc = edit_view_->parent_view()->bounds().ToRECT();
  // Subtract the top left corner to make the coordinates relative to the
  // location bar view itself, and convert to screen coordinates.
  gfx::Point top_left(-rc.TopLeft());
  views::View::ConvertPointToScreen(edit_view_->parent_view(), &top_left);
  rc.OffsetRect(top_left.ToPOINT());
  // Expand by one pixel on each side since that's the amount the location bar
  // view is inset from the divider line that edges the adjacent buttons.
  // Deflate the top and bottom by the height of the extra graphics around the
  // edit.
  // TODO(pkasting): http://b/972786 This shouldn't be hardcoded to rely on
  // LocationBarView constants.  Instead we should just make the edit be "at the
  // right coordinates", or something else generic.
  rc.InflateRect(1, -LocationBarView::kVertMargin);
  // Now rc is the exact width we want and is positioned like the edit would
  // be, so shift the top and bottom downwards so the new top is where the old
  // bottom is and the rect has the height we need for all our entries, plus a
  // one-pixel border on top and bottom.
  rc.top = rc.bottom;
  rc.bottom += static_cast<int>(result.size()) * line_info_.line_height + 2;

  if (!m_hWnd) {
    // To prevent this window from being activated, we create an invisible
    // window and manually show it without activating it.
    Create(edit_view_->m_hWnd, rc, AUTOCOMPLETEPOPUPVIEW_CLASSNAME, WS_POPUP,
           WS_EX_TOOLWINDOW);
    // When an IME is attached to the rich-edit control, retrieve its window
    // handle and show this popup window under the IME windows.
    // Otherwise, show this popup window under top-most windows.
    // TODO(hbono): http://b/1111369 if we exclude this popup window from the
    // display area of IME windows, this workaround becomes unnecessary.
    HWND ime_window = ImmGetDefaultIMEWnd(edit_view_->m_hWnd);
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
  // the edit_view_'s IME context here, and exclude ourselves from its display
  // area.  Not clear what to pass for the lpCandidate->ptCurrentPos member,
  // though...
}

void AutocompletePopupViewWin::OnHoverEnabledOrDisabled(bool disabled) {
  TRACKMOUSEEVENT tme;
  tme.cbSize = sizeof(TRACKMOUSEEVENT);
  if (disabled) {
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

void AutocompletePopupViewWin::OnLButtonDown(UINT keys, const CPoint& point) {
  const size_t new_hovered_line = PixelToLine(point.y);
  model_->SetHoveredLine(new_hovered_line);
  model_->SetSelectedLine(new_hovered_line, false);
}

void AutocompletePopupViewWin::OnMButtonDown(UINT keys, const CPoint& point) {
  model_->SetHoveredLine(PixelToLine(point.y));
}

void AutocompletePopupViewWin::OnLButtonUp(UINT keys, const CPoint& point) {
  OnButtonUp(point, CURRENT_TAB);
}

void AutocompletePopupViewWin::OnMButtonUp(UINT keys, const CPoint& point) {
  OnButtonUp(point, NEW_BACKGROUND_TAB);
}

LRESULT AutocompletePopupViewWin::OnMouseActivate(HWND window,
                                               UINT hit_test,
                                               UINT mouse_message) {
  return MA_NOACTIVATE;
}

void AutocompletePopupViewWin::OnMouseLeave() {
  // The mouse has left the window, so no line is hovered.
  model_->SetHoveredLine(AutocompletePopupModel::kNoMatch);
}

void AutocompletePopupViewWin::OnMouseMove(UINT keys, const CPoint& point) {
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
      (model_->hovered_line() != AutocompletePopupModel::kNoMatch)) {
    // Determine the hovered line from the y coordinate of the event.  We don't
    // need to check whether the x coordinates are within the window since if
    // they weren't someone else would have received the WM_MOUSEMOVE.
    const size_t new_hovered_line = PixelToLine(point.y);
    model_->SetHoveredLine(new_hovered_line);

    // When the user has the left button down, update their selection
    // immediately (don't wait for mouseup).
    if (keys & MK_LBUTTON)
      model_->SetSelectedLine(new_hovered_line, false);
  }
}

void AutocompletePopupViewWin::OnPaint(HDC other_dc) {
  const AutocompleteResult& result = model_->result();
  DCHECK(!result.empty());  // Shouldn't be drawing an empty popup.

  CPaintDC dc(m_hWnd);

  RECT rc;
  GetClientRect(&rc);
  mirroring_context_->Initialize(rc.left, rc.right,
      l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT);
  DrawBorder(rc, dc);

  bool all_descriptions_empty = true;
  for (AutocompleteResult::const_iterator i(result.begin()); i != result.end();
       ++i) {
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
    if (i == model_->selected_line())
      status = DrawLineInfo::SELECTED;
    else if (i == model_->hovered_line())
      status = DrawLineInfo::HOVERED;
    else
      status = DrawLineInfo::NORMAL;
    DrawEntry(dc, rc, i, status, all_descriptions_empty,
              result.match_at(i).starred);
  }
}

void AutocompletePopupViewWin::OnButtonUp(const CPoint& point,
                                          WindowOpenDisposition disposition) {
  const size_t line = PixelToLine(point.y);
  const AutocompleteMatch& match = model_->result().match_at(line);
  // OpenURL() may close the popup, which will clear the result set and, by
  // extension, |match| and its contents.  So copy the relevant strings out to
  // make sure they stay alive until the call completes.
  const GURL url(match.destination_url);
  std::wstring keyword;
  const bool is_keyword_hint = model_->GetKeywordForMatch(match, &keyword);
  edit_view_->OpenURL(url, disposition, match.transition, GURL(), line,
                      is_keyword_hint ? std::wstring() : keyword);
}

int AutocompletePopupViewWin::LineTopPixel(size_t line) const {
  // The popup has a 1 px top border.
  return line_info_.line_height * static_cast<int>(line) + 1;
}

size_t AutocompletePopupViewWin::PixelToLine(int y) const {
  const size_t line = std::max(y - 1, 0) / line_info_.line_height;
  return std::min(line, model_->result().size() - 1);
}

// Draws a light border around the inside of the window with the given client
// rectangle and DC.
void AutocompletePopupViewWin::DrawBorder(const RECT& rc, HDC dc) {
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

int AutocompletePopupViewWin::DrawString(HDC dc,
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

void AutocompletePopupViewWin::DrawMatchFragments(
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
  l10n_util::BiDiLineIterator bidi_line;
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

void AutocompletePopupViewWin::DrawEntry(HDC dc,
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
  const AutocompleteMatch& match = model_->result().match_at(line);
  if ((description_width < (line_info_.ave_char_width * 20)) ||
      all_descriptions_empty ||
      (match.type == AutocompleteMatch::OPEN_HISTORY_PAGE)) {
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

void AutocompletePopupViewWin::DrawStar(HDC dc, int x, int y) const {
  ChromeCanvas canvas(star_->width(), star_->height(), false);
  // Make the background completely transparent.
  canvas.drawColor(SK_ColorBLACK, SkPorterDuff::kClear_Mode);
  canvas.DrawBitmapInt(*star_, 0, 0);
  canvas.getTopPlatformDevice().drawToHDC(
      dc, mirroring_context_->GetLeft(x, x + star_->width()), y, NULL);
}
