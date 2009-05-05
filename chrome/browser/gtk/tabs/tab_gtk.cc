// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/tabs/tab_gtk.h"

#include "app/resource_bundle.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/common/gfx/path.h"
#include "chrome/common/l10n_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

static const SkScalar kTabCapWidth = 15;
static const SkScalar kTabTopCurveWidth = 4;
static const SkScalar kTabBottomCurveWidth = 3;

class TabGtk::ContextMenuController : public MenuGtk::Delegate {
 public:
  explicit ContextMenuController(TabGtk* tab)
      : tab_(tab) {
    menu_.reset(new MenuGtk(this, false));
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandNewTab,
        l10n_util::GetStringUTF8(IDS_TAB_CXMENU_NEWTAB));
    menu_->AppendSeparator();
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandReload,
        l10n_util::GetStringUTF8(IDS_TAB_CXMENU_RELOAD));
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandDuplicate,
        l10n_util::GetStringUTF8(IDS_TAB_CXMENU_DUPLICATE));
    menu_->AppendSeparator();
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandCloseTab,
        l10n_util::GetStringUTF8(IDS_TAB_CXMENU_CLOSETAB));
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandCloseOtherTabs,
        l10n_util::GetStringUTF8(IDS_TAB_CXMENU_CLOSEOTHERTABS));
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandCloseTabsToRight,
        l10n_util::GetStringUTF8(IDS_TAB_CXMENU_CLOSETABSTORIGHT));
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandCloseTabsOpenedBy,
        l10n_util::GetStringUTF8(IDS_TAB_CXMENU_CLOSETABSOPENEDBY));
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandRestoreTab,
        l10n_util::GetStringUTF8(IDS_RESTORE_TAB));
  }

  virtual ~ContextMenuController() {}

  void RunMenu() {
    menu_->PopupAsContext(gtk_get_current_event_time());
  }

  void Cancel() {
    tab_ = NULL;
    menu_->Cancel();
  }

 private:

  // MenuGtk::Delegate implementation:
  virtual bool IsCommandEnabled(int command_id) const {
    if (!tab_)
      return false;

    return tab_->delegate()->IsCommandEnabledForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id), tab_);
  }

  virtual void ExecuteCommand(int command_id) {
    if (!tab_)
      return;

    tab_->delegate()->ExecuteCommandForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id), tab_);
  }

  // The context menu.
  scoped_ptr<MenuGtk> menu_;

  // The Tab the context menu was brought up for. Set to NULL when the menu
  // is canceled.
  TabGtk* tab_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuController);
};

///////////////////////////////////////////////////////////////////////////////
// TabGtk, public:

TabGtk::TabGtk(TabDelegate* delegate)
    : TabRendererGtk(),
      delegate_(delegate),
      closing_(false) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SkBitmap* bitmap = rb.GetBitmapNamed(IDR_TAB_CLOSE);

  close_button_.reset(new TabButtonGtk(this));
  close_button_.get()->SetImage(TabButtonGtk::BS_NORMAL, bitmap);
  close_button_.get()->SetImage(TabButtonGtk::BS_HOT,
                                rb.GetBitmapNamed(IDR_TAB_CLOSE_H));
  close_button_.get()->SetImage(TabButtonGtk::BS_PUSHED,
                                rb.GetBitmapNamed(IDR_TAB_CLOSE_P));
  close_button_.get()->set_bounds(
      gfx::Rect(0, 0, bitmap->width(), bitmap->height()));
}

TabGtk::~TabGtk() {
  if (menu_controller_.get()) {
    // The menu is showing. Close the menu.
    menu_controller_->Cancel();

    // Invoke this so that we hide the highlight.
    ContextMenuClosed();
  }
}

bool TabGtk::IsPointInBounds(const gfx::Point& point) {
  GdkRegion* region = MakeRegionForTab();
  bool in_bounds = (gdk_region_point_in(region, point.x(), point.y()) == TRUE);
  gdk_region_destroy(region);
  return in_bounds;
}

bool TabGtk::OnMotionNotify(GdkEventMotion* event) {
  gfx::Point point(event->x, event->y);
  bool paint = false;

  if (!(event->state & GDK_BUTTON1_MASK))
    paint = set_hovering(IsPointInBounds(point));

  paint |= close_button_.get()->OnMotionNotify(event);
  return paint;
}

bool TabGtk::OnMousePress(const gfx::Point& point) {
  if (close_button_.get()->IsPointInBounds(point))
    return close_button_.get()->OnMousePress();

  return false;
}

void TabGtk::OnMouseRelease(GdkEventButton* event) {
  close_button_.get()->OnMouseRelease();

  if (event->button == 2) {
    delegate_->CloseTab(this);
  } else if (event->button == 3) {
    ShowContextMenu();
  }
}

bool TabGtk::OnLeaveNotify() {
  bool paint = set_hovering(false);
  paint |= close_button_.get()->OnLeaveNotify();
  return paint;
}

///////////////////////////////////////////////////////////////////////////////
// TabGtk, TabRendererGtk overrides:

bool TabGtk::IsSelected() const {
  return delegate_->IsTabSelected(this);
}

void TabGtk::CloseButtonResized(const gfx::Rect& bounds) {
  close_button_.get()->set_bounds(bounds);
}

void TabGtk::Paint(ChromeCanvasPaint* canvas) {
  TabRendererGtk::Paint(canvas);
  close_button_.get()->Paint(canvas);
}

////////////////////////////////////////////////////////////////////////////////
// TabGtk, TabButtonGtk::Delegate implementation:

GdkRegion* TabGtk::MakeRegionForButton(const TabButtonGtk* button) const {
  // Use the close button bounds for hit-testing.
  return NULL;
}

void TabGtk::OnButtonActivate(const TabButtonGtk* button) {
  delegate_->CloseTab(this);
}

///////////////////////////////////////////////////////////////////////////////
// TabGtk, private:

GdkRegion* TabGtk::MakeRegionForTab()const {
  int w = width();
  int h = height();
  static const int kNumRegionPoints = 9;

  GdkPoint polygon[kNumRegionPoints] = {
    { 0, h },
    { kTabBottomCurveWidth, h - kTabBottomCurveWidth },
    { kTabCapWidth - kTabTopCurveWidth, kTabTopCurveWidth },
    { kTabCapWidth, 0 },
    { w - kTabCapWidth, 0 },
    { w - kTabCapWidth - kTabTopCurveWidth, kTabTopCurveWidth },
    { w - kTabBottomCurveWidth, h - kTabBottomCurveWidth },
    { w, h },
    { 0, h },
  };

  GdkRegion* region = gdk_region_polygon(polygon, kNumRegionPoints,
                                         GDK_WINDING_RULE);
  gdk_region_offset(region, x(), y());
  return region;
}

void TabGtk::ShowContextMenu() {
  if (!menu_controller_.get())
    menu_controller_.reset(new ContextMenuController(this));

  menu_controller_->RunMenu();
}

void TabGtk::ContextMenuClosed() {
  delegate()->StopAllHighlighting();
  menu_controller_.reset();
}
