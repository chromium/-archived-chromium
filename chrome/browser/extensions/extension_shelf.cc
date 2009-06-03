// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_shelf.h"

#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
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
    if (y_delta > GetVerticalDragThreshold()) {
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
    shelf_->DropExtension(event.location(), canceled);
  }
}

////////////////////////////////////////////////

ExtensionShelf::ExtensionShelf(Browser* browser)
    : browser_(browser),
      handle_(NULL),
      handle_visible_(false),
      current_handle_view_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(timer_factory_(this)),
      drag_placeholder_view_(NULL) {
  // Watch extensions loaded and unloaded notifications.
  registrar_.Add(this, NotificationType::EXTENSIONS_LOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 NotificationService::AllSources());

  // Add any already-loaded extensions now, since we missed the notification for
  // those.
  ExtensionsService* service = browser_->profile()->GetExtensionsService();
  if (service) {  // This can be null in unit tests.
    if (AddExtensionViews(service->extensions())) {
      Layout();
      SchedulePaint();
    }
  }
}

BrowserBubble* ExtensionShelf::GetHandle() {
  if (!handle_.get() && HasExtensionViews() && current_handle_view_) {
    ExtensionShelfHandle* handle_view = new ExtensionShelfHandle(this);
    handle_view->SetExtensionView(current_handle_view_);
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
  if (HasExtensionViews())
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

  int count = GetChildViewCount();
  for (int i = 0; i < count; ++i) {
    x += kToolstripPadding;  // left padding
    views::View* child = GetChildViewAt(i);
    gfx::Size pref = child->GetPreferredSize();
    int next_x = x + pref.width() + kToolstripPadding;  // right padding
    child->SetVisible(next_x < max_x);
    child->SetBounds(x, y, pref.width(), content_height);
    child->Layout();
    x = next_x + kToolstripDividerWidth;
  }
  if (handle_.get())
    LayoutShelfHandle();
  SchedulePaint();
}

void ExtensionShelf::OnMouseEntered(const views::MouseEvent& event) {
  int count = GetChildViewCount();
  for (int i = 0; i < count; ++i) {
    ExtensionView* child = static_cast<ExtensionView*>(GetChildViewAt(i));
    if (event.x() > (child->x() + child->width() + kToolstripPadding))
      continue;
    current_handle_view_ = child;
    ShowShelfHandle();
    break;
  }
}

void ExtensionShelf::OnMouseExited(const views::MouseEvent& event) {
  HideShelfHandle(kHideDelayMs);
}

void ExtensionShelf::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSIONS_LOADED: {
      const ExtensionList* extensions = Details<ExtensionList>(details).ptr();
      AddExtensionViews(extensions);
      break;
    }
    case NotificationType::EXTENSION_UNLOADED: {
      Extension* extension = Details<Extension>(details).ptr();
      RemoveExtensionViews(extension);
      break;
    }
    default:
      DCHECK(false) << "Unhandled notification of type: " << type.value;
      break;
  }
}

bool ExtensionShelf::AddExtensionViews(const ExtensionList* extensions) {
  bool had_views = HasExtensionViews();
  bool added_toolstrip = false;
  ExtensionProcessManager* manager =
      browser_->profile()->GetExtensionProcessManager();
  for (ExtensionList::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    for (std::vector<std::string>::const_iterator toolstrip_path =
         (*extension)->toolstrips().begin();
         toolstrip_path != (*extension)->toolstrips().end(); ++toolstrip_path) {
      ExtensionView* toolstrip =
          manager->CreateView(*extension,
                              (*extension)->GetResourceURL(*toolstrip_path),
                              browser_);
      if (!background_.empty())
        toolstrip->SetBackground(background_);
      AddChildView(toolstrip);
      toolstrip->SetContainer(this);
      added_toolstrip = true;
    }
  }
  if (added_toolstrip) {
    SchedulePaint();
    if (!had_views)
      PreferredSizeChanged();
  }
  return added_toolstrip;
}

bool ExtensionShelf::RemoveExtensionViews(Extension* extension) {
  if (!HasExtensionViews())
    return false;

  bool removed_toolstrip = false;
  int count = GetChildViewCount();
  for (int i = count - 1; i >= 0; --i) {
    ExtensionView* view = static_cast<ExtensionView*>(GetChildViewAt(i));
    if (view->host()->extension()->id() == extension->id()) {
      RemoveChildView(view);
      delete view;
      removed_toolstrip = true;
    }
  }

  if (removed_toolstrip) {
    SchedulePaint();
    PreferredSizeChanged();
  }
  return removed_toolstrip;
}

bool ExtensionShelf::HasExtensionViews() {
  return GetChildViewCount() > 0;
}

void ExtensionShelf::OnExtensionMouseEvent(ExtensionView* view) {
  // Ignore these events when dragging.
  if (drag_placeholder_view_)
    return;
  if (view != current_handle_view_) {
    current_handle_view_ = view;
  }
  ShowShelfHandle();
}

void ExtensionShelf::OnExtensionMouseLeave(ExtensionView* view) {
  // Ignore these events when dragging.
  if (drag_placeholder_view_)
    return;
  if (view == current_handle_view_) {
    HideShelfHandle(kHideDelayMs);
  }
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
  int index = GetChildIndex(current_handle_view_);
  drag_placeholder_view_ = new View();
  drag_placeholder_view_->SetBounds(current_handle_view_->bounds());
  AddChildView(index, drag_placeholder_view_);

  // Now move the view into the handle's widget.
  ExtensionShelfHandle* handle_view =
      static_cast<ExtensionShelfHandle*>(GetHandle()->view());
  handle_view->AddChildView(current_handle_view_);
  handle_view->SizeToPreferredSize();
  handle_->ResizeToView();
  handle_view->Layout();
  handle_->DetachFromBrowser();
  SchedulePaint();
}

void ExtensionShelf::DropExtension(const gfx::Point& pt, bool cancel) {
  handle_->AttachToBrowser();

  // Replace the placeholder view with the original.
  int index = GetChildIndex(drag_placeholder_view_);
  AddChildView(index, current_handle_view_);
  current_handle_view_->SetBounds(drag_placeholder_view_->bounds());
  RemoveChildView(drag_placeholder_view_);
  delete drag_placeholder_view_;
  drag_placeholder_view_ = NULL;

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
  int count = GetChildViewCount();
  for (int i = 0; i < count; ++i)
    static_cast<ExtensionView*>(GetChildViewAt(i))->SetBackground(background_);
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
    current_handle_view_ = NULL;
  }
}

void ExtensionShelf::LayoutShelfHandle() {
  if (current_handle_view_) {
    GetHandle();  // ensure that the handle exists since we delete on hide
    ExtensionShelfHandle* handle_view =
        static_cast<ExtensionShelfHandle*>(GetHandle()->view());
    handle_view->SetExtensionView(current_handle_view_);
    int width = std::max(current_handle_view_->width(), handle_view->width());
    gfx::Point origin(-kToolstripPadding,
                      -(handle_view->height() + kToolstripPadding - 1));
    views::View::ConvertPointToWidget(current_handle_view_, &origin);
    handle_view->SetBounds(0, 0, width, handle_view->height());
    handle_->SetBounds(origin.x(), origin.y(),
                       width, handle_view->height());
  }
}
