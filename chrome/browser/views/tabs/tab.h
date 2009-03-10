// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_H_
#define CHROME_BROWSER_VIEWS_TABS_TAB_H_

#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/views/tabs/tab_renderer.h"
#include "chrome/views/base_button.h"

namespace gfx {
class Path;
class Point;
}
class TabContents;
class Profile;

///////////////////////////////////////////////////////////////////////////////
//
// Tab
//
//  A subclass of TabRenderer that represents an individual Tab in a TabStrip.
//
///////////////////////////////////////////////////////////////////////////////
class Tab : public TabRenderer,
            public views::ContextMenuController,
            public views::BaseButton::ButtonListener {
 public:
  static const std::string kTabClassName;

  // An interface implemented by an object that can help this Tab complete
  // various actions. The index parameter is the index of this Tab in the
  // TabRenderer::Model.
  class TabDelegate {
   public:
    // Returns true if the specified Tab is selected.
    virtual bool IsTabSelected(const Tab* tab) const = 0;

    // Selects the specified Tab.
    virtual void SelectTab(Tab* tab) = 0;

    // Closes the specified Tab.
    virtual void CloseTab(Tab* tab) = 0;

    // Returns true if the specified command is enabled for the specified Tab.
    virtual bool IsCommandEnabledForTab(
        TabStripModel::ContextMenuCommand command_id, const Tab* tab) const = 0;

    // Executes the specified command for the specified Tab.
    virtual void ExecuteCommandForTab(
        TabStripModel::ContextMenuCommand command_id, Tab* tab) = 0;

    // Starts/Stops highlighting the tabs that will be affected by the
    // specified command for the specified Tab.
    virtual void StartHighlightTabsForCommand(
        TabStripModel::ContextMenuCommand command_id, Tab* tab) = 0;
    virtual void StopHighlightTabsForCommand(
        TabStripModel::ContextMenuCommand command_id, Tab* tab) = 0;
    virtual void StopAllHighlighting() = 0;

    // Potentially starts a drag for the specified Tab.
    virtual void MaybeStartDrag(Tab* tab, const views::MouseEvent& event) = 0;

    // Continues dragging a Tab.
    virtual void ContinueDrag(const views::MouseEvent& event) = 0;

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

  explicit Tab(TabDelegate* delegate);
  virtual ~Tab();

  // Access the delegate.
  TabDelegate* delegate() const { return delegate_; }

  // Used to set/check whether this Tab is being animated closed.
  void set_closing(bool closing) { closing_ = closing; }
  bool closing() const { return closing_; }

  // TabRenderer overrides:
  virtual bool IsSelected() const;

 private:
  class ContextMenuController;

  friend class ContextMenuController;

  // views::View overrides:
  virtual bool HasHitTestMask() const;
  virtual void GetHitTestMask(gfx::Path* mask) const;
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual bool OnMouseDragged(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event,
                               bool canceled);
  virtual bool GetTooltipText(int x, int y, std::wstring* tooltip);
  virtual bool GetTooltipTextOrigin(int x, int y, CPoint* origin);
  virtual std::string GetClassName() const { return kTabClassName; }
  virtual bool GetAccessibleRole(VARIANT* role);
  virtual bool GetAccessibleName(std::wstring* name);

  // views::ContextMenuController overrides:
  virtual void ShowContextMenu(views::View* source,
                               int x,
                               int y,
                               bool is_mouse_gesture);

  // views::BaseButton::ButtonListener overrides:
  virtual void ButtonPressed(views::BaseButton* sender);

  // Creates a path that contains the clickable region of the tab's visual
  // representation. Used by GetViewForPoint for hit-testing.
  void MakePathForTab(gfx::Path* path) const;

  // Invoked when the context menu closes.
  void ContextMenuClosed();

  // An instance of a delegate object that can perform various actions based on
  // user gestures.
  TabDelegate* delegate_;

  // True if the tab is being animated closed.
  bool closing_;

  // If non-null it means we're showing a menu for the tab.
  ContextMenuController* menu_controller_;

  DISALLOW_COPY_AND_ASSIGN(Tab);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_TAB_H_
