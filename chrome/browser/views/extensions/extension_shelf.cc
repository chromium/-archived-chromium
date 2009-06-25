// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/extensions/extension_shelf.h"

#include <algorithm>

#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/extensions/extension_view.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "skia/ext/skia_utils.h"
#include "views/controls/label.h"
#include "views/screen.h"
#include "views/widget/root_view.h"

namespace {

// Margins around the content.
static const int kTopMargin = 2;
static const int kBottomMargin = 2;
static const int kLeftMargin = 0;
static const int kRightMargin = 0;

// Padding on left and right side of an extension toolstrip.
static const int kToolstripPadding = 2;

// Width of the toolstrip divider.
static const int kToolstripDividerWidth = 2;

// Preferred height of the ExtensionShelf.
static const int kShelfHeight = 29;

// Colors for the ExtensionShelf.
static const SkColor kBackgroundColor = SkColorSetRGB(230, 237, 244);
static const SkColor kBorderColor = SkColorSetRGB(201, 212, 225);
static const SkColor kDividerHighlightColor = SkColorSetRGB(247, 250, 253);

// Text colors for the handle
static const SkColor kHandleTextColor = SkColorSetRGB(6, 45, 117);
static const SkColor kHandleTextHighlightColor =
    SkColorSetARGB(200, 255, 255, 255);

// Handle padding
static const int kHandlePadding = 4;

// TODO(erikkay) convert back to a gradient when Glen figures out the
// specs.
// static const SkColor kBackgroundColor = SkColorSetRGB(237, 244, 252);
// static const SkColor kTopGradientColor = SkColorSetRGB(222, 234, 248);

// Delays for showing and hiding the shelf handle.
static const int kHideDelayMs = 500;

}  // namespace


// A small handle that is used for dragging or otherwise interacting with an
// extension toolstrip.
class ExtensionShelfHandle : public views::View {
 public:
  explicit ExtensionShelfHandle(ExtensionShelf* shelf);

  // The ExtensionView that the handle is attached to.
  void SetExtensionView(ExtensionView* v);

  // View
  virtual void Paint(gfx::Canvas* canvas);
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void OnMouseEntered(const views::MouseEvent& event);
  virtual void OnMouseExited(const views::MouseEvent& event);
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual bool OnMouseDragged(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
  virtual bool IsFocusable() const { return true; }

 private:
  ExtensionShelf* shelf_;
  ExtensionView* extension_view_;
  scoped_ptr<views::Label> title_;
  bool dragging_;
  gfx::Point initial_drag_location_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionShelfHandle);
};

ExtensionShelfHandle::ExtensionShelfHandle(ExtensionShelf* shelf)
    : shelf_(shelf), extension_view_(NULL), dragging_(false) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  // |title_| isn't actually put in the view hierarchy.  We just use it
  // to draw in place.  The reason for this is so that we can properly handle
  // the various mouse events necessary for hovering and dragging.
  title_.reset(new views::Label(L"", rb.GetFont(ResourceBundle::BaseFont)));
  title_->SetColor(kHandleTextColor);
  title_->SetDrawHighlighted(true);
  title_->SetHighlightColor(kHandleTextHighlightColor);
  title_->SetBounds(kHandlePadding, kHandlePadding, 100, 100);
  title_->SizeToPreferredSize();
}

void ExtensionShelfHandle::SetExtensionView(ExtensionView* v) {
  DCHECK(v->extension());
  extension_view_ = v;
  if (!extension_view_->extension())
    return;
  title_->SetText(UTF8ToWide(extension_view_->extension()->name()));
  title_->SizeToPreferredSize();
  SizeToPreferredSize();
}

void ExtensionShelfHandle::Paint(gfx::Canvas* canvas) {
  canvas->FillRectInt(kBackgroundColor, 0, 0, width(), height());
  canvas->FillRectInt(kBorderColor, 0, 0, width(), 1);
  canvas->FillRectInt(kBorderColor, 0, 0, 1, height() - 1);
  canvas->FillRectInt(kBorderColor, width() - 1, 0, 1, height() - 1);
  int ext_width = extension_view_->width() + kToolstripPadding +
      kToolstripDividerWidth;
  if (ext_width < width()) {
    canvas->FillRectInt(kBorderColor, ext_width, height() - 1,
                        width() - ext_width, 1);
  }

  // Draw the title using a Label as a stamp.
  // See constructor for comment about this.
  title_->ProcessPaint(canvas);

  if (dragging_) {
    // when we're dragging, draw the bottom border.
    canvas->FillRectInt(kBorderColor, 0, height() - 1, width(), 1);
  }
}

gfx::Size ExtensionShelfHandle::GetPreferredSize() {
  gfx::Size sz = title_->GetPreferredSize();
  if (extension_view_) {
    int width = std::max(extension_view_->width() + 2, sz.width());
    sz.set_width(width);
  }
  sz.Enlarge(kHandlePadding * 2, kHandlePadding * 2);
  if (dragging_) {
    gfx::Size extension_size = extension_view_->GetPreferredSize();
    sz.Enlarge(0, extension_size.height() + 2);
  }
  return sz;
}

void ExtensionShelfHandle::Layout() {
  if (dragging_) {
    int y = title_->bounds().bottom() + kHandlePadding + 1;
    extension_view_->SetBounds(1,
                               y,
                               extension_view_->width(),
                               extension_view_->height());
  }
}

void ExtensionShelfHandle::OnMouseEntered(const views::MouseEvent& event) {
  DCHECK(extension_view_);
  shelf_->OnExtensionMouseEvent(extension_view_);
}

void ExtensionShelfHandle::OnMouseExited(const views::MouseEvent& event) {
  DCHECK(extension_view_);
  shelf_->OnExtensionMouseLeave(extension_view_);
}

bool ExtensionShelfHandle::OnMousePressed(const views::MouseEvent& event) {
  initial_drag_location_ = event.location();
  return true;
}

bool ExtensionShelfHandle::OnMouseDragged(const views::MouseEvent& event) {
  if (!dragging_) {
    int y_delta = abs(initial_drag_location_.y() - event.location().y());
    int x_delta = abs(initial_drag_location_.x() - event.location().x());
    if (y_delta > GetVerticalDragThreshold() ||
        x_delta > GetHorizontalDragThreshold()) {
      dragging_ = true;
      shelf_->DragExtension();
    }
  } else {
    // When freely dragging a window, you can really only trust the
    // actual screen point.  Coordinate conversions, just don't work.
    gfx::Point screen = views::Screen::GetCursorScreenPoint();

    // However, the handle is actually a child of the browser window
    // so we need to convert it back to local coordinates.
    gfx::Point origin(0, 0);
    views::View::ConvertPointToScreen(shelf_->GetRootView(), &origin);
    screen.set_x(screen.x() - origin.x() - initial_drag_location_.x());
    screen.set_y(screen.y() - origin.y() - initial_drag_location_.y());
    shelf_->DragHandleTo(screen);
  }
  return true;
}

void ExtensionShelfHandle::OnMouseReleased(const views::MouseEvent& event,
                                           bool canceled) {
  if (dragging_) {
    views::View::OnMouseReleased(event, canceled);
    dragging_ = false;
    // |this| and |shelf_| are in different view hierarchies, so we need to
    // convert to screen coordinates and back again to map locations.
    gfx::Point loc = event.location();
    View::ConvertPointToScreen(this, &loc);
    View::ConvertPointToView(NULL, shelf_, &loc);
    shelf_->DropExtension(loc, canceled);
  }
}

////////////////////////////////////////////////

ExtensionShelf::ExtensionShelf(Browser* browser)
    : handle_(NULL),
      handle_visible_(false),
      current_toolstrip_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(timer_factory_(this)),
      drag_placeholder_view_(NULL),
      model_(new ExtensionShelfModel(browser)) {
  model_->AddObserver(this);
  LoadFromModel();
  EnableCanvasFlippingForRTLUI(true);
}

ExtensionShelf::~ExtensionShelf() {
  model_->RemoveObserver(this);
}

BrowserBubble* ExtensionShelf::GetHandle() {
  if (!handle_.get() && current_toolstrip_) {
    ExtensionShelfHandle* handle_view = new ExtensionShelfHandle(this);
    handle_view->SetExtensionView(current_toolstrip_->view());
    handle_.reset(new BrowserBubble(handle_view, GetWidget(),
                                    gfx::Point(0, 0)));
    handle_->set_delegate(this);
  }
  return handle_.get();
}

void ExtensionShelf::Paint(gfx::Canvas* canvas) {
#if 0
  // TODO(erikkay) re-enable this when Glen has the gradient values worked out.
  SkPaint paint;
  paint.setShader(skia::CreateGradientShader(0,
                                             height(),
                                             kTopGradientColor,
                                             kBackgroundColor))->safeUnref();
  canvas->FillRectInt(0, 0, width(), height(), paint);
#else
  canvas->FillRectInt(kBackgroundColor, 0, 0, width(), height());
#endif

  canvas->FillRectInt(kBorderColor, 0, 0, width(), 1);
  canvas->FillRectInt(kBorderColor, 0, height() - 1, width(), 1);

  int count = GetChildViewCount();
  for (int i = 0; i < count; ++i) {
    int right = GetChildViewAt(i)->bounds().right() + kToolstripPadding;
    int h = height() - 2;
    canvas->FillRectInt(kBorderColor, right, 1, 1, h);
    canvas->FillRectInt(kDividerHighlightColor, right + 1, 1, 1, h);
  }

  SkRect background_rect = {
      SkIntToScalar(0),
      SkIntToScalar(1),
      SkIntToScalar(1),
      SkIntToScalar(height() - 2)};
  InitBackground(canvas, background_rect);
}

gfx::Size ExtensionShelf::GetPreferredSize() {
  if (model_->count())
    return gfx::Size(0, kShelfHeight);
  return gfx::Size(0, 0);
}

void ExtensionShelf::ChildPreferredSizeChanged(View* child) {
  Layout();
}

void ExtensionShelf::Layout() {
  if (!GetParent())
    return;

  int x = kLeftMargin;
  int y = kTopMargin;
  int content_height = height() - kTopMargin - kBottomMargin;
  int max_x = width() - kRightMargin;

  int count = model_->count();
  for (int i = 0; i < count; ++i) {
    x += kToolstripPadding;  // left padding
    ExtensionHost* toolstrip = model_->ToolstripAt(i);
    ExtensionView* extension_view = toolstrip->view();
    gfx::Size pref = extension_view->GetPreferredSize();
    int next_x = x + pref.width() + kToolstripPadding;  // right padding
    extension_view->set_is_clipped(next_x >= max_x);
    extension_view->SetBounds(x, y, pref.width(), content_height);
    extension_view->Layout();
    x = next_x + kToolstripDividerWidth;
  }
  if (handle_.get())
    LayoutShelfHandle();
  SchedulePaint();
}

void ExtensionShelf::OnMouseEntered(const views::MouseEvent& event) {
  ExtensionHost* toolstrip = ToolstripAtX(event.x());
  if (toolstrip) {
    current_toolstrip_ = toolstrip;
    ShowShelfHandle();
  }
}

void ExtensionShelf::OnMouseExited(const views::MouseEvent& event) {
  HideShelfHandle(kHideDelayMs);
}

void ExtensionShelf::ToolstripInsertedAt(ExtensionHost* toolstrip,
                                         int index) {
  bool had_views = GetChildViewCount() > 0;
  ExtensionView* view = toolstrip->view();
  if (!background_.empty())
    view->SetBackground(background_);
  AddChildView(view);
  view->SetContainer(this);
  if (!had_views)
    PreferredSizeChanged();
  Layout();
}

void ExtensionShelf::ToolstripRemovingAt(ExtensionHost* toolstrip,
                                         int index) {
  ExtensionView* view = toolstrip->view();
  RemoveChildView(view);
  Layout();
}

void ExtensionShelf::ToolstripDraggingFrom(ExtensionHost* toolstrip,
                                           int index) {
}

void ExtensionShelf::ToolstripMoved(ExtensionHost* toolstrip,
                                    int from_index,
                                    int to_index) {
  Layout();
}

void ExtensionShelf::ToolstripChangedAt(ExtensionHost* toolstrip,
                                        int index) {
}

void ExtensionShelf::ExtensionShelfEmpty() {
  PreferredSizeChanged();
}

void ExtensionShelf::ShelfModelReloaded() {
  // None of the child views are parent owned, so nothing is being leaked here.
  RemoveAllChildViews(false);
  LoadFromModel();
}

void ExtensionShelf::OnExtensionMouseEvent(ExtensionView* view) {
  // Ignore these events when dragging.
  if (drag_placeholder_view_)
    return;
  ExtensionHost *toolstrip = ToolstripForView(view);
  if (toolstrip != current_toolstrip_)
    current_toolstrip_ = toolstrip;
  if (current_toolstrip_)
    ShowShelfHandle();
}

void ExtensionShelf::OnExtensionMouseLeave(ExtensionView* view) {
  // Ignore these events when dragging.
  if (drag_placeholder_view_)
    return;
  ExtensionHost *toolstrip = ToolstripForView(view);
  if (toolstrip == current_toolstrip_)
    HideShelfHandle(kHideDelayMs);
}

void ExtensionShelf::BubbleBrowserWindowMoved(BrowserBubble* bubble) {
  HideShelfHandle(0);
}

void ExtensionShelf::BubbleBrowserWindowClosed(BrowserBubble* bubble) {
  // We'll be going away shortly, so no need to do any other teardown here.
  HideShelfHandle(0);
}

void ExtensionShelf::DragExtension() {
  // Construct a placeholder view to replace the view.
  // TODO(erikkay) the placeholder should draw a dimmed version of the
  // extension view
  drag_placeholder_view_ = new View();
  drag_placeholder_view_->SetBounds(current_toolstrip_->view()->bounds());
  AddChildView(drag_placeholder_view_);

  // Now move the view into the handle's widget.
  ExtensionShelfHandle* handle_view =
      static_cast<ExtensionShelfHandle*>(GetHandle()->view());
  handle_view->AddChildView(current_toolstrip_->view());
  handle_view->SizeToPreferredSize();
  handle_->ResizeToView();
  handle_view->Layout();
  handle_->DetachFromBrowser();
  SchedulePaint();
}

void ExtensionShelf::DropExtension(const gfx::Point& pt, bool cancel) {
  handle_->AttachToBrowser();

  // Replace the placeholder view with the original.
  AddChildView(current_toolstrip_->view());
  current_toolstrip_->view()->SetBounds(drag_placeholder_view_->bounds());
  RemoveChildView(drag_placeholder_view_);
  delete drag_placeholder_view_;
  drag_placeholder_view_ = NULL;

  ExtensionHost* toolstrip = ToolstripAtX(pt.x());
  if (toolstrip) {
    int from = model_->IndexOfToolstrip(current_toolstrip_);
    int to = model_->IndexOfToolstrip(toolstrip);
    model_->MoveToolstripAt(from, to);
  }

  ExtensionShelfHandle* handle_view =
      static_cast<ExtensionShelfHandle*>(GetHandle()->view());
  handle_view->SizeToPreferredSize();
  handle_view->Layout();
  handle_->ResizeToView();
  LayoutShelfHandle();
  SchedulePaint();
}

void ExtensionShelf::DragHandleTo(const gfx::Point& pt) {
  handle_->MoveTo(pt.x(), pt.y());
  // TODO(erikkay) as this gets dragged around, update the placeholder view
  // on the shelf to show where it will get dropped to.
}

void ExtensionShelf::InitBackground(gfx::Canvas* canvas,
                                    const SkRect& subset) {
  if (!background_.empty())
    return;

  const SkBitmap& background = canvas->getDevice()->accessBitmap(false);

  // Extract the correct subset of the toolstrip background into a bitmap. We
  // must use a temporary here because extractSubset() returns a bitmap that
  // references pixels in the original one and we want to actually make a copy
  // that will have a long lifetime.
  SkBitmap temp;
  temp.setConfig(background.config(),
                 static_cast<int>(subset.width()),
                 static_cast<int>(subset.height()));

  SkRect mapped_subset = subset;
  bool result = canvas->getTotalMatrix().mapRect(&mapped_subset);
  DCHECK(result);

  SkIRect isubset;
  mapped_subset.round(&isubset);
  result = background.extractSubset(&temp, isubset);
  if (!result)
    return;

  temp.copyTo(&background_, temp.config());
  DCHECK(background_.readyToDraw());

  // Tell all extension views about the new background
  int count = model_->count();
  for (int i = 0; i < count; ++i)
    model_->ToolstripAt(i)->view()->SetBackground(background_);
}

ExtensionHost* ExtensionShelf::ToolstripAtX(int x) {
  int count = model_->count();
  if (count == 0)
    return NULL;

  if (x < 0)
    return model_->ToolstripAt(0);

  for (int i = 0; i < count; ++i) {
    ExtensionHost* toolstrip = model_->ToolstripAt(i);
    ExtensionView* view = toolstrip->view();
    if (x > (view->x() + view->width() + kToolstripPadding))
      continue;
    return toolstrip;
  }

  return model_->ToolstripAt(count - 1);
}

ExtensionHost* ExtensionShelf::ToolstripForView(ExtensionView* view) {
  int count = model_->count();
  for (int i = 0; i < count; ++i) {
    ExtensionHost* toolstrip = model_->ToolstripAt(i);
    if (view == toolstrip->view())
      return toolstrip;
  }
  NOTREACHED();
  return NULL;
}

void ExtensionShelf::ShowShelfHandle() {
  if (drag_placeholder_view_)
    return;
  if (!timer_factory_.empty())
    timer_factory_.RevokeAll();
  if (handle_visible_) {
    // The contents may have changed, even though the handle is still visible.
    LayoutShelfHandle();
    return;
  }
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      timer_factory_.NewRunnableMethod(&ExtensionShelf::DoShowShelfHandle),
      1000);
}

void ExtensionShelf::DoShowShelfHandle() {
  if (!handle_visible_) {
    handle_visible_ = true;
    LayoutShelfHandle();
    handle_->Show();
  }
}

void ExtensionShelf::HideShelfHandle(int delay_ms) {
  if (drag_placeholder_view_)
    return;
  if (!timer_factory_.empty())
    timer_factory_.RevokeAll();
  if (!handle_visible_)
    return;
  if (delay_ms) {
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        timer_factory_.NewRunnableMethod(&ExtensionShelf::DoHideShelfHandle),
        delay_ms);
  } else {
    DoHideShelfHandle();
  }
}

void ExtensionShelf::DoHideShelfHandle() {
  if (handle_visible_) {
    handle_visible_ = false;
    handle_->Hide();
    handle_->DetachFromBrowser();
    handle_.reset(NULL);
    current_toolstrip_ = NULL;
  }
}

void ExtensionShelf::LayoutShelfHandle() {
  if (current_toolstrip_) {
    GetHandle();  // ensure that the handle exists since we delete on hide
    ExtensionShelfHandle* handle_view =
        static_cast<ExtensionShelfHandle*>(GetHandle()->view());
    handle_view->SetExtensionView(current_toolstrip_->view());
    int width = std::max(current_toolstrip_->view()->width(),
                         handle_view->width());
    gfx::Point origin(-kToolstripPadding,
                      -(handle_view->height() + kToolstripPadding - 1));
    views::View::ConvertPointToWidget(current_toolstrip_->view(), &origin);
    handle_view->SetBounds(0, 0, width, handle_view->height());
    handle_->SetBounds(origin.x(), origin.y(),
                       width, handle_view->height());
  }
}

void ExtensionShelf::LoadFromModel() {
  int count = model_->count();
  for (int i = 0; i < count; ++i)
    ToolstripInsertedAt(model_->ToolstripAt(i), i);
}
