// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_GTK_TABS_TAB_GTK_H_
#define CHROME_BROWSER_VIEWS_GTK_TABS_TAB_GTK_H_

#include "base/basictypes.h"
#include "chrome/browser/gtk/tabs/tab_renderer_gtk.h"
#include "chrome/browser/tabs/tab_strip_model.h"

namespace gfx {
class Path;
}

class TabGtk : public TabRendererGtk {
 public:
  // An interface implemented by an object that can help this Tab complete
  // various actions. The index parameter is the index of this Tab in the
  // TabRenderer::Model.
  class TabDelegate {
   public:
    // Returns true if the specified Tab is selected.
    virtual bool IsTabSelected(const TabGtk* tab) const = 0;

    // Selects the specified Tab.
    virtual void SelectTab(TabGtk* tab) = 0;

    // Closes the specified Tab.
    virtual void CloseTab(TabGtk* tab) = 0;

    // Returns true if the specified command is enabled for the specified Tab.
    virtual bool IsCommandEnabledForTab(
        TabStripModel::ContextMenuCommand command_id,
        const TabGtk* tab) const = 0;

    // Executes the specified command for the specified Tab.
    virtual void ExecuteCommandForTab(
        TabStripModel::ContextMenuCommand command_id, TabGtk* tab) = 0;

    // Starts/Stops highlighting the tabs that will be affected by the
    // specified command for the specified Tab.
    virtual void StartHighlightTabsForCommand(
        TabStripModel::ContextMenuCommand command_id, TabGtk* tab) = 0;
    virtual void StopHighlightTabsForCommand(
        TabStripModel::ContextMenuCommand command_id, TabGtk* tab) = 0;
    virtual void StopAllHighlighting() = 0;

    // Ends dragging a Tab. |canceled| is true if the drag was aborted in a way
    // other than the user releasing the mouse. Returns whether the tab has been
    // destroyed.
    virtual bool EndDrag(bool canceled) = 0;

    // Returns true if the associated TabStrip's delegate supports tab moving or
    // detaching. Used by the Frame to determine if dragging on the Tab
    // itself should move the window in cases where there's only one
    // non drag-able Tab.
    virtual bool HasAvailableDragActions() const = 0;
  };

  explicit TabGtk(TabDelegate* delegate);
  virtual ~TabGtk();

  // Access the delegate.
  TabDelegate* delegate() const { return delegate_; }

  // Used to set/check whether this Tab is being animated closed.
  void set_closing(bool closing) { closing_ = closing; }
  bool closing() const { return closing_; }

  // Checks whether |point| is inside the bounds of the tab.
  bool IsPointInBounds(const gfx::Point& point);

  // TabRendererGtk overrides:
  virtual bool IsSelected() const;

 private:
  // Creates a clickable region of the tab's visual representation. Used for
  // hit-testing.  Caller is responsible for destroying the region.
  GdkRegion* MakeRegionForTab() const;

  // An instance of a delegate object that can perform various actions based on
  // user gestures.
  TabDelegate* delegate_;

  // True if the tab is being animated closed.
  bool closing_;

  DISALLOW_COPY_AND_ASSIGN(TabGtk);
};

#endif  // CHROME_BROWSER_VIEWS_GTK_TABS_TAB_GTK_H_
