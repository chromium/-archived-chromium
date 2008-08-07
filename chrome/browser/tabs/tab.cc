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

#include "chrome/browser/tabs/tab.h"

#include "base/gfx/size.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tabs/tab_strip.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/chrome_menu.h"
#include "chrome/views/tooltip_manager.h"
#include "generated_resources.h"

const std::string Tab::kTabClassName = "browser/tabs/Tab";

class TabContextMenuController : public ChromeViews::MenuDelegate {
 public:
  explicit TabContextMenuController(Tab* tab)
      : tab_(tab),
        last_command_(TabStripModel::CommandFirst) {
    menu_.reset(new ChromeViews::MenuItemView(this));
    menu_->AppendMenuItemWithLabel(TabStripModel::CommandNewTab,
                                   l10n_util::GetString(IDS_TAB_CXMENU_NEWTAB));
    menu_->AppendSeparator();
    menu_->AppendMenuItemWithLabel(TabStripModel::CommandReload,
                                   l10n_util::GetString(IDS_TAB_CXMENU_RELOAD));
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandDuplicate,
        l10n_util::GetString(IDS_TAB_CXMENU_DUPLICATE));
    menu_->AppendSeparator();
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandCloseTab,
        l10n_util::GetString(IDS_TAB_CXMENU_CLOSETAB));
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandCloseOtherTabs,
        l10n_util::GetString(IDS_TAB_CXMENU_CLOSEOTHERTABS));
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandCloseTabsToRight,
        l10n_util::GetString(IDS_TAB_CXMENU_CLOSETABSTORIGHT));
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandCloseTabsOpenedBy,
        l10n_util::GetString(IDS_TAB_CXMENU_CLOSETABSOPENEDBY));
  }
  virtual ~TabContextMenuController() {
    tab_->delegate()->StopAllHighlighting();
  }

  void RunMenuAt(int x, int y) {
    menu_->RunMenuAt(tab_->GetViewContainer()->GetHWND(),
                     gfx::Rect(x, y, 0, 0), ChromeViews::MenuItemView::TOPLEFT,
                     false);
  }

 private:
  // ChromeViews::MenuDelegate implementation:
  virtual bool IsCommandEnabled(int id) const {
    // The MenuItemView used to contain the contents of the Context Menu itself
    // has a command id of 0, and it will check to see if it's enabled for
    // some reason during its construction. The TabStripModel can't handle
    // command indices it doesn't know about, so we need to filter this out
    // here.
    if (id == 0)
      return false;
    return tab_->delegate()->IsCommandEnabledForTab(
        static_cast<TabStripModel::ContextMenuCommand>(id),
        tab_);
  }

  virtual void ExecuteCommand(int id) {
    tab_->delegate()->ExecuteCommandForTab(
        static_cast<TabStripModel::ContextMenuCommand>(id),
        tab_);
  }

  virtual void SelectionChanged(ChromeViews::MenuItemView* menu) {
    TabStripModel::ContextMenuCommand command =
        static_cast<TabStripModel::ContextMenuCommand>(menu->GetCommand());
    tab_->delegate()->StopHighlightTabsForCommand(last_command_, tab_);
    last_command_ = command;
    tab_->delegate()->StartHighlightTabsForCommand(command, tab_);
  }

 private:
  // The context menu.
  scoped_ptr<ChromeViews::MenuItemView> menu_;

  // The Tab the context menu was brought up for.
  Tab* tab_;

  // The last command that was selected, so that we can start/stop highlighting
  // appropriately as the user moves through the menu.
  TabStripModel::ContextMenuCommand last_command_;

  DISALLOW_EVIL_CONSTRUCTORS(TabContextMenuController);
};

///////////////////////////////////////////////////////////////////////////////
// Tab, public:

Tab::Tab(TabDelegate* delegate)
    : TabRenderer(),
      delegate_(delegate),
      closing_(false) {
  close_button()->SetListener(this, 0);
  close_button()->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_CLOSE));
  close_button()->SetAnimationDuration(0);
  SetContextMenuController(this);
}

Tab::~Tab() {
}

///////////////////////////////////////////////////////////////////////////////
// Tab, TabRenderer overrides:

bool Tab::IsSelected() const {
  return delegate_->IsTabSelected(this);
}

///////////////////////////////////////////////////////////////////////////////
// Tab, ChromeViews::View overrides:

bool Tab::OnMousePressed(const ChromeViews::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton()) {
    // Store whether or not we were selected just now... we only want to be
    // able to drag foreground tabs, so we don't start dragging the tab if
    // it was in the background.
    bool just_selected = !IsSelected();
    if (just_selected)
      delegate_->SelectTab(this);
    delegate_->MaybeStartDrag(this, event);
  }
  return true;
}

bool Tab::OnMouseDragged(const ChromeViews::MouseEvent& event) {
  delegate_->ContinueDrag(event);
  return true;
}

void Tab::OnMouseReleased(const ChromeViews::MouseEvent& event,
                          bool canceled) {
  // Notify the drag helper that we're done with any potential drag operations.
  // Clean up the drag helper, which is re-created on the next mouse press.
  delegate_->EndDrag(canceled);
  if (event.IsMiddleMouseButton())
    delegate_->CloseTab(this);
}

bool Tab::GetTooltipText(int x, int y, std::wstring* tooltip) {
  std::wstring title = GetTitle();
  if (!title.empty()) {
    // Only show the tooltip if the title is truncated.
    ChromeFont font;
    if (font.GetStringWidth(title) > title_bounds().width()) {
      *tooltip = title;
      return true;
    }
  }
  return false;
}

bool Tab::GetTooltipTextOrigin(int x, int y, CPoint* origin) {
  ChromeFont font;
  origin->x = title_bounds().x() + 10;
  origin->y = -ChromeViews::TooltipManager::GetTooltipHeight() - 4;
  return true;
}

bool Tab::GetAccessibleRole(VARIANT* role) {
  DCHECK(role);

  role->vt = VT_I4;
  role->lVal = ROLE_SYSTEM_PAGETAB;
  return true;
}

bool Tab::GetAccessibleName(std::wstring* name) {
  *name = GetTitle();
  return !name->empty();
}

///////////////////////////////////////////////////////////////////////////////
// Tab, ChromeViews::ContextMenuController implementation:

void Tab::ShowContextMenu(ChromeViews::View* source, int x, int y,
                          bool is_mouse_gesture) {
  TabContextMenuController controller(this);
  controller.RunMenuAt(x, y);
}


///////////////////////////////////////////////////////////////////////////////
// ChromeViews::BaseButton::ButtonListener implementation:

void Tab::ButtonPressed(ChromeViews::BaseButton* sender) {
  if (sender == close_button())
    delegate_->CloseTab(this);
}
