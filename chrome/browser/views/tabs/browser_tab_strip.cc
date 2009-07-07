// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/views/tabs/browser_tab_strip.h"

#include "base/compiler_specific.h"
#include "chrome/browser/tab_contents/tab_contents.h"

namespace {

////////////////////////////////////////////////////////////////////////////////
// RemovingTabModel
//  After a tab is removed from the TabStrip2's model, the model can no longer
//  provide information about the tab needed to update the display, however the
//  TabStrip2 continues to display the tab until it animates out of existence.
//  During this period, this model serves as a dummy. It is created and assigned
//  to the tab when the tab is removed from the model and owned by the Tab2.

class RemovingTabModel : public Tab2Model {
 public:
  explicit RemovingTabModel(TabContents* source)
      : title_(source->GetTitle()) {
  }

  // Overridden from Tab2Model:
  virtual string16 GetTitle(Tab2* tab) const { return title_; }
  virtual bool IsSelected(Tab2* tab) const { return false; }
  virtual void SelectTab(Tab2* tab) { NOTREACHED(); }
  virtual void CaptureDragInfo(Tab2* tab,
                               const views::MouseEvent& drag_event) {
    NOTREACHED();
  }
  virtual bool DragTab(Tab2* tab, const views::MouseEvent& drag_event) {
    NOTREACHED();
    return false;
  }
  virtual void DragEnded(Tab2* tab) {
    NOTREACHED();
  }
  virtual views::AnimatorDelegate* AsAnimatorDelegate() {
    NOTREACHED();
    return NULL;
  }

 private:
  string16 title_;

  DISALLOW_COPY_AND_ASSIGN(RemovingTabModel);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStrip, public:

BrowserTabStrip::BrowserTabStrip(TabStripModel* model)
    : ALLOW_THIS_IN_INITIALIZER_LIST(TabStrip2(this)),
      model_(model) {
  model_->AddObserver(this);
}

BrowserTabStrip::~BrowserTabStrip() {
  model_->RemoveObserver(this);
}

TabContents* BrowserTabStrip::DetachTab(int index) {
  TabContents* contents = model_->GetTabContentsAt(index);
  model_->DetachTabContentsAt(index);
  return contents;
}

void BrowserTabStrip::AttachTab(TabContents* contents,
                                const gfx::Point& screen_point,
                                const gfx::Rect& tab_screen_bounds) {
  gfx::Point tabstrip_point(screen_point);

  gfx::Point screen_origin;
  View::ConvertPointToScreen(this, &screen_origin);
  tabstrip_point.Offset(-screen_origin.x(), -screen_origin.y());

  int index = GetInsertionIndexForPoint(tabstrip_point);
  model_->InsertTabContentsAt(index, contents, true, false);

  gfx::Point origin(tab_screen_bounds.origin());
  View::ConvertPointToView(NULL, this, &origin);
  ResumeDraggingTab(index, gfx::Rect(origin, tab_screen_bounds.size()));
  // TODO(beng): post task to continue dragging now.
}

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStrip, TabStripModelObserver overrides:

void BrowserTabStrip::TabInsertedAt(TabContents* contents,
                                    int index,
                                    bool foreground) {
  AddTabAt(index);
}

void BrowserTabStrip::TabDetachedAt(TabContents* contents, int index) {
  RemoveTabAt(index, new RemovingTabModel(contents));
}

void BrowserTabStrip::TabSelectedAt(TabContents* old_contents,
                                    TabContents* contents,
                                    int index,
                                    bool user_gesture) {
  TabStrip2::SelectTabAt(index);
}

void BrowserTabStrip::TabMoved(TabContents* contents,
                               int from_index,
                               int to_index) {
  TabStrip2::MoveTabAt(from_index, to_index);
}

void BrowserTabStrip::TabChangedAt(TabContents* contents, int index) {
  // TODO
}

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStrip, TabStrip2Model overrides:

string16 BrowserTabStrip::GetTitle(int index) const {
  return model_->GetTabContentsAt(index)->GetTitle();
}

bool BrowserTabStrip::IsSelected(int index) const {
  return model_->selected_index() == index;
}

void BrowserTabStrip::SelectTabAt(int index) {
  model_->SelectTabContentsAt(index, true);
}

bool BrowserTabStrip::CanDragTabs() const {
  return model_->delegate()->GetDragActions() != 0;
}

void BrowserTabStrip::MoveTabAt(int index, int to_index) {
  model_->MoveTabContentsAt(index, to_index, true);
}

void BrowserTabStrip::DetachTabAt(int index, const gfx::Rect& window_bounds,
                                  const gfx::Rect& tab_bounds) {
  TabContents* contents = DetachTab(index);
  model_->delegate()->ContinueDraggingDetachedTab(contents, window_bounds,
                                                  tab_bounds);
}
