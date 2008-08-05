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

#include "chrome/browser/views/frame/aero_glass_non_client_view.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/gfx/path.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/client_view.h"
#include "chrome/views/window_delegate.h"

///////////////////////////////////////////////////////////////////////////////
// WindowResources
//
// An enumeration of bitmap resources used by this window.
enum FramePartBitmap {
  FRAME_PART_BITMAP_FIRST = 0, // must be first.

  // Client Edge Border.
  FRAME_CLIENT_EDGE_TOP_LEFT,
  FRAME_CLIENT_EDGE_TOP,
  FRAME_CLIENT_EDGE_TOP_RIGHT,
  FRAME_CLIENT_EDGE_RIGHT,
  FRAME_CLIENT_EDGE_BOTTOM_RIGHT,
  FRAME_CLIENT_EDGE_BOTTOM,
  FRAME_CLIENT_EDGE_BOTTOM_LEFT,
  FRAME_CLIENT_EDGE_LEFT,

  FRAME_PART_BITMAP_COUNT  // Must be last.
};

class WindowResources {
 public:
  virtual SkBitmap* GetPartBitmap(FramePartBitmap part) const = 0;
  virtual const ChromeFont& GetTitleFont() const = 0;
};

class AeroGlassWindowResources : public WindowResources {
 public:
  AeroGlassWindowResources() { InitClass(); }
  virtual ~AeroGlassWindowResources() { }

  // WindowResources implementation:
  virtual SkBitmap* GetPartBitmap(FramePartBitmap part) const {
    return standard_frame_bitmaps_[part];
  }
  virtual const ChromeFont& GetTitleFont() const {
    return title_font_;
  }

 private:
  static void InitClass() {
    static bool initialized = false;
    if (!initialized) {
      static const int kFramePartBitmapIds[] = {
        0,
        IDR_CONTENT_TOP_LEFT_CORNER, IDR_CONTENT_TOP_CENTER,
            IDR_CONTENT_TOP_RIGHT_CORNER, IDR_CONTENT_RIGHT_SIDE,
            IDR_CONTENT_BOTTOM_RIGHT_CORNER, IDR_CONTENT_BOTTOM_CENTER,
            IDR_CONTENT_BOTTOM_LEFT_CORNER, IDR_CONTENT_LEFT_SIDE,
        0
      };

      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      for (int i = 0; i < FRAME_PART_BITMAP_COUNT; ++i) {
        int id = kFramePartBitmapIds[i];
        if (id != 0)
          standard_frame_bitmaps_[i] = rb.GetBitmapNamed(id);
      }
      title_font_ =
          rb.GetFont(ResourceBundle::BaseFont).DeriveFont(1, ChromeFont::BOLD);
      initialized = true;
    }
  }

  static SkBitmap* standard_frame_bitmaps_[FRAME_PART_BITMAP_COUNT];
  static ChromeFont title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(AeroGlassWindowResources);
};

// static
SkBitmap* AeroGlassWindowResources::standard_frame_bitmaps_[];
ChromeFont AeroGlassWindowResources::title_font_;

WindowResources* AeroGlassNonClientView::resources_ = NULL;
SkBitmap AeroGlassNonClientView::distributor_logo_;

static const int kTitleTopOffset = 6;
static const int kTitleBottomSpacing = 6;
static const int kNoTitleTopSpacing = 8;
static const int kWindowHorizontalBorderSize = 2;
static const int kWindowVerticalBorderSize = 2;
static const int kDistributorLogoHorizontalOffset = 7;
static const int kDistributorLogoVerticalOffset = 3;

///////////////////////////////////////////////////////////////////////////////
// AeroGlassNonClientView, public:

AeroGlassNonClientView::AeroGlassNonClientView(AeroGlassFrame* frame)
    : frame_(frame) {
  InitClass();
}

AeroGlassNonClientView::~AeroGlassNonClientView() {
}

///////////////////////////////////////////////////////////////////////////////
// AeroGlassNonClientView, ChromeViews::NonClientView implementation:

gfx::Rect AeroGlassNonClientView::CalculateClientAreaBounds(int width,
                                                            int height) const {
  int top_margin = CalculateNonClientTopHeight();
  return gfx::Rect(kWindowHorizontalBorderSize, top_margin,
      std::max(0, width - (2 * kWindowHorizontalBorderSize)),
      std::max(0, height - top_margin - kWindowVerticalBorderSize));
}

gfx::Size AeroGlassNonClientView::CalculateWindowSizeForClientSize(
    int width,
    int height) const {
  int top_margin = CalculateNonClientTopHeight();
  return gfx::Size(width + (2 * kWindowHorizontalBorderSize),
                   height + top_margin + kWindowVerticalBorderSize);
}

CPoint AeroGlassNonClientView::GetSystemMenuPoint() const {
  return CPoint();
}

int AeroGlassNonClientView::NonClientHitTest(const gfx::Point& point) {
  CRect bounds;
  CPoint test_point = point.ToPOINT();

  // First see if it's within the grow box area, since that overlaps the client
  // bounds.
  int component = frame_->client_view()->NonClientHitTest(point);
  if (component != HTNOWHERE)
    return component;

  return HTNOWHERE;
}

void AeroGlassNonClientView::GetWindowMask(const gfx::Size& size,
                                           gfx::Path* window_mask) {
}

void AeroGlassNonClientView::EnableClose(bool enable) {
}

///////////////////////////////////////////////////////////////////////////////
// AeroGlassNonClientView, ChromeViews::View overrides:

void AeroGlassNonClientView::Paint(ChromeCanvas* canvas) {
  PaintDistributorLogo(canvas);
  PaintToolbarBackground(canvas);
  PaintClientEdge(canvas);

  // TODO(beng): remove this
  gfx::Rect contents_bounds = frame_->GetContentsBounds();
  canvas->FillRectInt(SK_ColorGRAY, contents_bounds.x(), contents_bounds.y(),
                      contents_bounds.width(), contents_bounds.height());
}

void AeroGlassNonClientView::Layout() {
  LayoutDistributorLogo();
  LayoutClientView();
}

void AeroGlassNonClientView::GetPreferredSize(CSize* out) {
  DCHECK(out);
  frame_->client_view()->GetPreferredSize(out);
  out->cx += 2 * kWindowHorizontalBorderSize;
  out->cy += CalculateNonClientTopHeight() + kWindowVerticalBorderSize;
}

void AeroGlassNonClientView::DidChangeBounds(const CRect& previous,
                                          const CRect& current) {
  Layout();
}

void AeroGlassNonClientView::ViewHierarchyChanged(bool is_add,
                                               ChromeViews::View* parent,
                                               ChromeViews::View* child) {
  if (is_add && child == this) {
    DCHECK(GetViewContainer());
    DCHECK(frame_->client_view()->GetParent() != this);
    AddChildView(frame_->client_view());
  }
}

///////////////////////////////////////////////////////////////////////////////
// AeroGlassNonClientView, private:

int AeroGlassNonClientView::CalculateNonClientTopHeight() const {
  if (frame_->window_delegate()->ShouldShowWindowTitle()) {
    return kTitleTopOffset + resources_->GetTitleFont().height() +
        kTitleBottomSpacing;
  }
  return kNoTitleTopSpacing;
}

void AeroGlassNonClientView::PaintDistributorLogo(ChromeCanvas* canvas) {
  // The distributor logo is only painted when the frame is not maximized.
  if (!frame_->IsMaximized() && !frame_->IsMinimized()) {
    canvas->DrawBitmapInt(distributor_logo_, logo_bounds_.x(),
                          logo_bounds_.y());
  }
}

void AeroGlassNonClientView::PaintToolbarBackground(ChromeCanvas* canvas) {
  if (frame_->IsToolbarVisible() || frame_->IsTabStripVisible()) {
    SkBitmap* toolbar_left =
        resources_->GetPartBitmap(FRAME_CLIENT_EDGE_TOP_LEFT);
    SkBitmap* toolbar_center =
        resources_->GetPartBitmap(FRAME_CLIENT_EDGE_TOP);
    SkBitmap* toolbar_right =
        resources_->GetPartBitmap(FRAME_CLIENT_EDGE_TOP_RIGHT);

    gfx::Rect toolbar_bounds = frame_->GetToolbarBounds();
    gfx::Point topleft(toolbar_bounds.x(), toolbar_bounds.y());
    View::ConvertPointToView(frame_->client_view(), this, &topleft);
    toolbar_bounds.set_x(topleft.x());
    toolbar_bounds.set_y(topleft.y());

    canvas->DrawBitmapInt(*toolbar_left,
                          toolbar_bounds.x() - toolbar_left->width(),
                          toolbar_bounds.y());
    canvas->TileImageInt(*toolbar_center,
                         toolbar_bounds.x(), toolbar_bounds.y(),
                         toolbar_bounds.width(), toolbar_center->height());
    canvas->DrawBitmapInt(*toolbar_right, toolbar_bounds.right(),
                          toolbar_bounds.y());
  }
}

void AeroGlassNonClientView::PaintClientEdge(ChromeCanvas* canvas) {
  SkBitmap* right = resources_->GetPartBitmap(FRAME_CLIENT_EDGE_RIGHT);
  SkBitmap* bottom_right =
      resources_->GetPartBitmap(FRAME_CLIENT_EDGE_BOTTOM_RIGHT);
  SkBitmap* bottom = resources_->GetPartBitmap(FRAME_CLIENT_EDGE_BOTTOM);
  SkBitmap* bottom_left =
      resources_->GetPartBitmap(FRAME_CLIENT_EDGE_BOTTOM_LEFT);
  SkBitmap* left = resources_->GetPartBitmap(FRAME_CLIENT_EDGE_LEFT);

  // The toolbar renders its own client edge in PaintToolbarBackground, however
  // there are other bands that need to have a client edge rendered along their
  // sides, such as the Bookmark bar, infobars, etc.
  gfx::Rect toolbar_bounds = frame_->GetToolbarBounds();
  gfx::Rect client_area_bounds = frame_->GetContentsBounds();
  client_area_bounds.SetRect(
      client_area_bounds.x(),
      frame_->client_view()->GetY() + toolbar_bounds.bottom() - 1,
      client_area_bounds.width(),
      std::max(0, GetHeight() - frame_->client_view()->GetY() -
          toolbar_bounds.bottom() + 1 - kWindowVerticalBorderSize));

  canvas->TileImageInt(*right, client_area_bounds.right(),
                       client_area_bounds.y(),
                       right->width(), client_area_bounds.height());
  canvas->DrawBitmapInt(*bottom_right, client_area_bounds.right(),
                        client_area_bounds.bottom());
  canvas->TileImageInt(*bottom, client_area_bounds.x(),
                       client_area_bounds.bottom(),
                       client_area_bounds.width(), bottom_right->height());
  canvas->DrawBitmapInt(*bottom_left,
                        client_area_bounds.x() - bottom_left->width(),
                        client_area_bounds.bottom());
  canvas->TileImageInt(*left, client_area_bounds.x() - left->width(),
                       client_area_bounds.y(),
                       left->width(), client_area_bounds.height());
}

void AeroGlassNonClientView::LayoutDistributorLogo() {
  int logo_w = distributor_logo_.width();
  int logo_h = distributor_logo_.height();
  
  int w = GetWidth();
  int mbx = frame_->GetMinimizeButtonOffset();

  logo_bounds_.SetRect(
      GetWidth() - frame_->GetMinimizeButtonOffset() - logo_w,
      kDistributorLogoVerticalOffset, logo_w, logo_h);
}

void AeroGlassNonClientView::LayoutClientView() {
  gfx::Rect client_bounds(
      CalculateClientAreaBounds(GetWidth(), GetHeight()));
  frame_->client_view()->SetBounds(client_bounds.ToRECT());
}

// static
void AeroGlassNonClientView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    resources_ = new AeroGlassWindowResources;
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    distributor_logo_ = *rb.GetBitmapNamed(IDR_DISTRIBUTOR_LOGO);
    initialized = true;
  }
}
