// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/tabs/tab_strip_gtk.h"

#include <algorithm>

#include "app/gfx/canvas_paint.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/slide_animation.h"
#include "base/gfx/gtk_util.h"
#include "base/gfx/point.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/dnd_registry.h"
#include "chrome/browser/gtk/tabs/dragged_tab_controller_gtk.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/gtk_util.h"
#include "grit/app_resources.h"
#include "grit/theme_resources.h"

#if defined(LINUX2)
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/views/tabs/tab_overview_types.h"
#include "chrome/common/x11_util.h"
#endif

namespace {

const int kDefaultAnimationDurationMs = 100;
const int kResizeLayoutAnimationDurationMs = 166;
const int kReorderAnimationDurationMs = 166;
const int kAnimateToBoundsDurationMs = 150;

const int kNewTabButtonHOffset = -5;
#if defined(LINUX2)
const int kNewTabButtonVOffset = 0;
#else
const int kNewTabButtonVOffset = 5;
#endif

// The delay between when the mouse leaves the tabstrip and the resize animation
// is started.
const int kResizeTabsTimeMs = 300;

// The range outside of the tabstrip where the pointer must enter/leave to
// start/stop the resize animation.
const int kTabStripAnimationVSlop = 40;

const int kHorizontalMoveThreshold = 16;  // pixels

// The horizontal offset from one tab to the next,
// which results in overlapping tabs.
const int kTabHOffset = -16;

// A linux specific menu item for toggling window decorations.
const int kShowWindowDecorationsCommand = 200;

// Size of the drop indicator.
static int drop_indicator_width;
static int drop_indicator_height;

inline int Round(double x) {
  return static_cast<int>(x + 0.5);
}

// widget->allocation is not guaranteed to be set.  After window creation,
// we pick up the normal bounds by connecting to the configure-event signal.
gfx::Rect GetInitialWidgetBounds(GtkWidget* widget) {
  GtkRequisition request;
  gtk_widget_size_request(widget, &request);
  return gfx::Rect(0, 0, request.width, request.height);
}

// Mime types for DnD. Used to synchronize across applications.
const char kTargetString[] = "STRING";
const char kTargetTextPlain[] = "text/plain";
const char kTargetTextUriList[] = "text/uri-list";

// Table of the mime types that we accept with their options.
const GtkTargetEntry kTargetTable[] = {
  { const_cast<gchar*>(kTargetString), 0, dnd::X_CHROME_STRING },
  { const_cast<gchar*>(kTargetTextPlain), 0, dnd::X_CHROME_TEXT_PLAIN },
  { const_cast<gchar*>(kTargetTextUriList), 0, dnd::X_CHROME_TEXT_URI_LIST }
};

const int kTargetTableSize = G_N_ELEMENTS(kTargetTable);

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// TabAnimation
//
//  A base class for all TabStrip animations.
//
class TabStripGtk::TabAnimation : public AnimationDelegate {
 public:
  friend class TabStripGtk;

  // Possible types of animation.
  enum Type {
    INSERT,
    REMOVE,
    MOVE,
    RESIZE,
    SNAP
  };

  TabAnimation(TabStripGtk* tabstrip, Type type)
      : tabstrip_(tabstrip),
        animation_(this),
        start_selected_width_(0),
        start_unselected_width_(0),
        end_selected_width_(0),
        end_unselected_width_(0),
        layout_on_completion_(false),
        type_(type) {
  }
  virtual ~TabAnimation() {}

  Type type() const { return type_; }

  void Start() {
    animation_.SetSlideDuration(GetDuration());
    animation_.SetTweenType(SlideAnimation::EASE_OUT);
    if (!animation_.IsShowing()) {
      animation_.Reset();
      animation_.Show();
    }
  }

  void Stop() {
    animation_.Stop();
  }

  void set_layout_on_completion(bool layout_on_completion) {
    layout_on_completion_ = layout_on_completion;
  }

  // Retrieves the width for the Tab at the specified index if an animation is
  // active.
  static double GetCurrentTabWidth(TabStripGtk* tabstrip,
                                   TabStripGtk::TabAnimation* animation,
                                   int index) {
    double unselected, selected;
    tabstrip->GetCurrentTabWidths(&unselected, &selected);
    TabGtk* tab = tabstrip->GetTabAt(index);
    double tab_width = tab->IsSelected() ? selected : unselected;

    if (animation) {
      double specified_tab_width = animation->GetWidthForTab(index);
      if (specified_tab_width != -1)
        tab_width = specified_tab_width;
    }

    return tab_width;
  }

  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const Animation* animation) {
    tabstrip_->AnimationLayout(end_unselected_width_);
  }

  virtual void AnimationEnded(const Animation* animation) {
    tabstrip_->FinishAnimation(this, layout_on_completion_);
    // This object is destroyed now, so we can't do anything else after this.
  }

  virtual void AnimationCanceled(const Animation* animation) {
    AnimationEnded(animation);
  }

 protected:
  // Returns the duration of the animation.
  virtual int GetDuration() const {
    return kDefaultAnimationDurationMs;
  }

  // Subclasses override to return the width of the Tab at the specified index
  // at the current animation frame. -1 indicates the default width should be
  // used for the Tab.
  virtual double GetWidthForTab(int index) const {
    return -1;  // Use default.
  }

  // Figure out the desired start and end widths for the specified pre- and
  // post- animation tab counts.
  void GenerateStartAndEndWidths(int start_tab_count, int end_tab_count) {
    tabstrip_->GetDesiredTabWidths(start_tab_count, &start_unselected_width_,
                                   &start_selected_width_);
    double standard_tab_width =
        static_cast<double>(TabRendererGtk::GetStandardSize().width());

    if ((end_tab_count - start_tab_count) > 0 &&
        start_unselected_width_ < standard_tab_width) {
      double minimum_tab_width = static_cast<double>(
          TabRendererGtk::GetMinimumUnselectedSize().width());
      start_unselected_width_ -= minimum_tab_width / start_tab_count;
    }

    tabstrip_->GenerateIdealBounds();
    tabstrip_->GetDesiredTabWidths(end_tab_count,
                                   &end_unselected_width_,
                                   &end_selected_width_);
  }

  TabStripGtk* tabstrip_;
  SlideAnimation animation_;

  double start_selected_width_;
  double start_unselected_width_;
  double end_selected_width_;
  double end_unselected_width_;

 private:
  // True if a complete re-layout is required upon completion of the animation.
  // Subclasses set this if they don't perform a complete layout
  // themselves and canceling the animation may leave the strip in an
  // inconsistent state.
  bool layout_on_completion_;

  const Type type_;

  DISALLOW_COPY_AND_ASSIGN(TabAnimation);
};

////////////////////////////////////////////////////////////////////////////////

// Handles insertion of a Tab at |index|.
class InsertTabAnimation : public TabStripGtk::TabAnimation {
 public:
  explicit InsertTabAnimation(TabStripGtk* tabstrip, int index)
      : TabAnimation(tabstrip, INSERT),
        index_(index) {
    int tab_count = tabstrip->GetTabCount();
    GenerateStartAndEndWidths(tab_count - 1, tab_count);
  }
  virtual ~InsertTabAnimation() {}

 protected:
  // Overridden from TabStripGtk::TabAnimation:
  virtual double GetWidthForTab(int index) const {
    if (index == index_) {
      bool is_selected = tabstrip_->model()->selected_index() == index;
      double target_width =
          is_selected ? end_unselected_width_ : end_selected_width_;
      double start_width =
          is_selected ? TabGtk::GetMinimumSelectedSize().width() :
                        TabGtk::GetMinimumUnselectedSize().width();

      double delta = target_width - start_width;
      if (delta > 0)
        return start_width + (delta * animation_.GetCurrentValue());

      return start_width;
    }

    if (tabstrip_->GetTabAt(index)->IsSelected()) {
      double delta = end_selected_width_ - start_selected_width_;
      return start_selected_width_ + (delta * animation_.GetCurrentValue());
    }

    double delta = end_unselected_width_ - start_unselected_width_;
    return start_unselected_width_ + (delta * animation_.GetCurrentValue());
  }

 private:
  int index_;

  DISALLOW_COPY_AND_ASSIGN(InsertTabAnimation);
};

////////////////////////////////////////////////////////////////////////////////

// Handles removal of a Tab from |index|
class RemoveTabAnimation : public TabStripGtk::TabAnimation {
 public:
  RemoveTabAnimation(TabStripGtk* tabstrip, int index, TabContents* contents)
      : TabAnimation(tabstrip, REMOVE),
        index_(index) {
    int tab_count = tabstrip->GetTabCount();
    GenerateStartAndEndWidths(tab_count, tab_count - 1);
  }

  virtual ~RemoveTabAnimation() {}

  // Returns the index of the tab being removed.
  int index() const { return index_; }

 protected:
  // Overridden from TabStripGtk::TabAnimation:
  virtual double GetWidthForTab(int index) const {
    TabGtk* tab = tabstrip_->GetTabAt(index);

    if (index == index_) {
      // The tab(s) being removed are gradually shrunken depending on the state
      // of the animation.
      // Removed animated Tabs are never selected.
      double start_width = start_unselected_width_;
      // Make sure target_width is at least abs(kTabHOffset), otherwise if
      // less than kTabHOffset during layout tabs get negatively offset.
      double target_width =
          std::max(abs(kTabHOffset),
                   TabGtk::GetMinimumUnselectedSize().width() + kTabHOffset);
      double delta = start_width - target_width;
      return start_width - (delta * animation_.GetCurrentValue());
    }

    if (tabstrip_->available_width_for_tabs_ != -1 &&
        index_ != tabstrip_->GetTabCount() - 1) {
      return TabStripGtk::TabAnimation::GetWidthForTab(index);
    }

    // All other tabs are sized according to the start/end widths specified at
    // the start of the animation.
    if (tab->IsSelected()) {
      double delta = end_selected_width_ - start_selected_width_;
      return start_selected_width_ + (delta * animation_.GetCurrentValue());
    }

    double delta = end_unselected_width_ - start_unselected_width_;
    return start_unselected_width_ + (delta * animation_.GetCurrentValue());
  }

  virtual void AnimationEnded(const Animation* animation) {
    tabstrip_->RemoveTabAt(index_);
    TabStripGtk::TabAnimation::AnimationEnded(animation);
  }

 private:
  int index_;

  DISALLOW_COPY_AND_ASSIGN(RemoveTabAnimation);
};

////////////////////////////////////////////////////////////////////////////////

// Handles the movement of a Tab from one position to another.
class MoveTabAnimation : public TabStripGtk::TabAnimation {
 public:
  MoveTabAnimation(TabStripGtk* tabstrip, int tab_a_index, int tab_b_index)
      : TabAnimation(tabstrip, MOVE),
        start_tab_a_bounds_(tabstrip_->GetIdealBounds(tab_b_index)),
        start_tab_b_bounds_(tabstrip_->GetIdealBounds(tab_a_index)) {
    tab_a_ = tabstrip_->GetTabAt(tab_a_index);
    tab_b_ = tabstrip_->GetTabAt(tab_b_index);

    // Since we don't do a full TabStrip re-layout, we need to force a full
    // layout upon completion since we're not guaranteed to be in a good state
    // if for example the animation is canceled.
    set_layout_on_completion(true);
  }
  virtual ~MoveTabAnimation() {}

  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const Animation* animation) {
    // Position Tab A
    double distance = start_tab_b_bounds_.x() - start_tab_a_bounds_.x();
    double delta = distance * animation_.GetCurrentValue();
    double new_x = start_tab_a_bounds_.x() + delta;
    gfx::Rect bounds(Round(new_x), tab_a_->y(), tab_a_->width(),
        tab_a_->height());
    tabstrip_->SetTabBounds(tab_a_, bounds);

    // Position Tab B
    distance = start_tab_a_bounds_.x() - start_tab_b_bounds_.x();
    delta = distance * animation_.GetCurrentValue();
    new_x = start_tab_b_bounds_.x() + delta;
    bounds = gfx::Rect(Round(new_x), tab_b_->y(), tab_b_->width(),
        tab_b_->height());
    tabstrip_->SetTabBounds(tab_b_, bounds);
  }

 protected:
  // Overridden from TabStripGtk::TabAnimation:
  virtual int GetDuration() const { return kReorderAnimationDurationMs; }

 private:
  // The two tabs being exchanged.
  TabGtk* tab_a_;
  TabGtk* tab_b_;

  // ...and their bounds.
  gfx::Rect start_tab_a_bounds_;
  gfx::Rect start_tab_b_bounds_;

  DISALLOW_COPY_AND_ASSIGN(MoveTabAnimation);
};

////////////////////////////////////////////////////////////////////////////////

// Handles the animated resize layout of the entire TabStrip from one width
// to another.
class ResizeLayoutAnimation : public TabStripGtk::TabAnimation {
 public:
  explicit ResizeLayoutAnimation(TabStripGtk* tabstrip)
      : TabAnimation(tabstrip, RESIZE) {
    int tab_count = tabstrip->GetTabCount();
    GenerateStartAndEndWidths(tab_count, tab_count);
    InitStartState();
  }
  virtual ~ResizeLayoutAnimation() {}

  // Overridden from AnimationDelegate:
  virtual void AnimationEnded(const Animation* animation) {
    tabstrip_->resize_layout_scheduled_ = false;
    TabStripGtk::TabAnimation::AnimationEnded(animation);
  }

 protected:
  // Overridden from TabStripGtk::TabAnimation:
  virtual int GetDuration() const {
    return kResizeLayoutAnimationDurationMs;
  }

  virtual double GetWidthForTab(int index) const {
    if (tabstrip_->GetTabAt(index)->IsSelected()) {
      double delta = end_selected_width_ - start_selected_width_;
      return start_selected_width_ + (delta * animation_.GetCurrentValue());
    }

    double delta = end_unselected_width_ - start_unselected_width_;
    return start_unselected_width_ + (delta * animation_.GetCurrentValue());
  }

 private:
  // We need to start from the current widths of the Tabs as they were last
  // laid out, _not_ the last known good state, which is what'll be done if we
  // don't measure the Tab sizes here and just go with the default TabAnimation
  // behavior...
  void InitStartState() {
    for (int i = 0; i < tabstrip_->GetTabCount(); ++i) {
      TabGtk* current_tab = tabstrip_->GetTabAt(i);
      if (current_tab->IsSelected()) {
        start_selected_width_ = current_tab->width();
      } else {
        start_unselected_width_ = current_tab->width();
      }
    }
  }

  DISALLOW_COPY_AND_ASSIGN(ResizeLayoutAnimation);
};

////////////////////////////////////////////////////////////////////////////////
// TabStripGtk, public:

TabStripGtk::TabStripGtk(TabStripModel* model)
    : current_unselected_width_(TabGtk::GetStandardSize().width()),
      current_selected_width_(TabGtk::GetStandardSize().width()),
      available_width_for_tabs_(-1),
      resize_layout_scheduled_(false),
      model_(model),
      resize_layout_factory_(this),
      added_as_message_loop_observer_(false) {
}

TabStripGtk::~TabStripGtk() {
  model_->RemoveObserver(this);
  tabstrip_.Destroy();

  // Free any remaining tabs.  This is needed to free the very last tab,
  // because it is not animated on close.  This also happens when all of the
  // tabs are closed at once.
  std::vector<TabData>::iterator iterator = tab_data_.begin();
  for (; iterator < tab_data_.end(); iterator++) {
    delete iterator->tab;
  }

  tab_data_.clear();

  // Make sure we unhook ourselves as a message loop observer so that we don't
  // crash in the case where the user closes the last tab in a window.
  RemoveMessageLoopObserver();
}

void TabStripGtk::Init() {
  model_->AddObserver(this);

  tabstrip_.Own(gtk_fixed_new());
  gtk_widget_set_size_request(tabstrip_.get(), -1,
                              TabGtk::GetMinimumUnselectedSize().height());
  gtk_widget_set_app_paintable(tabstrip_.get(), TRUE);
  gtk_drag_dest_set(tabstrip_.get(), GTK_DEST_DEFAULT_ALL,
                    kTargetTable, kTargetTableSize,
                    static_cast<GdkDragAction>(
                        GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK));
  g_signal_connect(G_OBJECT(tabstrip_.get()), "expose-event",
                   G_CALLBACK(OnExpose), this);
  g_signal_connect(G_OBJECT(tabstrip_.get()), "size-allocate",
                     G_CALLBACK(OnSizeAllocate), this);
  g_signal_connect(G_OBJECT(tabstrip_.get()), "drag-motion",
                   G_CALLBACK(OnDragMotion), this);
  g_signal_connect(G_OBJECT(tabstrip_.get()), "drag-drop",
                   G_CALLBACK(OnDragDrop), this);
  g_signal_connect(G_OBJECT(tabstrip_.get()), "drag-leave",
                   G_CALLBACK(OnDragLeave), this);
  g_signal_connect(G_OBJECT(tabstrip_.get()), "drag-failed",
                   G_CALLBACK(OnDragFailed), this);
  g_signal_connect(G_OBJECT(tabstrip_.get()), "drag-data-received",
                   G_CALLBACK(OnDragDataReceived), this);

  newtab_button_.reset(MakeNewTabButton());

#if defined(LINUX2)
  tab_overview_button_.reset(MakeTabOverviewButton());
#endif

  gtk_widget_show_all(tabstrip_.get());

  bounds_ = GetInitialWidgetBounds(tabstrip_.get());

  if (drop_indicator_width == 0) {
    // Direction doesn't matter, both images are the same size.
    GdkPixbuf* drop_image = GetDropArrowImage(true);
    drop_indicator_width = gdk_pixbuf_get_width(drop_image);
    drop_indicator_height = gdk_pixbuf_get_height(drop_image);
  }
}

void TabStripGtk::Show() {
  gtk_widget_show(tabstrip_.get());
}

void TabStripGtk::Hide() {
  gtk_widget_hide(tabstrip_.get());
}

void TabStripGtk::Layout() {
  // Called from:
  // - window resize
  // - animation completion
  if (active_animation_.get())
    active_animation_->Stop();

  GenerateIdealBounds();
  int tab_count = GetTabCount();
  int tab_right = 0;
  for (int i = 0; i < tab_count; ++i) {
    const gfx::Rect& bounds = tab_data_.at(i).ideal_bounds;
    TabGtk* tab = GetTabAt(i);
    SetTabBounds(tab, bounds);
    tab_right = bounds.right() + kTabHOffset;
  }

  LayoutNewTabButton(static_cast<double>(tab_right), current_unselected_width_);
#if defined(LINUX2)
  gtk_fixed_move(GTK_FIXED(tabstrip_.get()), tab_overview_button_->widget(),
                 bounds_.width() - tab_overview_button_->width(), 0);
#endif
  gtk_widget_queue_draw(tabstrip_.get());
}

void TabStripGtk::SchedulePaint() {
  gtk_widget_queue_draw(tabstrip_.get());
}

void TabStripGtk::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
}

void TabStripGtk::UpdateLoadingAnimations() {
  for (int i = 0, index = 0; i < GetTabCount(); ++i, ++index) {
    TabGtk* current_tab = GetTabAt(i);
    if (current_tab->closing()) {
      --index;
    } else {
      TabContents* contents = model_->GetTabContentsAt(index);
      if (!contents || !contents->is_loading()) {
        current_tab->ValidateLoadingAnimation(TabGtk::ANIMATION_NONE);
      } else if (contents->waiting_for_response()) {
        current_tab->ValidateLoadingAnimation(TabGtk::ANIMATION_WAITING);
      } else {
        current_tab->ValidateLoadingAnimation(TabGtk::ANIMATION_LOADING);
      }
    }
  }

  gtk_widget_queue_draw(tabstrip_.get());
}

bool TabStripGtk::IsCompatibleWith(TabStripGtk* other) {
  return model_->profile() == other->model()->profile();
}

bool TabStripGtk::IsAnimating() const {
  return active_animation_.get() != NULL;
}

void TabStripGtk::DestroyDragController() {
  if (IsDragSessionActive())
    drag_controller_.reset(NULL);
}

void TabStripGtk::DestroyDraggedSourceTab(TabGtk* tab) {
  // We could be running an animation that references this Tab.
  if (active_animation_.get())
    active_animation_->Stop();

  // Make sure we leave the tab_data_ vector in a consistent state, otherwise
  // we'll be pointing to tabs that have been deleted and removed from the
  // child view list.
  std::vector<TabData>::iterator it = tab_data_.begin();
  for (; it != tab_data_.end(); ++it) {
    if (it->tab == tab) {
      if (!model_->closing_all())
        NOTREACHED() << "Leaving in an inconsistent state!";
      tab_data_.erase(it);
      break;
    }
  }

  gtk_container_remove(GTK_CONTAINER(tabstrip_.get()), tab->widget());
  delete tab;

  // Force a layout here, because if we've just quickly drag detached a Tab,
  // the stopping of the active animation above may have left the TabStrip in a
  // bad (visual) state.
  Layout();
}

gfx::Rect TabStripGtk::GetIdealBounds(int index) {
  DCHECK(index >= 0 && index < GetTabCount());
  return tab_data_.at(index).ideal_bounds;
}

gfx::Point TabStripGtk::GetTabStripOriginForWidget(GtkWidget* target) {
  int x, y;
  if (!gtk_widget_translate_coordinates(widget(), target,
      -widget()->allocation.x, 0, &x, &y)) {
    // If the tab strip isn't showing, give the coordinates relative to the
    // toplevel instead.
    gtk_widget_translate_coordinates(
        gtk_widget_get_toplevel(widget()), target, 0, 0, &x, &y);
  }
  if (GTK_WIDGET_NO_WINDOW(target)) {
    x += target->allocation.x;
    y += target->allocation.y;
  }
  return gfx::Point(x, y);
}

////////////////////////////////////////////////////////////////////////////////
// TabStripGtk, TabStripModelObserver implementation:

void TabStripGtk::TabInsertedAt(TabContents* contents,
                                int index,
                                bool foreground) {
  DCHECK(contents);
  DCHECK(index == TabStripModel::kNoTab || model_->ContainsIndex(index));

  if (active_animation_.get())
    active_animation_->Stop();

  bool contains_tab = false;
  TabGtk* tab = NULL;
  // First see if this Tab is one that was dragged out of this TabStrip and is
  // now being dragged back in. In this case, the DraggedTabController actually
  // has the Tab already constructed and we can just insert it into our list
  // again.
  if (IsDragSessionActive()) {
    tab = drag_controller_->GetDragSourceTabForContents(contents);
    if (tab) {
      // If the Tab was detached, it would have been animated closed but not
      // removed, so we need to reset this property.
      tab->set_closing(false);
      tab->ValidateLoadingAnimation(TabRendererGtk::ANIMATION_NONE);
      tab->SetVisible(true);
    }

    // See if we're already in the list. We don't want to add ourselves twice.
    std::vector<TabData>::const_iterator iter = tab_data_.begin();
    for (; iter != tab_data_.end() && !contains_tab; ++iter) {
      if (iter->tab == tab)
        contains_tab = true;
    }
  }

  if (!tab)
    tab = new TabGtk(this);

  // Only insert if we're not already in the list.
  if (!contains_tab) {
    if (index == TabStripModel::kNoTab) {
      TabData d = { tab, gfx::Rect() };
      tab_data_.push_back(d);
      tab->UpdateData(contents, false);
    } else {
      TabData d = { tab, gfx::Rect() };
      tab_data_.insert(tab_data_.begin() + index, d);
      tab->UpdateData(contents, false);
    }
  }

  if (gtk_widget_get_parent(tab->widget()) != tabstrip_.get())
    gtk_fixed_put(GTK_FIXED(tabstrip_.get()), tab->widget(), 0, 0);

  // Don't animate the first tab; it looks weird.
  if (GetTabCount() > 1) {
    StartInsertTabAnimation(index);
    // We added the tab at 0x0, we need to force an animation step otherwise
    // if GTK paints before the animation event the tab is painted at 0x0
    // which is most likely not where it should be positioned.
    active_animation_->AnimationProgressed(NULL);
  } else {
    Layout();
  }
}

void TabStripGtk::TabDetachedAt(TabContents* contents, int index) {
  if (CanUpdateDisplay()) {
    GenerateIdealBounds();
    StartRemoveTabAnimation(index, contents);
    // Have to do this _after_ calling StartRemoveTabAnimation, so that any
    // previous remove is completed fully and index is valid in sync with the
    // model index.
    GetTabAt(index)->set_closing(true);
  }
}

void TabStripGtk::TabSelectedAt(TabContents* old_contents,
                                TabContents* new_contents,
                                int index,
                                bool user_gesture) {
  DCHECK(index >= 0 && index < static_cast<int>(GetTabCount()));

  if (CanUpdateDisplay()) {
    // We have "tiny tabs" if the tabs are so tiny that the unselected ones are
    // a different size to the selected ones.
    bool tiny_tabs = current_unselected_width_ != current_selected_width_;
    if (!IsAnimating() && (!resize_layout_scheduled_ || tiny_tabs)) {
      Layout();
    } else {
      gtk_widget_queue_draw(tabstrip_.get());
    }
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
  StartMoveTabAnimation(from_index, to_index);
}

void TabStripGtk::TabChangedAt(TabContents* contents, int index,
                               bool loading_only) {
  // Index is in terms of the model. Need to make sure we adjust that index in
  // case we have an animation going.
  TabGtk* tab = GetTabAt(index);
  tab->UpdateData(contents, loading_only);
  tab->UpdateFromModel();
  gtk_widget_queue_draw(tabstrip_.get());
}

////////////////////////////////////////////////////////////////////////////////
// TabStripGtk, TabGtk::TabDelegate implementation:

bool TabStripGtk::IsTabSelected(const TabGtk* tab) const {
  if (tab->closing())
    return false;

  return GetIndexOfTab(tab) == model_->selected_index();
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
    // We hook into the message loop in order to receive mouse move events when
    // the mouse is outside of the tabstrip.  We unhook once the resize layout
    // animation is started.
    AddMessageLoopObserver();
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

void TabStripGtk::MaybeStartDrag(TabGtk* tab, const gfx::Point& point) {
  // Don't accidentally start any drag operations during animations if the
  // mouse is down.
  if (IsAnimating() || tab->closing() || !HasAvailableDragActions())
    return;

  drag_controller_.reset(new DraggedTabControllerGtk(tab, this));
  drag_controller_->CaptureDragInfo(point);
}

void TabStripGtk::ContinueDrag(GdkDragContext* context) {
  // We can get called even if |MaybeStartDrag| wasn't called in the event of
  // a TabStrip animation when the mouse button is down. In this case we should
  // _not_ continue the drag because it can lead to weird bugs.
  if (drag_controller_.get())
    drag_controller_->Drag();
}

bool TabStripGtk::EndDrag(bool canceled) {
  return drag_controller_.get() ? drag_controller_->EndDrag(canceled) : false;
}

bool TabStripGtk::HasAvailableDragActions() const {
  return model_->delegate()->GetDragActions() != 0;
}

///////////////////////////////////////////////////////////////////////////////
// TabStripGtk, MessageLoop::Observer implementation:

void TabStripGtk::WillProcessEvent(GdkEvent* event) {
  // Nothing to do.
}

void TabStripGtk::DidProcessEvent(GdkEvent* event) {
  switch (event->type) {
    case GDK_MOTION_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      HandleGlobalMouseMoveEvent();
      break;
    default:
      break;
  }
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
  DCHECK_GE(index, 0);
  DCHECK_LT(index, GetTabCount());
  return tab_data_.at(index).tab;
}

void TabStripGtk::RemoveTabAt(int index) {
  TabGtk* removed = tab_data_.at(index).tab;

  // Remove the Tab from the TabStrip's list.
  tab_data_.erase(tab_data_.begin() + index);

  if (!IsDragSessionActive() || !drag_controller_->IsDragSourceTab(removed)) {
    gtk_container_remove(GTK_CONTAINER(tabstrip_.get()), removed->widget());
    delete removed;
  }
}

void TabStripGtk::HandleGlobalMouseMoveEvent() {
  if (!IsCursorInTabStripZone()) {
    // Mouse moved outside the tab slop zone, start a timer to do a resize
    // layout after a short while...
    if (resize_layout_factory_.empty()) {
      MessageLoop::current()->PostDelayedTask(FROM_HERE,
          resize_layout_factory_.NewRunnableMethod(
              &TabStripGtk::ResizeLayoutTabs),
          kResizeTabsTimeMs);
    }
  } else {
    // Mouse moved quickly out of the tab strip and then into it again, so
    // cancel the timer so that the strip doesn't move when the mouse moves
    // back over it.
    resize_layout_factory_.RevokeAll();
  }
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
  double tab_x = tab_start_x();
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

void TabStripGtk::LayoutNewTabButton(double last_tab_right,
                                     double unselected_width) {
  gfx::Rect bounds(0, kNewTabButtonVOffset,
                   newtab_button_->width(), newtab_button_->height());
#if !defined(LINUX2)
  int delta = abs(Round(unselected_width) - TabGtk::GetStandardSize().width());
  if (delta > 1 && !resize_layout_scheduled_) {
    // We're shrinking tabs, so we need to anchor the New Tab button to the
    // right edge of the TabStrip's bounds, rather than the right edge of the
    // right-most Tab, otherwise it'll bounce when animating.
    bounds.set_x(bounds_.width() - newtab_button_->width());
  } else {
    bounds.set_x(Round(last_tab_right - kTabHOffset) + kNewTabButtonHOffset);
  }

  bounds.set_x(gtk_util::MirroredLeftPointForRect(tabstrip_.get(), bounds));
#endif

  gtk_fixed_move(GTK_FIXED(tabstrip_.get()), newtab_button_->widget(),
                 bounds.x(), bounds.y());
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
  int available_width = tabstrip_->allocation.width;
  if (available_width_for_tabs_ < 0) {
    available_width = bounds_.width();
    available_width -=
        (kNewTabButtonHOffset + newtab_button_->width());
#if defined(LINUX2)
    available_width -= tab_overview_button_->width();
#endif
  } else {
    // Interesting corner case: if |available_width_for_tabs_| > the result
    // of the calculation in the conditional arm above, the strip is in
    // overflow.  We can either use the specified width or the true available
    // width here; the first preserves the consistent "leave the last tab under
    // the user's mouse so they can close many tabs" behavior at the cost of
    // prolonging the glitchy appearance of the overflow state, while the second
    // gets us out of overflow as soon as possible but forces the user to move
    // their mouse for a few tabs' worth of closing.  We choose visual
    // imperfection over behavioral imperfection and select the first option.
    available_width = available_width_for_tabs_;
  }

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

int TabStripGtk::tab_start_x() const {
#if defined(LINUX2)
  return newtab_button_->width() + kNewTabButtonHOffset;
#else
  return 0;
#endif
}

void TabStripGtk::ResizeLayoutTabs() {
  resize_layout_factory_.RevokeAll();

  // It is critically important that this is unhooked here, otherwise we will
  // keep spying on messages forever.
  RemoveMessageLoopObserver();

  available_width_for_tabs_ = -1;
  double unselected, selected;
  GetDesiredTabWidths(GetTabCount(), &unselected, &selected);
  TabGtk* first_tab = GetTabAt(0);
  int w = Round(first_tab->IsSelected() ? selected : unselected);

  // We only want to run the animation if we're not already at the desired
  // size.
  if (abs(first_tab->width() - w) > 1)
    StartResizeLayoutAnimation();
}

bool TabStripGtk::IsCursorInTabStripZone() const {
  gfx::Rect bds = bounds();
  gfx::Point tabstrip_topleft(bds.origin());
  gtk_util::ConvertWidgetPointToScreen(tabstrip_.get(), &tabstrip_topleft);
  bds.set_origin(tabstrip_topleft);
  bds.set_height(bds.height() + kTabStripAnimationVSlop);

  GdkScreen* screen = gdk_screen_get_default();
  GdkDisplay* display = gdk_screen_get_display(screen);
  gint x, y;
  gdk_display_get_pointer(display, NULL, &x, &y, NULL);
  gfx::Point cursor_point(x, y);

  return bds.Contains(cursor_point);
}

void TabStripGtk::AddMessageLoopObserver() {
  if (!added_as_message_loop_observer_) {
    MessageLoopForUI::current()->AddObserver(this);
    added_as_message_loop_observer_ = true;
  }
}

void TabStripGtk::RemoveMessageLoopObserver() {
  if (added_as_message_loop_observer_) {
    MessageLoopForUI::current()->RemoveObserver(this);
    added_as_message_loop_observer_ = false;
  }
}

gfx::Rect TabStripGtk::GetDropBounds(int drop_index,
                                     bool drop_before,
                                     bool* is_beneath) {
  DCHECK_NE(drop_index, -1);
  int center_x;
  if (drop_index < GetTabCount()) {
    TabGtk* tab = GetTabAt(drop_index);
    if (drop_before)
      center_x = tab->x() - (kTabHOffset / 2);
    else
      center_x = tab->x() + (tab->width() / 2);
  } else {
    TabGtk* last_tab = GetTabAt(drop_index - 1);
    center_x = last_tab->x() + last_tab->width() + (kTabHOffset / 2);
  }

  // TODO(jhawkins): Handle RTL layout.

  // Determine the screen bounds.
  gfx::Point drop_loc(center_x - drop_indicator_width / 2,
                      -drop_indicator_height);
  gtk_util::ConvertWidgetPointToScreen(tabstrip_.get(), &drop_loc);
  gfx::Rect drop_bounds(drop_loc.x(), drop_loc.y(), drop_indicator_width,
                        drop_indicator_height);

  // TODO(jhawkins): We always display the arrow underneath the tab because we
  // don't have custom frame support yet.
  *is_beneath = true;
  if (*is_beneath)
    drop_bounds.Offset(0, drop_bounds.height() + bounds().height());

  return drop_bounds;
}

void TabStripGtk::UpdateDropIndex(GdkDragContext* context, gint x, gint y) {
  // TODO(jhawkins): Handle RTL layout.
  for (int i = 0; i < GetTabCount(); ++i) {
    TabGtk* tab = GetTabAt(i);
    const int tab_max_x = tab->x() + tab->width();
    const int hot_width = tab->width() / 3;
    if (x < tab_max_x) {
      if (x < tab->x() + hot_width)
        SetDropIndex(i, true);
      else if (x >= tab_max_x - hot_width)
        SetDropIndex(i + 1, true);
      else
        SetDropIndex(i, false);
      return;
    }
  }

  // The drop isn't over a tab, add it to the end.
  SetDropIndex(GetTabCount(), true);
}

void TabStripGtk::SetDropIndex(int index, bool drop_before) {
  if (index == -1) {
    if (drop_info_.get())
      gtk_widget_hide(drop_info_->container);
    return;
  }

  if (drop_info_.get() && !GTK_WIDGET_VISIBLE(drop_info_->container))
    gtk_widget_show(drop_info_->container);

  if (drop_info_.get() && drop_info_->drop_index == index &&
      drop_info_->drop_before == drop_before) {
    return;
  }

  bool is_beneath;
  gfx::Rect drop_bounds = GetDropBounds(index, drop_before, &is_beneath);

  if (!drop_info_.get()) {
    drop_info_.reset(new DropInfo(index, drop_before, !is_beneath));
  } else {
    drop_info_->drop_index = index;
    drop_info_->drop_before = drop_before;
    if (is_beneath == drop_info_->point_down) {
      drop_info_->point_down = !is_beneath;
      drop_info_->drop_arrow= GetDropArrowImage(drop_info_->point_down);
    }
  }

  gtk_window_move(GTK_WINDOW(drop_info_->container),
                  drop_bounds.x(), drop_bounds.y());
  gtk_window_resize(GTK_WINDOW(drop_info_->container),
                    drop_bounds.width(), drop_bounds.height());
}

void TabStripGtk::CompleteDrop(guchar* data) {
  if (!drop_info_.get())
    return;

  const int drop_index = drop_info_->drop_index;
  const bool drop_before = drop_info_->drop_before;

  // Hide the drop indicator.
  SetDropIndex(-1, false);

  GURL url(reinterpret_cast<char*>(data));
  if (!url.is_valid())
    return;

  if (drop_before) {
    // Insert a new tab.
    TabContents* contents =
        model_->delegate()->CreateTabContentsForURL(
            url, GURL(), model_->profile(), PageTransition::TYPED, false,
            NULL);
    model_->AddTabContents(contents, drop_index, false,
                           PageTransition::GENERATED, true);
  } else {
    model_->GetTabContentsAt(drop_index)->controller().LoadURL(
        url, GURL(), PageTransition::GENERATED);
    model_->SelectTabContentsAt(drop_index, true);
  }
}

// static
GdkPixbuf* TabStripGtk::GetDropArrowImage(bool is_down) {
  return ResourceBundle::GetSharedInstance().GetPixbufNamed(
      is_down ? IDR_TAB_DROP_DOWN : IDR_TAB_DROP_UP);
}

// TabStripGtk::DropInfo -------------------------------------------------------

TabStripGtk::DropInfo::DropInfo(int drop_index, bool drop_before,
                                bool point_down)
    : drop_index(drop_index),
      drop_before(drop_before),
      point_down(point_down) {
  container = gtk_window_new(GTK_WINDOW_POPUP);
  SetContainerColorMap();
  gtk_widget_set_app_paintable(container, TRUE);
  g_signal_connect(G_OBJECT(container), "expose-event",
                   G_CALLBACK(OnExposeEvent), this);
  gtk_widget_add_events(container, GDK_STRUCTURE_MASK);
  gtk_widget_show_all(container);

  drop_arrow = GetDropArrowImage(point_down);

  gtk_window_move(GTK_WINDOW(container), 0, 0);
  gtk_window_resize(GTK_WINDOW(container),
                    drop_indicator_width, drop_indicator_height);
}

TabStripGtk::DropInfo::~DropInfo() {
  gtk_widget_destroy(container);
}

// static
gboolean TabStripGtk::DropInfo::OnExposeEvent(GtkWidget* widget,
                                              GdkEventExpose* event,
                                              DropInfo* drop_info) {
  if (gtk_util::IsScreenComposited()) {
    drop_info->SetContainerTransparency();
  } else {
    drop_info->SetContainerShapeMask();
  }

  gdk_pixbuf_render_to_drawable(drop_info->drop_arrow,
                                drop_info->container->window,
                                0, 0, 0,
                                0, 0,
                                drop_indicator_width,
                                drop_indicator_height,
                                GDK_RGB_DITHER_NONE, 0, 0);

  return FALSE;
}

// Sets the color map of the container window to allow the window to be
// transparent.
void TabStripGtk::DropInfo::SetContainerColorMap() {
  GdkScreen* screen = gtk_widget_get_screen(container);
  GdkColormap* colormap = gdk_screen_get_rgba_colormap(screen);

  // If rgba is not available, use rgb instead.
  if (!colormap)
    colormap = gdk_screen_get_rgb_colormap(screen);

  gtk_widget_set_colormap(container, colormap);
}

// Sets full transparency for the container window.  This is used if
// compositing is available for the screen.
void TabStripGtk::DropInfo::SetContainerTransparency() {
  cairo_t* cairo_context = gdk_cairo_create(container->window);
  if (!cairo_context)
      return;

  // Make the background of the dragged tab window fully transparent.  All of
  // the content of the window (child widgets) will be completely opaque.

  cairo_scale(cairo_context, static_cast<double>(drop_indicator_width),
              static_cast<double>(drop_indicator_height));
  cairo_set_source_rgba(cairo_context, 1.0f, 1.0f, 1.0f, 0.0f);
  cairo_set_operator(cairo_context, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cairo_context);
  cairo_destroy(cairo_context);
}

// Sets the shape mask for the container window to emulate a transparent
// container window.  This is used if compositing is not available for the
// screen.
void TabStripGtk::DropInfo::SetContainerShapeMask() {
  // Create a 1bpp bitmap the size of |container|.
  GdkPixmap* pixmap = gdk_pixmap_new(NULL,
                                     drop_indicator_width,
                                     drop_indicator_height, 1);
  cairo_t* cairo_context = gdk_cairo_create(GDK_DRAWABLE(pixmap));

  // Set the transparency.
  cairo_set_source_rgba(cairo_context, 1, 1, 1, 0);

  // Blit the rendered bitmap into a pixmap.  Any pixel set in the pixmap will
  // be opaque in the container window.
  cairo_set_operator(cairo_context, CAIRO_OPERATOR_SOURCE);
  gdk_cairo_set_source_pixbuf(cairo_context, drop_arrow, 0, 0);
  cairo_paint(cairo_context);
  cairo_destroy(cairo_context);

  // Set the shape mask.
  gdk_window_shape_combine_mask(container->window, pixmap, 0, 0);
  g_object_unref(pixmap);
}

// Called from:
// - animation tick
void TabStripGtk::AnimationLayout(double unselected_width) {
  int tab_height = TabGtk::GetStandardSize().height();
  double tab_x = tab_start_x();
  for (int i = 0; i < GetTabCount(); ++i) {
    TabAnimation* animation = active_animation_.get();
    double tab_width = TabAnimation::GetCurrentTabWidth(this, animation, i);
    double end_of_tab = tab_x + tab_width;
    int rounded_tab_x = Round(tab_x);
    TabGtk* tab = GetTabAt(i);
    gfx::Rect bounds(rounded_tab_x, 0, Round(end_of_tab) - rounded_tab_x,
                     tab_height);
    SetTabBounds(tab, bounds);
    tab_x = end_of_tab + kTabHOffset;
  }
  LayoutNewTabButton(tab_x, unselected_width);
  gtk_widget_queue_draw(tabstrip_.get());
}

void TabStripGtk::StartInsertTabAnimation(int index) {
  // The TabStrip can now use its entire width to lay out Tabs.
  available_width_for_tabs_ = -1;
  if (active_animation_.get())
    active_animation_->Stop();
  active_animation_.reset(new InsertTabAnimation(this, index));
  active_animation_->Start();
}

void TabStripGtk::StartRemoveTabAnimation(int index, TabContents* contents) {
  if (active_animation_.get()) {
    // Some animations (e.g. MoveTabAnimation) cause there to be a Layout when
    // they're completed (which includes canceled). Since |tab_data_| is now
    // inconsistent with TabStripModel, doing this Layout will crash now, so
    // we ask the MoveTabAnimation to skip its Layout (the state will be
    // corrected by the RemoveTabAnimation we're about to initiate).
    active_animation_->set_layout_on_completion(false);
    active_animation_->Stop();
  }

  active_animation_.reset(new RemoveTabAnimation(this, index, contents));
  active_animation_->Start();
}

void TabStripGtk::StartMoveTabAnimation(int from_index, int to_index) {
  if (active_animation_.get())
    active_animation_->Stop();
  active_animation_.reset(new MoveTabAnimation(this, from_index, to_index));
  active_animation_->Start();
}

void TabStripGtk::StartResizeLayoutAnimation() {
  if (active_animation_.get())
    active_animation_->Stop();
  active_animation_.reset(new ResizeLayoutAnimation(this));
  active_animation_->Start();
}

bool TabStripGtk::CanUpdateDisplay() {
  // Don't bother laying out/painting when we're closing all tabs.
  if (model_->closing_all()) {
    // Make sure any active animation is ended, too.
    if (active_animation_.get())
      active_animation_->Stop();
    return false;
  }
  return true;
}

void TabStripGtk::FinishAnimation(TabStripGtk::TabAnimation* animation,
                                  bool layout) {
  active_animation_.reset(NULL);
  if (layout)
    Layout();
}

// static
gboolean TabStripGtk::OnExpose(GtkWidget* widget, GdkEventExpose* event,
                               TabStripGtk* tabstrip) {
  if (gdk_region_empty(event->region))
    return TRUE;

  // TODO(jhawkins): Ideally we'd like to only draw what's needed in the damage
  // rect, but the tab widgets overlap each other, and painting on one widget
  // will cause an expose-event to be sent to the widgets underneath.  The
  // underlying widget does not need to be redrawn as we control the order of
  // expose-events.  Currently we hack it to redraw the entire tabstrip.  We
  // could change the damage rect to just contain the tabs + the new tab button.
  event->area.x = 0;
  event->area.y = 0;
  event->area.width = tabstrip->bounds_.width();
  event->area.height = tabstrip->bounds_.height();
  gdk_region_union_with_rect(event->region, &event->area);

  // Paint the New Tab button.
  gtk_container_propagate_expose(GTK_CONTAINER(tabstrip->tabstrip_.get()),
      tabstrip->newtab_button_->widget(), event);

#if defined(LINUX2)
  // Paint the tab overview button.
  gtk_container_propagate_expose(GTK_CONTAINER(tabstrip->tabstrip_.get()),
      tabstrip->tab_overview_button_->widget(), event);
#endif

  // Paint the tabs in reverse order, so they stack to the left.
  TabGtk* selected_tab = NULL;
  int tab_count = tabstrip->GetTabCount();
  for (int i = tab_count - 1; i >= 0; --i) {
    TabGtk* tab = tabstrip->GetTabAt(i);
    // We must ask the _Tab's_ model, not ourselves, because in some situations
    // the model will be different to this object, e.g. when a Tab is being
    // removed after its TabContents has been destroyed.
    if (!tab->IsSelected()) {
      gtk_container_propagate_expose(GTK_CONTAINER(tabstrip->tabstrip_.get()),
                                     tab->widget(), event);
    } else {
      selected_tab = tab;
    }
  }

  // Paint the selected tab last, so it overlaps all the others.
  if (selected_tab) {
    gtk_container_propagate_expose(GTK_CONTAINER(tabstrip->tabstrip_.get()),
                                   selected_tab->widget(), event);
  }

  return TRUE;
}

// static
void TabStripGtk::OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation,
                                 TabStripGtk* tabstrip) {
  gfx::Rect bounds = gfx::Rect(allocation->x, allocation->y,
      allocation->width, allocation->height);

  // Nothing to do if the bounds are the same.  If we don't catch this, we'll
  // get an infinite loop of size-allocate signals.
  if (tabstrip->bounds_ == bounds)
    return;

  tabstrip->SetBounds(bounds);

  // No tabs, nothing to layout.  This happens when a browser window is created
  // and shown before tabs are added (as in a popup window).
  if (tabstrip->GetTabCount() == 0)
    return;

  // Do a regular layout on the first configure-event so we don't animate
  // the first tab.
  // TODO(jhawkins): Windows resizes the layout tabs continuously during
  // a resize.  I need to investigate which signal to watch in order to
  // reproduce this behavior.
  if (tabstrip->GetTabCount() == 1)
    tabstrip->Layout();
  else
    tabstrip->ResizeLayoutTabs();
}

// static
gboolean TabStripGtk::OnDragMotion(GtkWidget* widget, GdkDragContext* context,
                                   gint x, gint y, guint time,
                                   TabStripGtk* tabstrip) {
  tabstrip->UpdateDropIndex(context, x, y);
  return TRUE;
}

// static
gboolean TabStripGtk::OnDragDrop(GtkWidget* widget, GdkDragContext* context,
                                 gint x, gint y, guint time,
                                 TabStripGtk* tabstrip) {
  if (!tabstrip->drop_info_.get())
    return FALSE;

  GtkTargetList* list = gtk_target_list_new(kTargetTable, kTargetTableSize);
  DCHECK(list);

  GList* target = context->targets;
  for (; target != NULL; target = target->next) {
    guint info;
    GdkAtom target_atom = GDK_POINTER_TO_ATOM(target->data);
    if (gtk_target_list_find(list, target_atom, &info)) {
      gtk_drag_get_data(widget, context, target_atom, time);
    }
  }

  g_free(list);
  return TRUE;
}

// static
gboolean TabStripGtk::OnDragLeave(GtkWidget* widget, GdkDragContext* context,
                                  guint time, TabStripGtk* tabstrip) {
  // Hide the drop indicator.
  tabstrip->SetDropIndex(-1, false);
  return FALSE;
}

// static
gboolean TabStripGtk::OnDragFailed(GtkWidget* widget, GdkDragContext* context,
                                   GtkDragResult result,
                                   TabStripGtk* tabstrip) {
  // Hide the drop indicator.
  tabstrip->SetDropIndex(-1, false);
  return FALSE;
}

// static
gboolean TabStripGtk::OnDragDataReceived(GtkWidget* widget,
                                         GdkDragContext* context,
                                         gint x, gint y,
                                         GtkSelectionData* data,
                                         guint info, guint time,
                                         TabStripGtk* tabstrip) {
  // TODO(jhawkins): Parse URI lists.
  if (info == dnd::X_CHROME_STRING || info == dnd::X_CHROME_TEXT_PLAIN) {
    tabstrip->CompleteDrop(data->data);
    gtk_drag_finish(context, TRUE, TRUE, time);
  }

  return TRUE;
}

// static
void TabStripGtk::OnNewTabClicked(GtkWidget* widget, TabStripGtk* tabstrip) {
  tabstrip->model_->delegate()->AddBlankTab(true);
}

void TabStripGtk::SetTabBounds(TabGtk* tab, const gfx::Rect& bounds) {
  gfx::Rect bds = bounds;
  bds.set_x(gtk_util::MirroredLeftPointForRect(tabstrip_.get(), bounds));
  tab->SetBounds(bds);
  gtk_fixed_move(GTK_FIXED(tabstrip_.get()), tab->widget(),
                 bds.x(), bds.y());
}

CustomDrawButton* TabStripGtk::MakeNewTabButton() {
#if defined(LINUX2)
  CustomDrawButton* button = new CustomDrawButton(IDR_NEWTAB_BUTTON2, 0, 0, 0);
#else
  CustomDrawButton* button = new CustomDrawButton(IDR_NEWTAB_BUTTON,
      IDR_NEWTAB_BUTTON_P, IDR_NEWTAB_BUTTON_H, 0);
#endif

  g_signal_connect(G_OBJECT(button->widget()), "clicked",
                   G_CALLBACK(OnNewTabClicked), this);
  GTK_WIDGET_UNSET_FLAGS(button->widget(), GTK_CAN_FOCUS);
  gtk_fixed_put(GTK_FIXED(tabstrip_.get()), button->widget(), 0, 0);

  return button;
}

#if defined(LINUX2)
CustomDrawButton* TabStripGtk::MakeTabOverviewButton() {
  CustomDrawButton* button =
      new CustomDrawButton(IDR_TAB_OVERVIEW_BUTTON_ICON, 0, 0, 0);

  g_signal_connect(G_OBJECT(button->widget()), "clicked",
                   G_CALLBACK(OnTabOverviewButtonClicked), this);
  GTK_WIDGET_UNSET_FLAGS(button->widget(), GTK_CAN_FOCUS);
  gtk_fixed_put(GTK_FIXED(tabstrip_.get()), button->widget(), 0, 0);

  return button;
}

// static
void TabStripGtk::OnTabOverviewButtonClicked(GtkWidget* widget,
                                             TabStripGtk* tabstrip) {
  Browser* browser = BrowserList::GetLastActive();
  DCHECK(browser);  // In order for the user to click on the tab there should
                    // be an active browser.
  TabOverviewTypes::Message message;
  message.set_type(TabOverviewTypes::Message::WM_SWITCH_TO_OVERVIEW_MODE);
  GtkWidget* browser_widget = GTK_WIDGET(
      static_cast<BrowserWindowGtk*>(browser->window())->GetNativeHandle());
  message.set_param(0, x11_util::GetX11WindowFromGtkWidget(browser_widget));
  TabOverviewTypes::instance()->SendMessage(message);
}
#endif
