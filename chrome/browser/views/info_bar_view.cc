// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/info_bar_view.h"

#include "base/logging.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/tab_contents_delegate.h"
#include "chrome/browser/web_contents.h"
#include "chrome/browser/web_contents_view.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/background.h"

#include "generated_resources.h"

// Color for the separator.
static const SkColor kSeparatorColor = SkColorSetRGB(165, 165, 165);

// Default background color for the info bar.
static const SkColor kBackgroundColorTop = SkColorSetRGB(255, 242, 183);
static const SkColor kBackgroundColorBottom = SkColorSetRGB(250, 230, 145);

static const SkColor kBorderColorTop = SkColorSetRGB(240, 230, 170);
static const SkColor kBorderColorBottom = SkColorSetRGB(236, 216, 133);

// Height of the separator.
static const int kSeparatorHeight = 1;

InfoBarView::InfoBarView(WebContents* web_contents)
    : web_contents_(web_contents) {
  Init();
  NotificationService::current()->AddObserver(
      this, NOTIFY_NAV_ENTRY_COMMITTED,
      Source<NavigationController>(web_contents_->controller()));
}

InfoBarView::~InfoBarView() {
  NotificationService::current()->RemoveObserver(
      this, NOTIFY_NAV_ENTRY_COMMITTED,
      Source<NavigationController>(web_contents_->controller()));
}

void InfoBarView::AppendInfoBarItem(ChromeViews::View* view, bool auto_expire) {
  // AddChildView adds an entry to expire_map_ for view.
  AddChildView(view);
  if (auto_expire)
    expire_map_[view] = GetActiveID();
  else
    expire_map_.erase(expire_map_.find(view));
}

// Preferred size is equal to the max of the childrens horizontal sizes
// and the sum of their vertical sizes.
gfx::Size InfoBarView::GetPreferredSize() {
  gfx::Size prefsize;

  // We count backwards so the most recently added view is on the top.
  for (int i = GetChildViewCount() - 1; i >= 0; i--) {
    View* v = GetChildViewAt(i);
    if (v->IsVisible()) {
      prefsize.set_width(std::max(prefsize.width(), v->width()));
      prefsize.Enlarge(0, v->GetPreferredSize().height() + kSeparatorHeight);
    }
  }
  
  return prefsize;
}

void InfoBarView::Layout() {
  int x = 0;
  int y = height();

  // We lay the bars out from bottom to top.
  for (int i = 0; i < GetChildViewCount(); ++i) {
    View* v = GetChildViewAt(i);
    if (!v->IsVisible())
      continue;

    gfx::Size view_size = v->GetPreferredSize();
    int view_width = std::max(view_size.width(), width());
    y = y - view_size.height() - kSeparatorHeight;
    v->SetBounds(x, y, view_width, view_size.height());
  }
}

void InfoBarView::Paint(ChromeCanvas* canvas) {
  PaintBackground(canvas);
  PaintBorder(canvas);
  PaintSeparators(canvas);
}

void InfoBarView::DidChangeBounds(const CRect& previous,
                                  const CRect& current) {
  Layout();
}

void InfoBarView::ChildAnimationProgressed() {
  if (web_contents_)
    web_contents_->ToolbarSizeChanged(true);
}

void InfoBarView::ChildAnimationEnded() {
  if (web_contents_)
    web_contents_->ToolbarSizeChanged(false);
}

void InfoBarView::ViewHierarchyChanged(bool is_add, View *parent,
                                       View *child) {
  if (parent == this && child->GetParent() == this) {
    // ViewHierarchyChanged is actually called before a child is removed.
    // So set child to not be visible so it isn't included in the layout.
    if (!is_add) {
      child->SetVisible(false);

      // If there's an entry in the expire map, nuke it.
      std::map<View*,int>::iterator i = expire_map_.find(child);
      if (i != expire_map_.end())
        expire_map_.erase(i);
    } else {
      expire_map_[child] = GetActiveID();
    }

    // TODO(brettw) clean up the ownership of this info bar. It should be owned
    // by the web contents view instead. In the meantime, we assume we're owned
    // by a WebContents.
    if (web_contents_->AsWebContents()->view()->IsInfoBarVisible()) {
      web_contents_->ToolbarSizeChanged(false);
    } else {
      web_contents_->view()->SetInfoBarVisible(true);
    }
  }
}

void InfoBarView::Init() {
  SetBackground(
      ChromeViews::Background::CreateVerticalGradientBackground(
          kBackgroundColorTop, kBackgroundColorBottom));
}

int InfoBarView::GetActiveID() const {
  if (!web_contents_)
    return 0;

  // The WebContents is guaranteed to have a controller.
  const NavigationEntry* const entry =
      web_contents_->controller()->GetActiveEntry();
  return entry ? entry->unique_id() : 0;
}

void InfoBarView::PaintBorder(ChromeCanvas* canvas) {
  canvas->FillRectInt(kBorderColorTop, 0, 0, width(), 1);
  canvas->FillRectInt(kBorderColorBottom,
                      0, height() - kSeparatorHeight - 1,
                      width(), kSeparatorHeight);

  if (GetChildViewCount() > 0)
    canvas->FillRectInt(kSeparatorColor, 0, height() - kSeparatorHeight,
                        width(), kSeparatorHeight);
}

void InfoBarView::PaintSeparators(ChromeCanvas* canvas) {
  ChromeViews::View* last_view = NULL;
  for (int i = GetChildViewCount() - 1; i >= 0; i--) {
    ChromeViews::View* view = GetChildViewAt(i);
    if (last_view != NULL) {
      if (view->IsVisible()) {
        PaintSeparator(canvas, last_view, view);
      } else {
        // We aren't interested in views we can't see.
        continue;
      }
    }
    last_view = view;
  }
}

void InfoBarView::PaintSeparator(ChromeCanvas* canvas,
                                 ChromeViews::View* v1,
                                 ChromeViews::View* v2) {
  canvas->FillRectInt(kSeparatorColor,
                      0,
                      v2->y() - kSeparatorHeight,
                      width(),
                      kSeparatorHeight);
}

void InfoBarView::Observe(NotificationType type,
                          const NotificationSource& source,
                          const NotificationDetails& in_details) {
  // We should get only commit notifications from our controller.
  DCHECK(type == NOTIFY_NAV_ENTRY_COMMITTED);
  DCHECK(web_contents_->controller() ==
         Source<NavigationController>(source).ptr());

  NavigationController::LoadCommittedDetails& details =
      *(Details<NavigationController::LoadCommittedDetails>(in_details).ptr());

  // Only hide infobars when the user has done something that makes the main
  // frame load. We don't want various automatic or subframe navigations making
  // it disappear.
  if (!details.is_user_initiated_main_frame_load())
    return;

  // Determine the views to remove first.
  std::vector<ChromeViews::View*> to_remove;
  for (std::map<View*,int>::iterator i = expire_map_.begin();
       i != expire_map_.end(); ++i) {
    if (PageTransition::StripQualifier(details.entry->transition_type()) ==
            PageTransition::RELOAD ||
        i->second != details.entry->unique_id())
      to_remove.push_back(i->first);
  }

  if (to_remove.empty())
    return;

  // Remove the views.
  for (std::vector<ChromeViews::View*>::iterator i = to_remove.begin();
       i != to_remove.end(); ++i) {
    // RemoveChildView takes care of removing from expire_map for us.
    RemoveChildView(*i);

    // We own the child and we're removing it, need to delete it.
    delete *i;
  }

  if (GetChildViewCount() == 0) {
    // All our views have been removed, no need to stay visible.
    web_contents_->view()->SetInfoBarVisible(false);
  } else if (web_contents_) {
    // This triggers a layout.
    web_contents_->ToolbarSizeChanged(false);
  }
}
