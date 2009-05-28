// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/tab.h"

#include "app/gfx/canvas.h"
#include "app/gfx/font.h"
#include "app/gfx/path.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/gfx/size.h"
#include "grit/generated_resources.h"
#include "views/controls/menu/chrome_menu.h"
#include "views/widget/tooltip_manager.h"
#include "views/widget/widget.h"

const std::string Tab::kTabClassName = "browser/tabs/Tab";

static const SkScalar kTabCapWidth = 15;
static const SkScalar kTabTopCurveWidth = 4;
static const SkScalar kTabBottomCurveWidth = 3;

class Tab::ContextMenuController : public views::MenuDelegate {
 public:
  explicit ContextMenuController(Tab* tab)
      : tab_(tab),
        last_command_(TabStripModel::CommandFirst) {
    menu_.reset(new views::MenuItemView(this));
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
    menu_->AppendMenuItemWithLabel(
      TabStripModel::CommandRestoreTab, l10n_util::GetString(IDS_RESTORE_TAB));
  }

  void RunMenuAt(int x, int y) {
    views::MenuItemView::AnchorPosition anchor =
        (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) ?
        views::MenuItemView::TOPRIGHT : views::MenuItemView::TOPLEFT;
    menu_->RunMenuAt(tab_->GetWidget()->GetNativeView(), gfx::Rect(x, y, 0, 0),
                     anchor, true);
    if (tab_)
      tab_->ContextMenuClosed();
    delete this;
  }

  void Cancel() {
    tab_ = NULL;
    menu_->Cancel();
  }

 private:
  virtual ~ContextMenuController() {
  }

  // views::MenuDelegate implementation:
  virtual bool IsCommandEnabled(int id) const {
    // The MenuItemView used to contain the contents of the Context Menu itself
    // has a command id of 0, and it will check to see if it's enabled for
    // some reason during its construction. The TabStripModel can't handle
    // command indices it doesn't know about, so we need to filter this out
    // here.
    if (id == 0 || !tab_)
      return false;
    return tab_->delegate()->IsCommandEnabledForTab(
        static_cast<TabStripModel::ContextMenuCommand>(id),
        tab_);
  }

  virtual void ExecuteCommand(int id) {
    if (!tab_)
      return;

    tab_->delegate()->ExecuteCommandForTab(
        static_cast<TabStripModel::ContextMenuCommand>(id),
        tab_);
  }

  virtual void SelectionChanged(views::MenuItemView* menu) {
    if (!tab_)
      return;

    TabStripModel::ContextMenuCommand command =
        static_cast<TabStripModel::ContextMenuCommand>(menu->GetCommand());
    tab_->delegate()->StopHighlightTabsForCommand(last_command_, tab_);
    last_command_ = command;
    tab_->delegate()->StartHighlightTabsForCommand(command, tab_);
  }

 private:
  // The context menu.
  scoped_ptr<views::MenuItemView> menu_;

  // The Tab the context menu was brought up for. Set to NULL when the menu
  // is canceled.
  Tab* tab_;

  // The last command that was selected, so that we can start/stop highlighting
  // appropriately as the user moves through the menu.
  TabStripModel::ContextMenuCommand last_command_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuController);
};

///////////////////////////////////////////////////////////////////////////////
// Tab, public:

Tab::Tab(TabDelegate* delegate)
    : TabRenderer(),
      delegate_(delegate),
      closing_(false),
      menu_controller_(NULL) {
  close_button()->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_CLOSE));
  close_button()->SetAnimationDuration(0);
  SetContextMenuController(this);
}

Tab::~Tab() {
  if (menu_controller_) {
    // The menu is showing. Close the menu.
    menu_controller_->Cancel();

    // Invoke this so that we hide the highlight.
    ContextMenuClosed();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Tab, TabRenderer overrides:

bool Tab::IsSelected() const {
  return delegate_->IsTabSelected(this);
}

///////////////////////////////////////////////////////////////////////////////
// Tab, views::View overrides:

bool Tab::HasHitTestMask() const {
  return true;
}

void Tab::GetHitTestMask(gfx::Path* mask) const {
  MakePathForTab(mask);
}

bool Tab::OnMousePressed(const views::MouseEvent& event) {
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

bool Tab::OnMouseDragged(const views::MouseEvent& event) {
  delegate_->ContinueDrag(event);
  return true;
}

void Tab::OnMouseReleased(const views::MouseEvent& event, bool canceled) {
  // Notify the drag helper that we're done with any potential drag operations.
  // Clean up the drag helper, which is re-created on the next mouse press.
  // In some cases, ending the drag will schedule the tab for destruction; if
  // so, bail immediately, since our members are already dead and we shouldn't
  // do anything else except drop the tab where it is.
  if (delegate_->EndDrag(canceled))
    return;

  // Close tab on middle click, but only if the button is released over the tab
  // (normal windows behavior is to discard presses of a UI element where the
  // releases happen off the element).
  if (event.IsMiddleMouseButton() && HitTest(event.location()))
    delegate_->CloseTab(this);
}

bool Tab::GetTooltipText(int x, int y, std::wstring* tooltip) {
  std::wstring title = GetTitle();
  if (!title.empty()) {
    // Only show the tooltip if the title is truncated.
    gfx::Font font;
    if (font.GetStringWidth(title) > title_bounds().width()) {
      *tooltip = title;
      return true;
    }
  }
  return false;
}

bool Tab::GetTooltipTextOrigin(int x, int y, gfx::Point* origin) {
  gfx::Font font;
  origin->set_x(title_bounds().x() + 10);
  origin->set_y(-views::TooltipManager::GetTooltipHeight() - 4);
  return true;
}

bool Tab::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);

  *role = AccessibilityTypes::ROLE_PAGETAB;
  return true;
}

bool Tab::GetAccessibleName(std::wstring* name) {
  *name = GetTitle();
  return !name->empty();
}

///////////////////////////////////////////////////////////////////////////////
// Tab, views::ContextMenuController implementation:

void Tab::ShowContextMenu(views::View* source, int x, int y,
                          bool is_mouse_gesture) {
  if (menu_controller_)
    return;
  menu_controller_ = new ContextMenuController(this);
  menu_controller_->RunMenuAt(x, y);
  // ContextMenuController takes care of deleting itself.
}

///////////////////////////////////////////////////////////////////////////////
// views::ButtonListener implementation:

void Tab::ButtonPressed(views::Button* sender) {
  if (sender == close_button())
    delegate_->CloseTab(this);
}

///////////////////////////////////////////////////////////////////////////////
// Tab, private:

void Tab::MakePathForTab(gfx::Path* path) const {
  DCHECK(path);

  SkScalar h = SkIntToScalar(height());
  SkScalar w = SkIntToScalar(width());

  path->moveTo(0, h);

  // Left end cap.
  path->lineTo(kTabBottomCurveWidth, h - kTabBottomCurveWidth);
  path->lineTo(kTabCapWidth - kTabTopCurveWidth, kTabTopCurveWidth);
  path->lineTo(kTabCapWidth, 0);

  // Connect to the right cap.
  path->lineTo(w - kTabCapWidth, 0);

  // Right end cap.
  path->lineTo(w - kTabCapWidth + kTabTopCurveWidth, kTabTopCurveWidth);
  path->lineTo(w - kTabBottomCurveWidth, h - kTabBottomCurveWidth);
  path->lineTo(w, h);

  // Close out the path.
  path->lineTo(0, h);
  path->close();
}

void Tab::ContextMenuClosed() {
  delegate()->StopAllHighlighting();
  menu_controller_ = NULL;
}
