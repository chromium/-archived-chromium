// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/views/tabs/browser_tab_strip.h"

#include "base/compiler_specific.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/tabs/tab_strip.h"  // for CreateTabStrip only.

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
      : title_(source->GetTitle()),
        icon_(source->GetFavIcon()),
        should_show_icon_(source->ShouldDisplayFavIcon()),
        is_loading_(source->is_loading()),
        is_crashed_(source->is_crashed()),
        is_incognito_(source->profile()->IsOffTheRecord()) {
  }

  // Overridden from Tab2Model:
  virtual string16 GetTitle(Tab2* tab) const { return title_; }
  virtual SkBitmap GetIcon(Tab2* tab) const { return icon_; }
  virtual bool IsSelected(Tab2* tab) const { return false; }
  virtual bool ShouldShowIcon(Tab2* tab) const { return should_show_icon_; }
  virtual bool IsLoading(Tab2* tab) const { return is_loading_; }
  virtual bool IsCrashed(Tab2* tab) const { return is_crashed_; }
  virtual bool IsIncognito(Tab2* tab) const { return is_incognito_; }
  virtual void SelectTab(Tab2* tab) { NOTREACHED(); }
  virtual void CloseTab(Tab2* tab) { NOTREACHED(); }
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
  // Captured display state.
  string16 title_;
  SkBitmap icon_;
  bool should_show_icon_;
  bool is_loading_;
  bool is_crashed_;
  bool is_incognito_;

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

SkBitmap BrowserTabStrip::GetIcon(int index) const {
  return model_->GetTabContentsAt(index)->GetFavIcon();
}

bool BrowserTabStrip::IsSelected(int index) const {
  return model_->selected_index() == index;
}

bool BrowserTabStrip::ShouldShowIcon(int index) const {
  return model_->GetTabContentsAt(index)->ShouldDisplayFavIcon();
}

bool BrowserTabStrip::IsLoading(int index) const {
  return model_->GetTabContentsAt(index)->is_loading();
}

bool BrowserTabStrip::IsCrashed(int index) const {
  return model_->GetTabContentsAt(index)->is_crashed();
}

bool BrowserTabStrip::IsIncognito(int index) const {
  return model_->GetTabContentsAt(index)->profile()->IsOffTheRecord();
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

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStrip, TabStripWrapper implementation:

int BrowserTabStrip::GetPreferredHeight() {
  return GetPreferredSize().height();
}

bool BrowserTabStrip::IsAnimating() const {
  return false;
}

void BrowserTabStrip::SetBackgroundOffset(gfx::Point offset) {
}

bool BrowserTabStrip::PointIsWithinWindowCaption(
    const gfx::Point& point) {
  return false;
}

bool BrowserTabStrip::IsDragSessionActive() const {
  return false;
}

bool BrowserTabStrip::IsCompatibleWith(TabStripWrapper* other) const {
  return false;
}

void BrowserTabStrip::SetDraggedTabBounds(int tab_index,
                                          const gfx::Rect& tab_bounds) {
  TabStrip2::SetDraggedTabBounds(tab_index, tab_bounds);
}

void BrowserTabStrip::UpdateLoadingAnimations() {
}

views::View* BrowserTabStrip::GetView() {
  return this;
}

BrowserTabStrip* BrowserTabStrip::AsBrowserTabStrip() {
  return this;
}

TabStrip* BrowserTabStrip::AsTabStrip() {
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// TabStripWrapper, public:

// static
TabStripWrapper* TabStripWrapper::CreateTabStrip(TabStripModel* model) {
  if (TabStrip2::Enabled())
    return new BrowserTabStrip(model);
  return new TabStrip(model);
}
