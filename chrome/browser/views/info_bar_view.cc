// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/info_bar_view.h"

#include "base/logging.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/tab_contents_delegate.h"
#include "chrome/browser/web_contents.h"
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
}

InfoBarView::~InfoBarView() {}

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
void InfoBarView::GetPreferredSize(CSize *out) {
  out->cx = 0;
  out->cy = 0;

  // We count backwards so the most recently added view is on the top.
  for (int i = GetChildViewCount() - 1; i >= 0; i--) {
    View* v = GetChildViewAt(i);
    if (v->IsVisible()) {
      CSize view_size;
      v->GetPreferredSize(&view_size);
      out->cx = std::max(static_cast<int>(out->cx), v->GetWidth());
      out->cy += static_cast<int>(view_size.cy) + kSeparatorHeight;
    }
  }
}

void InfoBarView::Layout() {
  int width = GetWidth();
  int height = GetHeight();

  int x = 0;
  int y = height;

  // We lay the bars out from bottom to top.
  for (int i = 0; i < GetChildViewCount(); ++i) {
    View* v = GetChildViewAt(i);
    if (!v->IsVisible())
      continue;

    CSize view_size;
    v->GetPreferredSize(&view_size);
    int view_width = std::max(static_cast<int>(view_size.cx), width);
    y = y - view_size.cy - kSeparatorHeight;
    v->SetBounds(x,
                 y,
                 view_width,
                 view_size.cy);
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

void InfoBarView::DidNavigate(NavigationEntry* entry) {
  // Determine the views to remove first.
  std::vector<ChromeViews::View*> to_remove;
  NavigationEntry* pending_entry =
      web_contents_->controller()->GetPendingEntry();
  const int active_id = pending_entry ? pending_entry->unique_id() :
                                        entry->unique_id();
  for (std::map<View*,int>::iterator i = expire_map_.begin();
       i != expire_map_.end(); ++i) {
    if ((pending_entry && 
        pending_entry->transition_type() == PageTransition::RELOAD) ||
        i->second != active_id)
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
    web_contents_->SetInfoBarVisible(false);
  } else if (web_contents_) {
    // This triggers a layout.
    web_contents_->ToolbarSizeChanged(false);
  }
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

    if (web_contents_->IsInfoBarVisible()) {
      web_contents_->ToolbarSizeChanged(false);
    } else {
      web_contents_->SetInfoBarVisible(true);
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
  canvas->FillRectInt(kBorderColorTop, 0, 0, GetWidth(), 1);
  canvas->FillRectInt(kBorderColorBottom,
                      0, GetHeight() - kSeparatorHeight - 1,
                      GetWidth(), kSeparatorHeight);

  if (GetChildViewCount() > 0)
    canvas->FillRectInt(kSeparatorColor, 0, GetHeight() - kSeparatorHeight,
                        GetWidth(), kSeparatorHeight);
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
                      v2->GetY() - kSeparatorHeight,
                      GetWidth(),
                      kSeparatorHeight);
}
