// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/native_theme.h"

#include <windows.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <vssym32.h>

#include "base/gfx/gdi_util.h"
#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/scoped_handle.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_win.h"
#include "skia/include/SkShader.h"

namespace gfx {

/* static */
const NativeTheme* NativeTheme::instance() {
  // The global NativeTheme instance.
  static const NativeTheme s_native_theme;
  return &s_native_theme;
}

NativeTheme::NativeTheme()
    : theme_dll_(LoadLibrary(L"uxtheme.dll")),
      draw_theme_(NULL),
      draw_theme_ex_(NULL),
      get_theme_color_(NULL),
      get_theme_content_rect_(NULL),
      get_theme_part_size_(NULL),
      open_theme_(NULL),
      close_theme_(NULL),
      set_theme_properties_(NULL),
      is_theme_active_(NULL),
      get_theme_int_(NULL) {
  if (theme_dll_) {
    draw_theme_ = reinterpret_cast<DrawThemeBackgroundPtr>(
        GetProcAddress(theme_dll_, "DrawThemeBackground"));
    draw_theme_ex_ = reinterpret_cast<DrawThemeBackgroundExPtr>(
        GetProcAddress(theme_dll_, "DrawThemeBackgroundEx"));
    get_theme_color_ = reinterpret_cast<GetThemeColorPtr>(
        GetProcAddress(theme_dll_, "GetThemeColor"));
    get_theme_content_rect_ = reinterpret_cast<GetThemeContentRectPtr>(
        GetProcAddress(theme_dll_, "GetThemeBackgroundContentRect"));
    get_theme_part_size_ = reinterpret_cast<GetThemePartSizePtr>(
        GetProcAddress(theme_dll_, "GetThemePartSize"));
    open_theme_ = reinterpret_cast<OpenThemeDataPtr>(
        GetProcAddress(theme_dll_, "OpenThemeData"));
    close_theme_ = reinterpret_cast<CloseThemeDataPtr>(
        GetProcAddress(theme_dll_, "CloseThemeData"));
    set_theme_properties_ = reinterpret_cast<SetThemeAppPropertiesPtr>(
        GetProcAddress(theme_dll_, "SetThemeAppProperties"));
    is_theme_active_ = reinterpret_cast<IsThemeActivePtr>(
        GetProcAddress(theme_dll_, "IsThemeActive"));
    get_theme_int_ = reinterpret_cast<GetThemeIntPtr>(
        GetProcAddress(theme_dll_, "GetThemeInt"));
  }
  memset(theme_handles_, 0, sizeof(theme_handles_));
}

NativeTheme::~NativeTheme() {
  if (theme_dll_) {
    // todo (cpu): fix this soon.
    // CloseHandles();
    FreeLibrary(theme_dll_);
  }
}

HRESULT NativeTheme::PaintButton(HDC hdc,
                                 int part_id,
                                 int state_id,
                                 int classic_state,
                                 RECT* rect) const {
  HANDLE handle = GetThemeHandle(BUTTON);
  if (handle && draw_theme_)
    return draw_theme_(handle, hdc, part_id, state_id, rect, NULL);

  // Draw it manually.
  // All pressed states have both low bits set, and no other states do.
  const bool focused = ((state_id & ETS_FOCUSED) == ETS_FOCUSED);
  const bool pressed = ((state_id & PBS_PRESSED) == PBS_PRESSED);
  if ((BP_PUSHBUTTON == part_id) && (pressed || focused)) {
    // BP_PUSHBUTTON has a focus rect drawn around the outer edge, and the
    // button itself is shrunk by 1 pixel.
    HBRUSH brush = GetSysColorBrush(COLOR_3DDKSHADOW);
    if (brush) {
      FrameRect(hdc, rect, brush);
      InflateRect(rect, -1, -1);
    }
  }
  DrawFrameControl(hdc, rect, DFC_BUTTON, classic_state);

  // Draw the focus rectangle (the dotted line box) only on buttons.  For radio
  // and checkboxes, we let webkit draw the focus rectangle (orange glow).
  if ((BP_PUSHBUTTON == part_id) && focused) {
    // The focus rect is inside the button.  The exact number of pixels depends
    // on whether we're in classic mode or using uxtheme.
    if (handle && get_theme_content_rect_) {
      get_theme_content_rect_(handle, hdc, part_id, state_id, rect, rect);
    } else {
      InflateRect(rect, -GetSystemMetrics(SM_CXEDGE), 
                  -GetSystemMetrics(SM_CYEDGE));
    }
    DrawFocusRect(hdc, rect);
  }

  return S_OK;
}

HRESULT NativeTheme::PaintTextField(HDC hdc,
                                    int part_id,
                                    int state_id,
                                    int classic_state,
                                    RECT* rect,
                                    COLORREF color,
                                    bool fill_content_area,
                                    bool draw_edges) const {
  // TODO(ojan): http://b/1210017 Figure out how to give the ability to
  // exclude individual edges from being drawn.

  HANDLE handle = GetThemeHandle(TEXTFIELD);
  // TODO(mpcomplete): can we detect if the color is specified by the user,
  // and if not, just use the system color?
  // CreateSolidBrush() accepts a RGB value but alpha must be 0.
  HBRUSH bg_brush = CreateSolidBrush(color);
  HRESULT hr;
  // DrawThemeBackgroundEx was introduced in XP SP2, so that it's possible
  // draw_theme_ex_ is NULL and draw_theme_ is non-null.
  if (handle && (draw_theme_ex_ || (draw_theme_ && draw_edges))) {
    if (draw_theme_ex_) {
      static DTBGOPTS omit_border_options = {
        sizeof(DTBGOPTS),
        DTBG_OMITBORDER,
        {0,0,0,0}
      };
      DTBGOPTS* draw_opts = draw_edges ? NULL : &omit_border_options;
      hr = draw_theme_ex_(handle, hdc, part_id, state_id, rect, draw_opts);
    } else {
      hr = draw_theme_(handle, hdc, part_id, state_id, rect, NULL);
    }

    // TODO(maruel): Need to be fixed if get_theme_content_rect_ is NULL.
    if (fill_content_area && get_theme_content_rect_) {
      RECT content_rect;
      hr = get_theme_content_rect_(handle, hdc, part_id, state_id, rect,
                                   &content_rect);
      FillRect(hdc, &content_rect, bg_brush);
    }
  } else {
    // Draw it manually.
    if (draw_edges)
      DrawEdge(hdc, rect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);

    if (fill_content_area) {
      FillRect(hdc, rect, (classic_state & DFCS_INACTIVE) ?
                   reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1) : bg_brush);
    }
    hr = S_OK;
  }
  DeleteObject(bg_brush);
  return hr;
}

HRESULT NativeTheme::PaintMenuList(HDC hdc,
                                   int part_id,
                                   int state_id,
                                   int classic_state,
                                   RECT* rect) const {
  HANDLE handle = GetThemeHandle(MENULIST);
  if (handle && draw_theme_)
    return draw_theme_(handle, hdc, part_id, state_id, rect, NULL);

  // Draw it manually.
  DrawFrameControl(hdc, rect, DFC_SCROLL, DFCS_SCROLLCOMBOBOX | classic_state);
  return S_OK;
}

HRESULT NativeTheme::PaintScrollbarArrow(HDC hdc,
                                         int state_id,
                                         int classic_state,
                                         RECT* rect) const {
  HANDLE handle = GetThemeHandle(SCROLLBAR);
  if (handle && draw_theme_)
    return draw_theme_(handle, hdc, SBP_ARROWBTN, state_id, rect, NULL);

  // Draw it manually.
  DrawFrameControl(hdc, rect, DFC_SCROLL, classic_state);
  return S_OK;
}

HRESULT NativeTheme::PaintScrollbarTrack(HDC hdc,
                                         int part_id,
                                         int state_id,
                                         int classic_state,
                                         RECT* target_rect,
                                         RECT* align_rect,
                                         skia::PlatformCanvasWin* canvas) const {
  HANDLE handle = GetThemeHandle(SCROLLBAR);
  if (handle && draw_theme_)
    return draw_theme_(handle, hdc, part_id, state_id, target_rect, NULL);

  // Draw it manually.
  const DWORD colorScrollbar = GetSysColor(COLOR_SCROLLBAR);
  const DWORD color3DFace = GetSysColor(COLOR_3DFACE);
  if ((colorScrollbar != color3DFace) &&
      (colorScrollbar != GetSysColor(COLOR_WINDOW))) {
    FillRect(hdc, target_rect, reinterpret_cast<HBRUSH>(COLOR_SCROLLBAR + 1));
  } else {
    // Create a 2x2 checkerboard pattern using the 3D face and highlight
    // colors.
    SkColor face = skia::COLORREFToSkColor(color3DFace);
    SkColor highlight = skia::COLORREFToSkColor(GetSysColor(COLOR_3DHILIGHT));
    SkColor buffer[] = { face, highlight, highlight, face };
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 2, 2);
    bitmap.setPixels(buffer);
    SkShader* shader = SkShader::CreateBitmapShader(bitmap,
                                                    SkShader::kRepeat_TileMode,
                                                    SkShader::kRepeat_TileMode);

    // Draw that pattern into the target rect, setting the origin to the top
    // left corner of the scrollbar track (so the checked rect below the thumb
    // aligns properly with the portion above the thumb).
    SkMatrix matrix;
    matrix.setTranslate(SkIntToScalar(align_rect->left),
                        SkIntToScalar(align_rect->top));
    shader->setLocalMatrix(matrix);
    SkPaint paint;
    paint.setShader(shader)->unref();
    canvas->drawIRect(skia::RECTToSkIRect(*target_rect), paint);
  }
  if (classic_state & DFCS_PUSHED)
    InvertRect(hdc, target_rect);
  return S_OK;
}

HRESULT NativeTheme::PaintScrollbarThumb(HDC hdc,
                                         int part_id,
                                         int state_id,
                                         int classic_state,
                                         RECT* rect) const {
  HANDLE handle = GetThemeHandle(SCROLLBAR);
  if (handle && draw_theme_)
    return draw_theme_(handle, hdc, part_id, state_id, rect, NULL);

  // Draw it manually.
  if ((part_id == SBP_THUMBBTNHORZ) || (part_id == SBP_THUMBBTNVERT))
    DrawEdge(hdc, rect, EDGE_RAISED, BF_RECT | BF_MIDDLE);
  // Classic mode doesn't have a gripper.
  return S_OK;
}

HRESULT NativeTheme::PaintStatusGripper(HDC hdc,
                                        int part_id,
                                        int state_id,
                                        int classic_state,
                                        RECT* rect) const {
  HANDLE handle = GetThemeHandle(STATUS);
  if (handle && draw_theme_) {
    // Paint the status bar gripper.  There doesn't seem to be a
    // standard gripper in Windows for the space between
    // scrollbars.  This is pretty close, but it's supposed to be
    // painted over a status bar.
    return draw_theme_(handle, hdc, SP_GRIPPER, 0, rect, 0);
  }

  // Draw a windows classic scrollbar gripper.
  DrawFrameControl(hdc, rect, DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
  return S_OK;
}

HRESULT NativeTheme::PaintDialogBackground(HDC hdc, bool active,
                                           RECT* rect) const {
  HANDLE handle = GetThemeHandle(WINDOW);
  if (handle && draw_theme_) {
    return draw_theme_(handle, hdc, WP_DIALOG,
                       active ? FS_ACTIVE : FS_INACTIVE, rect, NULL);
  }

  // Classic just renders a flat color background.
  FillRect(hdc, rect, reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1));
  return S_OK;
}

HRESULT NativeTheme::PaintTabPanelBackground(HDC hdc, RECT* rect) const {
  HANDLE handle = GetThemeHandle(TAB);
  if (handle && draw_theme_)
    return draw_theme_(handle, hdc, TABP_BODY, 0, rect, NULL);

  // Classic just renders a flat color background.
  FillRect(hdc, rect, reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1));
  return S_OK;
}

HRESULT NativeTheme::PaintListBackground(HDC hdc,
                                         bool enabled,
                                         RECT* rect) const {
  HANDLE handle = GetThemeHandle(LIST);
  if (handle && draw_theme_)
    return draw_theme_(handle, hdc, 1, TS_NORMAL, rect, NULL);

  // Draw it manually.
  HBRUSH bg_brush = GetSysColorBrush(COLOR_WINDOW);
  FillRect(hdc, rect, bg_brush);
  DrawEdge(hdc, rect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);
  return S_OK;
}

bool NativeTheme::IsThemingActive() const {
  if (is_theme_active_)
    return !!is_theme_active_();
  return false;
}

HRESULT NativeTheme::PaintMenuArrow(ThemeName theme,
                                    HDC hdc,
                                    int part_id,
                                    int state_id,
                                    RECT* rect,
                                    MenuArrowDirection arrow_direction,
                                    bool is_highlighted) const {
  HANDLE handle = GetThemeHandle(MENU);
  if (handle && draw_theme_) {
    if (arrow_direction == RIGHT_POINTING_ARROW) {
      return draw_theme_(handle, hdc, part_id, state_id, rect, NULL);
    } else {
      // There is no way to tell the uxtheme API to draw a left pointing arrow;
      // it doesn't have a flag equivalent to DFCS_MENUARROWRIGHT.  But they
      // are needed for RTL locales on Vista.  So use a memory DC and mirror
      // the region with GDI's StretchBlt.
      Rect r(*rect);
      ScopedHDC mem_dc(CreateCompatibleDC(hdc));
      ScopedBitmap mem_bitmap(CreateCompatibleBitmap(hdc, r.width(),
                                                     r.height()));
      HGDIOBJ old_bitmap = SelectObject(mem_dc, mem_bitmap);
      // Copy and horizontally mirror the background from hdc into mem_dc. Use
      // a negative-width source rect, starting at the rightmost pixel.
      StretchBlt(mem_dc, 0, 0, r.width(), r.height(),
                 hdc, r.right()-1, r.y(), -r.width(), r.height(), SRCCOPY);
      // Draw the arrow.
      RECT theme_rect = {0, 0, r.width(), r.height()};
      HRESULT result = draw_theme_(handle, mem_dc, part_id,
                                   state_id, &theme_rect, NULL);
      // Copy and mirror the result back into mem_dc.
      StretchBlt(hdc, r.x(), r.y(), r.width(), r.height(),
                 mem_dc, r.width()-1, 0, -r.width(), r.height(), SRCCOPY);
      SelectObject(mem_dc, old_bitmap);
      return result;
    }
  }

  // For some reason, Windows uses the name DFCS_MENUARROWRIGHT to indicate a
  // left pointing arrow. This makes the following 'if' statement slightly
  // counterintuitive.
  UINT state;
  if (arrow_direction == RIGHT_POINTING_ARROW)
    state = DFCS_MENUARROW;
  else
    state = DFCS_MENUARROWRIGHT;
  return PaintFrameControl(hdc, rect, DFC_MENU, state, is_highlighted);
}

HRESULT NativeTheme::PaintMenuBackground(ThemeName theme,
                                         HDC hdc,
                                         int part_id,
                                         int state_id,
                                         RECT* rect) const {
  HANDLE handle = GetThemeHandle(MENU);
  if (handle && draw_theme_) {
    HRESULT result = draw_theme_(handle, hdc, part_id, state_id, rect, NULL);
    FrameRect(hdc, rect, GetSysColorBrush(COLOR_3DSHADOW));
    return result;
  }

  FillRect(hdc, rect, GetSysColorBrush(COLOR_MENU));
  DrawEdge(hdc, rect, EDGE_RAISED, BF_RECT);
  return S_OK;
}

HRESULT NativeTheme::PaintMenuCheckBackground(ThemeName theme,
                                              HDC hdc,
                                              int part_id,
                                              int state_id,
                                              RECT* rect) const {
  HANDLE handle = GetThemeHandle(MENU);
  if (handle && draw_theme_)
    return draw_theme_(handle, hdc, part_id, state_id, rect, NULL);
  // Nothing to do for background.
  return S_OK;
}

HRESULT NativeTheme::PaintMenuCheck(ThemeName theme,
                                    HDC hdc,
                                    int part_id,
                                    int state_id,
                                    RECT* rect,
                                    bool is_highlighted) const {
  HANDLE handle = GetThemeHandle(MENU);
  if (handle && draw_theme_) {
    return draw_theme_(handle, hdc, part_id, state_id, rect, NULL);
  }
  return PaintFrameControl(hdc, rect, DFC_MENU, DFCS_MENUCHECK, is_highlighted);
}

HRESULT NativeTheme::PaintMenuGutter(HDC hdc,
                                     int part_id,
                                     int state_id,
                                     RECT* rect) const {
  HANDLE handle = GetThemeHandle(MENU);
  if (handle && draw_theme_)
    return draw_theme_(handle, hdc, part_id, state_id, rect, NULL);
  return E_NOTIMPL;
}

HRESULT NativeTheme::PaintMenuSeparator(HDC hdc,
                                        int part_id,
                                        int state_id,
                                        RECT* rect) const {
  HANDLE handle = GetThemeHandle(MENU);
  if (handle && draw_theme_)
    return draw_theme_(handle, hdc, part_id, state_id, rect, NULL);
  DrawEdge(hdc, rect, EDGE_ETCHED, BF_TOP);
  return S_OK;
}

HRESULT NativeTheme::PaintMenuItemBackground(ThemeName theme,
                                             HDC hdc,
                                             int part_id,
                                             int state_id,
                                             bool selected,
                                             RECT* rect) const {
  HANDLE handle = GetThemeHandle(MENU);
  if (handle && draw_theme_)
    return draw_theme_(handle, hdc, part_id, state_id, rect, NULL);
  if (selected)
    FillRect(hdc, rect, GetSysColorBrush(COLOR_HIGHLIGHT));
  return S_OK;
}

HRESULT NativeTheme::GetThemePartSize(ThemeName theme_name,
                                      HDC hdc,
                                      int part_id,
                                      int state_id,
                                      RECT* rect,
                                      int ts,
                                      SIZE* size) const {
  HANDLE handle = GetThemeHandle(theme_name);
  if (handle && get_theme_part_size_)
    return get_theme_part_size_(handle, hdc, part_id, state_id, rect, ts, size);

  return E_NOTIMPL;
}

HRESULT NativeTheme::GetThemeColor(ThemeName theme,
                                   int part_id,
                                   int state_id,
                                   int prop_id,
                                   SkColor* color) const {
  HANDLE handle = GetThemeHandle(theme);
  if (handle && get_theme_color_) {
    COLORREF color_ref;
    if (get_theme_color_(handle, part_id, state_id, prop_id, &color_ref) ==
        S_OK) {
      *color = skia::COLORREFToSkColor(color_ref);
      return S_OK;
    }
  }
  return E_NOTIMPL;
}

SkColor NativeTheme::GetThemeColorWithDefault(ThemeName theme,
                                              int part_id,
                                              int state_id,
                                              int prop_id,
                                              int default_sys_color) const {
  SkColor color;
  if (GetThemeColor(theme, part_id, state_id, prop_id, &color) != S_OK)
    color = skia::COLORREFToSkColor(GetSysColor(default_sys_color));
  return color;
}

HRESULT NativeTheme::GetThemeInt(ThemeName theme,
                                 int part_id,
                                 int state_id,
                                 int prop_id,
                                 int *value) const {
  HANDLE handle = GetThemeHandle(theme);
  if (handle && get_theme_int_)
    return get_theme_int_(handle, part_id, state_id, prop_id, value);
  return E_NOTIMPL;
}

Size NativeTheme::GetThemeBorderSize(ThemeName theme) const {
  // For simplicity use the wildcard state==0, part==0, since it works
  // for the cases we currently depend on.
  int border;
  if (GetThemeInt(theme, 0, 0, TMT_BORDERSIZE, &border) == S_OK)
    return Size(border, border);
  else
    return Size(GetSystemMetrics(SM_CXEDGE), GetSystemMetrics(SM_CYEDGE));
}


void NativeTheme::DisableTheming() const {
  if (!set_theme_properties_)
    return;
  set_theme_properties_(0);
}

HRESULT NativeTheme::PaintFrameControl(HDC hdc,
                                       RECT* rect,
                                       UINT type,
                                       UINT state,
                                       bool is_highlighted) const {
  const int width = rect->right - rect->left;
  const int height = rect->bottom - rect->top;

  // DrawFrameControl for menu arrow/check wants a monochrome bitmap.
  ScopedBitmap mask_bitmap(CreateBitmap(width, height, 1, 1, NULL));

  if (mask_bitmap == NULL)
    return E_OUTOFMEMORY;

  ScopedHDC bitmap_dc(CreateCompatibleDC(NULL));
  HGDIOBJ org_bitmap = SelectObject(bitmap_dc, mask_bitmap);
  RECT local_rect = { 0, 0, width, height };
  DrawFrameControl(bitmap_dc, &local_rect, type, state);

  // We're going to use BitBlt with a b&w mask. This results in using the dest
  // dc's text color for the black bits in the mask, and the dest dc's
  // background color for the white bits in the mask. DrawFrameControl draws the
  // check in black, and the background in white.
  COLORREF old_bg_color =
      SetBkColor(hdc,
                 GetSysColor(is_highlighted ? COLOR_HIGHLIGHT : COLOR_MENU));
  COLORREF old_text_color =
      SetTextColor(hdc,
                   GetSysColor(is_highlighted ? COLOR_HIGHLIGHTTEXT :
                                                COLOR_MENUTEXT));
  BitBlt(hdc, rect->left, rect->top, width, height, bitmap_dc, 0, 0, SRCCOPY);
  SetBkColor(hdc, old_bg_color);
  SetTextColor(hdc, old_text_color);

  SelectObject(bitmap_dc, org_bitmap);

  return S_OK;
}

void NativeTheme::CloseHandles() const
{
  if (!close_theme_)
    return;

  for (int i = 0; i < LAST; ++i) {
    if (theme_handles_[i])
      close_theme_(theme_handles_[i]);
      theme_handles_[i] = NULL;
  }
}

HANDLE NativeTheme::GetThemeHandle(ThemeName theme_name) const
{
  if (!open_theme_ || theme_name < 0 || theme_name >= LAST)
    return 0;

  if (theme_handles_[theme_name])
    return theme_handles_[theme_name];

  // Not found, try to load it.
  HANDLE handle = 0;
  switch (theme_name) {
  case NativeTheme::BUTTON:
    handle = open_theme_(NULL, L"Button");
    break;
  case NativeTheme::TEXTFIELD:
    handle = open_theme_(NULL, L"Edit");
    break;
  case NativeTheme::MENULIST:
    handle = open_theme_(NULL, L"Combobox");
    break;
  case NativeTheme::SCROLLBAR:
    handle = open_theme_(NULL, L"Scrollbar");
    break;
  case NativeTheme::STATUS:
    handle = open_theme_(NULL, L"Status");
    break;
  case NativeTheme::MENU:
    handle = open_theme_(NULL, L"Menu");
    break;
  case NativeTheme::WINDOW:
    handle = open_theme_(NULL, L"Window");
    break;
  case NativeTheme::TAB:
    handle = open_theme_(NULL, L"Tab");
    break;
  case NativeTheme::LIST:
    handle = open_theme_(NULL, L"Listview");
    break;
  default:
    NOTREACHED();
  }
  theme_handles_[theme_name] = handle;
  return handle;
}

}  // namespace gfx

