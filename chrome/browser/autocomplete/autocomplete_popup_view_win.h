// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlmisc.h>

#include <string>

#include "base/scoped_ptr.h"
#include "base/win_util.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view.h"
#include "chrome/common/gfx/chrome_font.h"
#include "webkit/glue/window_open_disposition.h"

class AutocompletePopupModel;
class AutocompleteEditModel;
class AutocompleteEditView;
class Profile;
class SkBitmap;

#define AUTOCOMPLETEPOPUPVIEW_CLASSNAME L"Chrome_AutocompletePopupView"

// This class implements a popup window used to display autocomplete results.
class AutocompletePopupViewWin
    : public CWindowImpl<AutocompletePopupViewWin, CWindow, CControlWinTraits>,
      public AutocompletePopupView {
 public:
  DECLARE_WND_CLASS_EX(AUTOCOMPLETEPOPUPVIEW_CLASSNAME,
                       ((win_util::GetWinVersion() < win_util::WINVERSION_XP) ?
                           0 : CS_DROPSHADOW), COLOR_WINDOW)

  BEGIN_MSG_MAP(AutocompletePopupViewWin)
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

  AutocompletePopupViewWin(const ChromeFont& font,
                           AutocompleteEditView* edit_view,
                           AutocompleteEditModel* edit_model,
                           Profile* profile);

  // Returns true if the popup is currently open.
  virtual bool IsOpen() const { return m_hWnd != NULL; }

  // Invalidates one line of the autocomplete popup.
  virtual void InvalidateLine(size_t line);

  // Redraws the popup window to match any changes in the result set; this may
  // mean opening or closing the window.
  virtual void UpdatePopupAppearance();

  // Called by the model when hover is enabled or disabled.
  virtual void OnHoverEnabledOrDisabled(bool disabled);

  virtual void PaintUpdatesNow() { UpdateWindow(); }

  AutocompletePopupModel* model() { return model_.get(); }

 private:
  class MirroringContext;

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

  scoped_ptr<AutocompletePopupModel> model_;

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

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupViewWin);
};
