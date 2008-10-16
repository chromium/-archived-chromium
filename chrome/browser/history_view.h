// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This view displays a list of historical page visits.  It requires a
// BaseHistoryModel to provide the information that will be shown.

#ifndef CHROME_BROWSER_HISTORY_VIEW_H_
#define CHROME_BROWSER_HISTORY_VIEW_H_

#include <vector>

#include "chrome/browser/history_model.h"
#include "chrome/views/link.h"
#include "chrome/views/scroll_view.h"

class PageNavigator;
class HistoryItemRenderer;
class BaseHistoryModel;
class SearchableUIContainer;

class HistoryView : public views::View,
                    public BaseHistoryModelObserver,
                    public views::LinkController,
    views::VariableRowHeightScrollHelper::Controller {
 public:
  HistoryView(SearchableUIContainer* container,
              BaseHistoryModel* model,
              PageNavigator* navigator);
  virtual ~HistoryView();

  // Returns true if this view is currently visible to the user.
  bool IsVisible();

  // Overridden for layout purposes.
  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);
  virtual void Layout();

  virtual bool GetFloatingViewIDForPoint(int x, int y, int* id);

  // Overriden for focus traversal.
  virtual bool EnumerateFloatingViews(
      views::View::FloatingViewPosition position,
      int starting_id, int* id);
  virtual views::View* ValidateFloatingViewForID(int id);

  // Render the visible area.
  virtual void Paint(ChromeCanvas* canvas);

  // BaseHistoryModelObserver implementation.
  void ModelChanged(bool result_set_changed);
  void ModelBeginWork();
  void ModelEndWork();

  // Returns the entry height, varies with font-height to prevent clipping.
  int GetEntryHeight();

  // Sets whether the delete controls are visible.
  void SetShowDeleteControls(bool show_delete_controls);

  // We expose the PageNavigator so history entries can cause navigations
  // directly.
  PageNavigator* navigator() const { return navigator_; }

  // Scrolling.
  virtual int GetPageScrollIncrement(views::ScrollView* scroll_view,
                                     bool is_horizontal, bool is_positive);
  virtual int GetLineScrollIncrement(views::ScrollView* scroll_view,
                                     bool is_horizontal, bool is_positive);
  virtual views::VariableRowHeightScrollHelper::RowInfo GetRowInfo(int y);

 private:
  // For any given break (see comments for BreakOffsets, below), we store the
  // index of the item following the break, and whether or not the break
  // corresponds to a day break or session break.
  struct BreakValue {
    int index;
    bool day;
  };

  // The map of our breaks (see comments for BreakOffsets, below).
  typedef std::map<int, BreakValue> BreakOffsets;

  // Ensures the renderers are valid.
  void EnsureRenderer();

  // Returns the bottom of the last entry.
  int GetLastEntryMaxY();

  // Returns the max view id.
  int GetMaxViewID();

  // Returns the y coordinate for the view with the specified floating view id,
  // the index into the model of the specified view and whether the view is
  // a delete control.
  int GetYCoordinateForViewID(int view_id,
                              int* model_index,
                              bool* is_delete_control);

  // Invoked when the user clicks the delete previous visits link.
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Prompts the user to make sure they really want to delete, and if so
  // deletes the day at the specified model index.
  void DeleteDayAtModelIndex(int index);

  // Returns the number of delete controls shown before the specified iterator.
  int CalculateDeleteOffset(const BreakOffsets::const_iterator& it);

  // Returns the width of the delete control, calculating if necessary.
  int GetDeleteControlWidth();

  // Calculates the bounds for the delete control given the specified y
  // location.
  gfx::Rect CalculateDeleteControlBounds(int base_y);

  // The "searchable view" container for this view.
  SearchableUIContainer* container_;

  // The font used for the "n days" ago heading.
  ChromeFont day_break_font_;

  // A "stamper"-style renderer for only painting the things that are
  // in the current view.
  HistoryItemRenderer* renderer_;

  // Used to render 'delete' controls.
  scoped_ptr<views::Link> delete_renderer_;

  // Class that performs the navigation when the user clicks on a page.
  PageNavigator* navigator_;

  // Pointer to the model that provides the contents of this view.
  scoped_ptr<BaseHistoryModel> model_;

  // For laying out the potentially huge list of history entries, we
  // cache the offsets of session and day breaks.
  //
  // Each entry in BreakOffsets is a pair, where the key is the y
  // coordinate of a day heading and the value is a struct containing
  // both the index of the first history entry after that day
  // heading and a boolean value indicating whether the offset
  // represents a day or session break (these display differently).
  //
  // This lets us quickly compute, for a given y value, how to lay out
  // entries in the vicinity of that y value.
  //
  // Here's an example:
  //   4 days ago             <- key: the y coordinate of the top of this block
  //   +----------------
  //   | history item #7      <- index: 7, the index of this history item
  //   +----------------
  //   +----------------
  //   | history item #8
  //   +----------------
  //                          <- key: the y coordinate of this separator
  //   +----------------
  //   | history item #9      <- index: 9, the index of this history item
  //   +----------------
  //   +----------------
  //   | history item #10
  //   +----------------
  //   5 days ago             <- key: the y coordinate of this block
  //   +----------------
  //   | history item #11     <- index: 11
  //   +----------------

  // Each history item is represented as a floating view. In addition the
  // the delete controls are represented as floating views. A ramification
  // of this is that when the delete controls are displayed the view ids do
  // not necessarily directly correspond to a model index. The following
  // example shows the view ids and corresponding model index.
  //
  //              delete      <- view_id=10
  //   +----------------
  //   | history item #7      <- view_id=11, model_index=10
  //   +----------------
  //   +----------------
  //   | history item #8      <- view_id=12, model_index=11
  //   +----------------
  //              delete      <- view_id=13
  //   +----------------
  //   | history item #9      <- view_id=14, model_index=12
  //   +----------------
  //
  BreakOffsets break_offsets_;

  // Retrieve the nearest BreakOffsets less than or equal to the given y.
  // Another way of looking at this is that it fetches the BreakOffsets
  // entry that heads the section containing y.
  BreakOffsets::iterator GetBreakOffsetIteratorForY(int y);

  int GetBreakOffsetHeight(BreakValue value);

  views::VariableRowHeightScrollHelper scroll_helper_;

  // Whether we are showing search results.
  bool show_results_;

  // The loading state of the model.
  bool loading_;

  // Whether we're showing delete controls.
  bool show_delete_controls_;

  // How tall a single line of text is.
  int line_height_;

  // Width needed for the delete control. Calculated in GetDeleteControlWidth.
  int delete_control_width_;

  DISALLOW_EVIL_CONSTRUCTORS(HistoryView);
};

#endif  // CHROME_BROWSER_HISTORY_VIEW_H_

