// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/info_bubble.h"

#include "base/win_util.h"
#include "grit/theme_resources.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/path.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/root_view.h"
#include "chrome/views/window.h"

using views::View;

// All sizes are in pixels.

// Size of the border, along each edge.
static const int kBorderSize = 1;

// Size of the arrow.
static const int kArrowSize = 5;

// Number of pixels to the start of the arrow from the edge of the window.
static const int kArrowXOffset = 13;

// Number of pixels between the tip of the arrow and the region we're
// pointing to.
static const int kArrowToContentPadding = -4;

// Background color of the bubble.
static const SkColor kBackgroundColor = SK_ColorWHITE;

// Color of the border and arrow.
static const SkColor kBorderColor1 = SkColorSetRGB(99, 99, 99);
// Border shadow color.
static const SkColor kBorderColor2 = SkColorSetRGB(160, 160, 160);

// Intended dimensions of the bubble's corner images. If you update these,
// make sure that the OnSize code works.
static const int kInfoBubbleCornerWidth = 3;
static const int kInfoBubbleCornerHeight = 3;

// Bubble corner images.
static const SkBitmap* kInfoBubbleCornerTopLeft = NULL;
static const SkBitmap* kInfoBubbleCornerTopRight = NULL;
static const SkBitmap* kInfoBubbleCornerBottomLeft = NULL;
static const SkBitmap* kInfoBubbleCornerBottomRight = NULL;

// Margins around the content.
static const int kInfoBubbleViewTopMargin = 6;
static const int kInfoBubbleViewBottomMargin = 9;
static const int kInfoBubbleViewLeftMargin = 6;
static const int kInfoBubbleViewRightMargin = 6;

// The minimum alpha the bubble can be - because we're using a simple layered
// window (in order to get window-level alpha at the same time as using native
// controls), the window's drop shadow doesn't fade; this means if we went
// to zero alpha, you'd see a drop shadow outline against nothing.
static const int kMinimumAlpha = 72;

// InfoBubble -----------------------------------------------------------------

// static
InfoBubble* InfoBubble::Show(HWND parent_hwnd,
                             const gfx::Rect& position_relative_to,
                             views::View* content,
                             InfoBubbleDelegate* delegate) {
  InfoBubble* window = new InfoBubble();
  DLOG(WARNING) << "new bubble=" << window;
  window->Init(parent_hwnd, position_relative_to, content);
  window->ShowWindow(SW_SHOW);
  window->delegate_ = delegate;
  return window;
}

InfoBubble::InfoBubble() : content_view_(NULL), closed_(false) {
}

InfoBubble::~InfoBubble() {
}

void InfoBubble::Init(HWND parent_hwnd,
                      const gfx::Rect& position_relative_to,
                      views::View* content) {
  HWND owning_frame_hwnd = GetAncestor(parent_hwnd, GA_ROOTOWNER);
  // We should always have a frame, but there was a bug elsewhere that
  // made it possible for the frame to be NULL, so we have the check. If
  // you hit this, file a bug.
  DCHECK(BrowserView::GetBrowserViewForHWND(owning_frame_hwnd));
  parent_ = reinterpret_cast<views::Window*>(win_util::GetWindowUserData(
      owning_frame_hwnd));
  parent_->DisableInactiveRendering(true);

  if (kInfoBubbleCornerTopLeft == NULL) {
    kInfoBubbleCornerTopLeft = ResourceBundle::GetSharedInstance()
        .GetBitmapNamed(IDR_INFO_BUBBLE_CORNER_TOP_LEFT);
    kInfoBubbleCornerTopRight = ResourceBundle::GetSharedInstance()
        .GetBitmapNamed(IDR_INFO_BUBBLE_CORNER_TOP_RIGHT);
    kInfoBubbleCornerBottomLeft = ResourceBundle::GetSharedInstance()
        .GetBitmapNamed(IDR_INFO_BUBBLE_CORNER_BOTTOM_LEFT);
    kInfoBubbleCornerBottomRight = ResourceBundle::GetSharedInstance()
        .GetBitmapNamed(IDR_INFO_BUBBLE_CORNER_BOTTOM_RIGHT);
  }
  set_window_style(WS_POPUP | WS_CLIPCHILDREN);
  set_window_ex_style(WS_EX_LAYERED | WS_EX_TOOLWINDOW);
  // Because we're going to change the alpha value of the layered window we
  // don't want to use the offscreen buffer provided by WidgetWin.
  SetUseLayeredBuffer(false);
  content_view_ = CreateContentView(content);
  gfx::Rect bounds = content_view_->
      CalculateWindowBounds(parent_hwnd, position_relative_to);
  set_initial_class_style(
      (win_util::GetWinVersion() < win_util::WINVERSION_XP) ?
      0 : CS_DROPSHADOW);

  WidgetWin::Init(parent_hwnd, bounds, true);
  SetContentsView(content_view_);
  // The preferred size may differ when parented. Ask for the bounds again
  // and if they differ reset the bounds.
  gfx::Rect parented_bounds = content_view_->
      CalculateWindowBounds(parent_hwnd, position_relative_to);

  if (bounds != parented_bounds) {
    SetWindowPos(NULL, parented_bounds.x(), parented_bounds.y(),
                 parented_bounds.width(), parented_bounds.height(),
                 SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOZORDER);
    // Invoke ChangeSize, otherwise layered window isn't updated correctly.
    ChangeSize(0, CSize(parented_bounds.width(), parented_bounds.height()));
  }

  // Register the Escape accelerator for closing.
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManager(GetHWND());
  focus_manager->RegisterAccelerator(views::Accelerator(VK_ESCAPE, false,
                                                        false, false),
                                     this);

  // Set initial alpha value of the layered window.
  SetLayeredWindowAttributes(GetHWND(),
                             RGB(0xFF, 0xFF, 0xFF),
                             kMinimumAlpha,
                             LWA_ALPHA);

  fade_animation_.reset(new SlideAnimation(this));
  fade_animation_->Show();
}

void InfoBubble::Close() {
  Close(false);
}

void InfoBubble::AnimationProgressed(const Animation* animation) {
  int alpha = static_cast<int>(static_cast<double>
      (fade_animation_->GetCurrentValue() * (255.0 - kMinimumAlpha) +
      kMinimumAlpha));

  SetLayeredWindowAttributes(GetHWND(),
                             RGB(0xFF, 0xFF, 0xFF),
                             alpha,
                             LWA_ALPHA);
  // Don't need to invoke paint as SetLayeredWindowAttributes handles that for
  // us.
}

bool InfoBubble::AcceleratorPressed(const views::Accelerator& accelerator) {
  DCHECK(accelerator.GetKeyCode() == VK_ESCAPE);
  if (!delegate_ || delegate_->CloseOnEscape()) {
    Close(true);
    return true;
  }
  return false;
}

void InfoBubble::OnSize(UINT param, const CSize& size) {
  SetWindowRgn(content_view_->GetMask(size), TRUE);
}

void InfoBubble::OnActivate(UINT action, BOOL minimized, HWND window) {
  // The popup should close when it is deactivated.
  if (action == WA_INACTIVE && !closed_) {
    Close();
  } else if (action == WA_ACTIVE) {
    DCHECK(GetRootView()->GetChildViewCount() > 0);
    GetRootView()->GetChildViewAt(0)->RequestFocus();
  }
}

InfoBubble::ContentView* InfoBubble::CreateContentView(View* content) {
  return new ContentView(content, this);
}

void InfoBubble::Close(bool closed_by_escape) {
  if (closed_)
    return;

  // We don't fade out because it looks terrible.
  if (delegate_)
    delegate_->InfoBubbleClosing(this, closed_by_escape);
  parent_->DisableInactiveRendering(false);
  closed_ = true;
  WidgetWin::Close();
}

// ContentView ----------------------------------------------------------------

InfoBubble::ContentView::ContentView(views::View* content, InfoBubble* host)
    : host_(host) {
  if (UILayoutIsRightToLeft()) {
    arrow_edge_ = TOP_RIGHT;
  } else {
    arrow_edge_ = TOP_LEFT;
  }
  AddChildView(content);
}

gfx::Rect InfoBubble::ContentView::CalculateWindowBounds(
    HWND parent_hwnd,
    const gfx::Rect& position_relative_to) {
  gfx::Rect monitor_bounds = win_util::GetMonitorBoundsForRect(
      position_relative_to);
  // Calculate the bounds using TOP_LEFT (the default).
  gfx::Rect window_bounds = CalculateWindowBounds(position_relative_to);
  if (monitor_bounds.IsEmpty() || monitor_bounds.Contains(window_bounds))
    return window_bounds;
  // Didn't fit, adjust the edge to fit as much as we can.
  if (window_bounds.bottom() > monitor_bounds.bottom())
    SetArrowEdge(BOTTOM_LEFT);
  if (window_bounds.right() > monitor_bounds.right()) {
    if (IsTop())
      SetArrowEdge(TOP_RIGHT);
    else
      SetArrowEdge(BOTTOM_RIGHT);
  }
  // And return new bounds.
  return CalculateWindowBounds(position_relative_to);
}

gfx::Size InfoBubble::ContentView::GetPreferredSize() {
  DCHECK(GetChildViewCount() == 1);
  View* content = GetChildViewAt(0);
  gfx::Size pref = content->GetPreferredSize();
  pref.Enlarge(kBorderSize + kBorderSize + kInfoBubbleViewLeftMargin +
                   kInfoBubbleViewRightMargin,
               kBorderSize + kBorderSize + kArrowSize +
                   kInfoBubbleViewTopMargin + kInfoBubbleViewBottomMargin);
  return pref;
}

void InfoBubble::ContentView::Layout() {
  DCHECK(GetChildViewCount() == 1);
  View* content = GetChildViewAt(0);
  int x = kBorderSize;
  int y = kBorderSize;
  int content_width = width() - kBorderSize - kBorderSize -
              kInfoBubbleViewLeftMargin - kInfoBubbleViewRightMargin;
  int content_height = height() - kBorderSize - kBorderSize - kArrowSize -
               kInfoBubbleViewTopMargin - kInfoBubbleViewBottomMargin;
  if (IsTop())
    y += kArrowSize;
  x += kInfoBubbleViewLeftMargin;
  y += kInfoBubbleViewTopMargin;
  content->SetBounds(x, y, content_width, content_height);
}

HRGN InfoBubble::ContentView::GetMask(const CSize &size) {
  gfx::Path mask;

  // Redefine the window visible region so that our dropshadows look right.
  SkScalar width = SkIntToScalar(size.cx);
  SkScalar height = SkIntToScalar(size.cy);
  SkScalar arrow_size = SkIntToScalar(kArrowSize);
  SkScalar arrow_x = SkIntToScalar(
      (IsLeft() ? kArrowXOffset : width - kArrowXOffset) - 1);
  SkScalar corner_size = SkIntToScalar(kInfoBubbleCornerHeight);

  if (IsTop()) {
    // Top left corner.
    mask.moveTo(0, arrow_size + corner_size - 1);
    mask.lineTo(corner_size - 1, arrow_size);

    // Draw the arrow and the notch of the arrow.
    mask.lineTo(arrow_x - arrow_size, arrow_size);
    mask.lineTo(arrow_x, 0);
    mask.lineTo(arrow_x + 3, 0);
    mask.lineTo(arrow_x + arrow_size + 3, arrow_size);

    // Top right corner.
    mask.lineTo(width - corner_size + 1, arrow_size);
    mask.lineTo(width, arrow_size + corner_size - 1);

    // Bottom right corner.
    mask.lineTo(width, height - corner_size);
    mask.lineTo(width - corner_size, height);

    // Bottom left corner.
    mask.lineTo(corner_size, height);
    mask.lineTo(0, height - corner_size);
  } else {
    // Top left corner.
    mask.moveTo(0, corner_size - 1);
    mask.lineTo(corner_size - 1, 0);

    // Top right corner.
    mask.lineTo(width - corner_size + 1, 0);
    mask.lineTo(width, corner_size - 1);

    // Bottom right corner.
    mask.lineTo(width, height - corner_size - arrow_size);
    mask.lineTo(width - corner_size, height - arrow_size);

    // Draw the arrow and the notch of the arrow.
    mask.lineTo(arrow_x + arrow_size + 2, height - arrow_size);
    mask.lineTo(arrow_x + 2, height);
    mask.lineTo(arrow_x + 1, height);
    mask.lineTo(arrow_x - arrow_size + 1, height - arrow_size);

    // Bottom left corner.
    mask.lineTo(corner_size, height - arrow_size);
    mask.lineTo(0, height - corner_size - arrow_size);
  }

  mask.close();

  return mask.CreateHRGN();
}

void InfoBubble::ContentView::Paint(ChromeCanvas* canvas) {
  int bubble_x = 0;
  int bubble_y = 0;
  int bubble_w = width();
  int bubble_h = height() - kArrowSize;

  int border_w = bubble_w - 2 * kInfoBubbleCornerWidth;
  int border_h = bubble_h - 2 * kInfoBubbleCornerHeight;

  if (IsTop())
    bubble_y += kArrowSize;

  // Fill in the background.
  // Left side.
  canvas->FillRectInt(kBackgroundColor,
                      bubble_x, bubble_y + kInfoBubbleCornerHeight,
                      kInfoBubbleCornerWidth, border_h);
  // Center Column.
  canvas->FillRectInt(kBackgroundColor,
                      kInfoBubbleCornerWidth, bubble_y,
                      border_w, bubble_h);
  // Right Column.
  canvas->FillRectInt(kBackgroundColor,
                      bubble_w - kInfoBubbleCornerWidth,
                      bubble_y + kInfoBubbleCornerHeight,
                      kInfoBubbleCornerWidth, border_h);

  // Draw the border.
  // Top border.
  canvas->DrawRectInt(kBorderColor1,
                      kInfoBubbleCornerWidth, bubble_y,
                      border_w,
                      0);
  // Bottom border.
  canvas->DrawRectInt(kBorderColor1,
                      kInfoBubbleCornerWidth, bubble_y + bubble_h - 1,
                      border_w, 0);
  // Left border.
  canvas->DrawRectInt(kBorderColor1,
                      bubble_x, bubble_y + kInfoBubbleCornerHeight,
                      0, border_h);

  // Right border.
  canvas->DrawRectInt(kBorderColor1,
                      width() - 1, bubble_y + kInfoBubbleCornerHeight,
                      0, border_h);

  // Draw the corners.
  canvas->DrawBitmapInt(*kInfoBubbleCornerTopLeft, 0, bubble_y);
  canvas->DrawBitmapInt(*kInfoBubbleCornerTopRight,
                        bubble_w - kInfoBubbleCornerWidth, bubble_y);
  canvas->DrawBitmapInt(*kInfoBubbleCornerBottomLeft, 0,
                        bubble_y + bubble_h - kInfoBubbleCornerHeight);
  canvas->DrawBitmapInt(*kInfoBubbleCornerBottomRight,
                        bubble_w - kInfoBubbleCornerWidth,
                        bubble_y + bubble_h - kInfoBubbleCornerHeight);

  // Draw the arrow and the notch of the arrow.
  int arrow_x = IsLeft() ? kArrowXOffset : width() - kArrowXOffset;
  int arrow_y = IsTop() ? bubble_y : bubble_y + bubble_h - 1;
  const int arrow_delta = IsTop() ? -1 : 1;
  for (int i = 0, y = arrow_y; i <= kArrowSize; ++i, y += arrow_delta) {
    if (kArrowSize != i) {
      // Draw the notch formed by the arrow.
      canvas->FillRectInt(kBackgroundColor, arrow_x - (kArrowSize - i) + 1,
                          y, (kArrowSize - i) * 2 - 1, 1);
    }
    // Draw the sides of the arrow.
    canvas->FillRectInt(kBorderColor1, arrow_x - (kArrowSize - i), y, 1, 1);
    canvas->FillRectInt(kBorderColor1, arrow_x + (kArrowSize - i), y, 1, 1);
    if (i != 0) {
      canvas->FillRectInt(kBorderColor2, arrow_x - (kArrowSize - i) - 1, y, 1,
                          1);
      canvas->FillRectInt(kBorderColor2, arrow_x + (kArrowSize - i) + 1, y, 1,
                          1);
    }
  }
}

gfx::Rect InfoBubble::ContentView::CalculateWindowBounds(
    const gfx::Rect& position_relative_to) {
  gfx::Size pref = GetPreferredSize();
  int x = position_relative_to.x() + position_relative_to.width() / 2;
  int y;
  if (IsLeft())
    x -= kArrowXOffset;
  else
    x = x + kArrowXOffset - pref.width();
  if (IsTop()) {
    y = position_relative_to.bottom() + kArrowToContentPadding;
  } else {
    y = position_relative_to.y() - kArrowToContentPadding - pref.height();
  }
  return gfx::Rect(x, y, pref.width(), pref.height());
}
