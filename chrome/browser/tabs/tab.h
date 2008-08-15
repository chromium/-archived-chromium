// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_TABS_TAB_H_
#define CHROME_BROWSER_TABS_TAB_H_

#include "chrome/browser/tabs/tab_renderer.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/views/base_button.h"

namespace gfx {
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
            public ChromeViews::ContextMenuController,
            public ChromeViews::BaseButton::ButtonListener {
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
    virtual void MaybeStartDrag(Tab* tab,
                                const ChromeViews::MouseEvent& event) = 0;

    // Continues dragging a Tab.
    virtual void ContinueDrag(const ChromeViews::MouseEvent& event) = 0;

    // Ends dragging a Tab. |canceled| is true if the drag was aborted in a way
    // other than the user releasing the mouse.
    virtual void EndDrag(bool canceled) = 0;
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
  // ChromeViews::View overrides:
  virtual bool OnMousePressed(const ChromeViews::MouseEvent& event);
  virtual bool OnMouseDragged(const ChromeViews::MouseEvent& event);
  virtual void OnMouseReleased(const ChromeViews::MouseEvent& event,
                               bool canceled);
  virtual bool GetTooltipText(int x, int y, std::wstring* tooltip);
  virtual bool GetTooltipTextOrigin(int x, int y, CPoint* origin);
  virtual std::string GetClassName() const { return kTabClassName; }
  virtual bool GetAccessibleRole(VARIANT* role);
  virtual bool GetAccessibleName(std::wstring* name);

  // ChromeViews::ContextMenuController overrides:
  virtual void ShowContextMenu(ChromeViews::View* source,
                               int x,
                               int y,
                               bool is_mouse_gesture);

  // ChromeViews::BaseButton::ButtonListener overrides:
  virtual void ButtonPressed(ChromeViews::BaseButton* sender);

  // An instance of a delegate object that can perform various actions based on
  // user gestures.
  TabDelegate* delegate_;

  // True if the tab is being animated closed.
  bool closing_;

  DISALLOW_COPY_AND_ASSIGN(Tab);
};

#endif  // CHROME_BROWSER_TABS_TAB_H_
