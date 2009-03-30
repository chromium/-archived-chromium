// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/tabs/tab_strip_gtk.h"

#include "base/gfx/gtk_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

const int kNewTabButtonHOffset = -5;
const int kNewTabButtonVOffset = 5;

// The horizontal offset from one tab to the next,
// which results in overlapping tabs.
const int kTabHOffset = -16;

inline int Round(double x) {
  return static_cast<int>(floor(x + 0.5));
}

// widget->allocation is not guaranteed to be set.  After window creation,
// we pick up the normal bounds by connecting to the configure-event signal.
gfx::Rect GetInitialWidgetBounds(GtkWidget* widget) {
  GtkRequisition request;
  gtk_widget_size_request(widget, &request);
  return gfx::Rect(0, 0, request.width, request.height);
}

}  // namespace

NineBox* TabStripGtk::background_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// TabStripGtk, public:

TabStripGtk::TabStripGtk(TabStripModel* model)
    : current_unselected_width_(TabGtk::GetStandardSize().width()),
      current_selected_width_(TabGtk::GetStandardSize().width()),
      available_width_for_tabs_(-1),
      resize_layout_scheduled_(false),
      model_(model) {
}

TabStripGtk::~TabStripGtk() {
  model_->RemoveObserver(this);
  tabstrip_.Destroy();
}

void TabStripGtk::Init() {
  model_->AddObserver(this);

  InitBackgroundNineBox();

  tabstrip_.Own(gtk_drawing_area_new());
  gtk_widget_set_size_request(tabstrip_.get(), -1,
                              TabGtk::GetMinimumUnselectedSize().height());
  gtk_widget_set_app_paintable(tabstrip_.get(), TRUE);
  g_signal_connect(G_OBJECT(tabstrip_.get()), "expose-event",
                   G_CALLBACK(OnExpose), this);
  g_signal_connect(G_OBJECT(tabstrip_.get()), "configure-event",
                   G_CALLBACK(OnConfigure), this);
  gtk_widget_show_all(tabstrip_.get());

  bounds_ = GetInitialWidgetBounds(tabstrip_.get());
}

void TabStripGtk::AddTabStripToBox(GtkWidget* box) {
  gtk_box_pack_start(GTK_BOX(box), tabstrip_.get(), FALSE, FALSE, 0);
}

void TabStripGtk::Layout() {
  // Called from:
  // - window resize
  GenerateIdealBounds();
  int tab_count = GetTabCount();
  for (int i = 0; i < tab_count; ++i) {
    const gfx::Rect& bounds = tab_data_.at(i).ideal_bounds;
    GetTabAt(i)->SetBounds(bounds);
  }

  gtk_widget_queue_draw(tabstrip_.get());
}

////////////////////////////////////////////////////////////////////////////////
// TabStripGtk, TabStripModelObserver implementation:

void TabStripGtk::TabInsertedAt(TabContents* contents,
                                int index,
                                bool foreground) {
  DCHECK(contents);
  DCHECK(index == TabStripModel::kNoTab || model_->ContainsIndex(index));

  TabGtk* tab = new TabGtk(this);

  // Only insert if we're not already in the list.
  if (index == TabStripModel::kNoTab) {
    TabData d = { tab, gfx::Rect() };
    tab_data_.push_back(d);
    tab->UpdateData(contents);
  } else {
    TabData d = { tab, gfx::Rect() };
    tab_data_.insert(tab_data_.begin() + index, d);
    tab->UpdateData(contents);
  }

  Layout();
}

void TabStripGtk::TabDetachedAt(TabContents* contents, int index) {
  GetTabAt(index)->set_closing(true);
  // TODO(jhawkins): Remove erase call when animations are hooked up.
  tab_data_.erase(tab_data_.begin() + index);
  GenerateIdealBounds();
  // TODO(jhawkins): Remove layout call when animations are hooked up.
  Layout();
}

void TabStripGtk::TabSelectedAt(TabContents* old_contents,
                                TabContents* new_contents,
                                int index,
                                bool user_gesture) {
  DCHECK(index >= 0 && index < static_cast<int>(GetTabCount()));

  // We have "tiny tabs" if the tabs are so tiny that the unselected ones are
  // a different size to the selected ones.
  bool tiny_tabs = current_unselected_width_ != current_selected_width_;
  if (!resize_layout_scheduled_ || tiny_tabs) {
    Layout();
  } else {
    gtk_widget_queue_draw(tabstrip_.get());
  }
}

void TabStripGtk::TabMoved(TabContents* contents,
                           int from_index,
                           int to_index) {
  TabGtk* tab = GetTabAt(from_index);
  tab_data_.erase(tab_data_.begin() + from_index);
  TabData data = {tab, gfx::Rect()};
  tab_data_.insert(tab_data_.begin() + to_index, data);
  GenerateIdealBounds();
  // TODO(jhawkins): Remove layout call when animations are hooked up.
  Layout();
}

void TabStripGtk::TabChangedAt(TabContents* contents, int index) {
  // Index is in terms of the model. Need to make sure we adjust that index in
  // case we have an animation going.
  TabGtk* tab = GetTabAt(index);
  tab->UpdateData(contents);
  tab->UpdateFromModel();
  gtk_widget_queue_draw(tabstrip_.get());
}

///////////////////////////////////////////////////////////////////////////////
// TabStripGtk, TabGtk::Delegate implementation:

bool TabStripGtk::IsTabSelected(const TabGtk* tab) const {
  if (tab->closing())
    return false;

  int tab_count = GetTabCount();
  for (int i = 0, index = 0; i < tab_count; ++i, ++index) {
    TabGtk* current_tab = GetTabAt(i);
    if (current_tab->closing())
      --index;
    if (current_tab == tab)
      return index == model_->selected_index();
  }
  return false;
}

void TabStripGtk::GetCurrentTabWidths(double* unselected_width,
                                      double* selected_width) const {
  *unselected_width = current_unselected_width_;
  *selected_width = current_selected_width_;
}

void TabStripGtk::SelectTab(TabGtk* tab) {
  int index = GetIndexOfTab(tab);
  if (model_->ContainsIndex(index))
    model_->SelectTabContentsAt(index, true);
}

void TabStripGtk::CloseTab(TabGtk* tab) {
  int tab_index = GetIndexOfTab(tab);
  if (model_->ContainsIndex(tab_index)) {
    TabGtk* last_tab = GetTabAt(GetTabCount() - 1);
    // Limit the width available to the TabStrip for laying out Tabs, so that
    // Tabs are not resized until a later time (when the mouse pointer leaves
    // the TabStrip).
    available_width_for_tabs_ = GetAvailableWidthForTabs(last_tab);
    resize_layout_scheduled_ = true;
    model_->CloseTabContentsAt(tab_index);
  }
}

bool TabStripGtk::IsCommandEnabledForTab(
    TabStripModel::ContextMenuCommand command_id, const TabGtk* tab) const {
  int index = GetIndexOfTab(tab);
  if (model_->ContainsIndex(index))
    return model_->IsContextMenuCommandEnabled(index, command_id);
  return false;
}

void TabStripGtk::ExecuteCommandForTab(
    TabStripModel::ContextMenuCommand command_id, TabGtk* tab) {
  int index = GetIndexOfTab(tab);
  if (model_->ContainsIndex(index))
    model_->ExecuteContextMenuCommand(index, command_id);
}

void TabStripGtk::StartHighlightTabsForCommand(
    TabStripModel::ContextMenuCommand command_id, TabGtk* tab) {
  if (command_id == TabStripModel::CommandCloseTabsOpenedBy) {
    int index = GetIndexOfTab(tab);
    if (model_->ContainsIndex(index)) {
      std::vector<int> indices = model_->GetIndexesOpenedBy(index);
      std::vector<int>::const_iterator iter = indices.begin();
      for (; iter != indices.end(); ++iter) {
        int current_index = *iter;
        DCHECK(current_index >= 0 && current_index < GetTabCount());
      }
    }
  }
}

void TabStripGtk::StopHighlightTabsForCommand(
    TabStripModel::ContextMenuCommand command_id, TabGtk* tab) {
  if (command_id == TabStripModel::CommandCloseTabsOpenedBy ||
      command_id == TabStripModel::CommandCloseTabsToRight ||
      command_id == TabStripModel::CommandCloseOtherTabs) {
    // Just tell all Tabs to stop pulsing - it's safe.
    StopAllHighlighting();
  }
}

void TabStripGtk::StopAllHighlighting() {
  // TODO(jhawkins): Hook up animations.
}

bool TabStripGtk::EndDrag(bool canceled) {
  // TODO(jhawkins): Tab dragging.
  return true;
}

bool TabStripGtk::HasAvailableDragActions() const {
  return model_->delegate()->GetDragActions() != 0;
}

////////////////////////////////////////////////////////////////////////////////
// TabStripGtk, private:

int TabStripGtk::GetTabCount() const {
  return static_cast<int>(tab_data_.size());
}

int TabStripGtk::GetAvailableWidthForTabs(TabGtk* last_tab) const {
  return last_tab->x() + last_tab->width();
}

int TabStripGtk::GetIndexOfTab(const TabGtk* tab) const {
  for (int i = 0, index = 0; i < GetTabCount(); ++i, ++index) {
    TabGtk* current_tab = GetTabAt(i);
    if (current_tab->closing()) {
      --index;
    } else if (current_tab == tab) {
      return index;
    }
  }
  return -1;
}

TabGtk* TabStripGtk::GetTabAt(int index) const {
  DCHECK(index >= 0 && index < GetTabCount());
  return tab_data_.at(index).tab;
}

void TabStripGtk::RemoveTabAt(int index) {
  tab_data_.erase(tab_data_.begin() + index);
  Layout();
}

void TabStripGtk::GenerateIdealBounds() {
  int tab_count = GetTabCount();
  double unselected, selected;
  GetDesiredTabWidths(tab_count, &unselected, &selected);

  current_unselected_width_ = unselected;
  current_selected_width_ = selected;

  // NOTE: This currently assumes a tab's height doesn't differ based on
  // selected state or the number of tabs in the strip!
  int tab_height = TabGtk::GetStandardSize().height();
  double tab_x = 0;
  for (int i = 0; i < tab_count; ++i) {
    TabGtk* tab = GetTabAt(i);
    double tab_width = unselected;
    if (tab->IsSelected())
      tab_width = selected;
    double end_of_tab = tab_x + tab_width;
    int rounded_tab_x = Round(tab_x);
    gfx::Rect state(rounded_tab_x, 0, Round(end_of_tab) - rounded_tab_x,
                    tab_height);
    tab_data_.at(i).ideal_bounds = state;
    tab_x = end_of_tab + kTabHOffset;
  }
}

void TabStripGtk::GetDesiredTabWidths(int tab_count,
                                      double* unselected_width,
                                      double* selected_width) const {
  const double min_unselected_width =
      TabGtk::GetMinimumUnselectedSize().width();
  const double min_selected_width =
      TabGtk::GetMinimumSelectedSize().width();

  if (tab_count == 0) {
    // Return immediately to avoid divide-by-zero below.
    *unselected_width = min_unselected_width;
    *selected_width = min_selected_width;
    return;
  }

  // Determine how much space we can actually allocate to tabs.
  int available_width = tabstrip_.get()->allocation.width;
  // TODO(jhawkins): Implement new tab button.

  // Calculate the desired tab widths by dividing the available space into equal
  // portions.  Don't let tabs get larger than the "standard width" or smaller
  // than the minimum width for each type, respectively.
  const int total_offset = kTabHOffset * (tab_count - 1);
  const double desired_tab_width = std::min(
      (static_cast<double>(available_width - total_offset) /
       static_cast<double>(tab_count)),
      static_cast<double>(TabGtk::GetStandardSize().width()));
  *unselected_width = std::max(desired_tab_width, min_unselected_width);
  *selected_width = std::max(desired_tab_width, min_selected_width);

  // When there are multiple tabs, we'll have one selected and some unselected
  // tabs.  If the desired width was between the minimum sizes of these types,
  // try to shrink the tabs with the smaller minimum.  For example, if we have
  // a strip of width 10 with 4 tabs, the desired width per tab will be 2.5.  If
  // selected tabs have a minimum width of 4 and unselected tabs have a minimum
  // width of 1, the above code would set *unselected_width = 2.5,
  // *selected_width = 4, which results in a total width of 11.5.  Instead, we
  // want to set *unselected_width = 2, *selected_width = 4, for a total width
  // of 10.
  if (tab_count > 1) {
    if ((min_unselected_width < min_selected_width) &&
        (desired_tab_width < min_selected_width)) {
      double calc_width =
          static_cast<double>(
              available_width - total_offset - min_selected_width) /
          static_cast<double>(tab_count - 1);
      *unselected_width = std::max(calc_width, min_unselected_width);
    } else if ((min_unselected_width > min_selected_width) &&
               (desired_tab_width < min_unselected_width)) {
      *selected_width = std::max(available_width - total_offset -
          (min_unselected_width * (tab_count - 1)), min_selected_width);
    }
  }
}

// static
gboolean TabStripGtk::OnExpose(GtkWidget* widget, GdkEventExpose* e,
                               TabStripGtk* tabstrip) {
  background_->RenderToWidget(widget);

  // Paint the tabs in reverse order, so they stack to the left.
  TabGtk* selected_tab = NULL;
  int tab_count = tabstrip->GetTabCount();
  for (int i = tab_count - 1; i >= 0; --i) {
    TabGtk* tab = tabstrip->GetTabAt(i);
    // We must ask the _Tab's_ model, not ourselves, because in some situations
    // the model will be different to this object, e.g. when a Tab is being
    // removed after its TabContents has been destroyed.
    if (!tab->IsSelected()) {
      tab->Paint(widget);
    } else {
      selected_tab = tab;
    }
  }

  // Paint the selected tab last, so it overlaps all the others.
  if (selected_tab)
    selected_tab->Paint(widget);

  return TRUE;
}

// static
gboolean TabStripGtk::OnConfigure(GtkWidget* widget, GdkEventConfigure* event,
                                  TabStripGtk* tabstrip) {
  gfx::Rect bounds = gfx::Rect(event->x, event->y, event->width, event->height);
  tabstrip->SetBounds(bounds);
  tabstrip->Layout();
  return TRUE;
}

// static
void TabStripGtk::InitBackgroundNineBox() {
  if (background_)
    return;

  ResourceBundle &rb = ResourceBundle::GetSharedInstance();

  GdkPixbuf* images[9] = {0};
  images[0] = rb.LoadPixbuf(IDR_WINDOW_TOP_CENTER);
  images[1] = rb.LoadPixbuf(IDR_WINDOW_TOP_CENTER);
  images[2] = rb.LoadPixbuf(IDR_WINDOW_TOP_CENTER);
  background_ = new NineBox(images);
}
