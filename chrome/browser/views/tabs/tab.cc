// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/tab.h"

#include "app/gfx/canvas.h"
#include "app/gfx/font.h"
#include "app/gfx/path.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/compiler_specific.h"
#include "base/gfx/size.h"
#include "grit/generated_resources.h"
#include "views/controls/menu/simple_menu_model.h"
#include "views/widget/tooltip_manager.h"
#include "views/widget/widget.h"

const std::string Tab::kTabClassName = "browser/tabs/Tab";

static const SkScalar kTabCapWidth = 15;
static const SkScalar kTabTopCurveWidth = 4;
static const SkScalar kTabBottomCurveWidth = 3;

class Tab::TabContextMenuContents : public views::SimpleMenuModel,
                                    public views::SimpleMenuModel::Delegate {
 public:
  explicit TabContextMenuContents(Tab* tab)
      : ALLOW_THIS_IN_INITIALIZER_LIST(SimpleMenuModel(this)),
        tab_(tab),
        last_command_(TabStripModel::CommandFirst) {
    Build();
  }
  virtual ~TabContextMenuContents() {
    menu_->CancelMenu();
    tab_->delegate()->StopAllHighlighting();
  }

  void RunMenuAt(const gfx::Point& point) {
    // Save a pointer to delegate before we call RunMenuAt, because it runs a
    // nested message loop that may not return until after we are deleted.
    Tab::TabDelegate* delegate = tab_->delegate();
    menu_->RunMenuAt(point, views::Menu2::ALIGN_TOPLEFT);
    // We could be gone now. Assume |this| is junk!
    if (delegate)
      delegate->StopAllHighlighting();
  }

  // Overridden from views::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const {
    return false;
  }
  virtual bool IsCommandIdEnabled(int command_id) const {
    return tab_ && tab_->delegate()->IsCommandEnabledForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id),
        tab_);
  }
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      views::Accelerator* accelerator) {
    return tab_->GetWidget()->GetAccelerator(command_id, accelerator);
  }
  virtual void CommandIdHighlighted(int command_id) {
    if (!tab_)
      return;

    tab_->delegate()->StopHighlightTabsForCommand(last_command_, tab_);
    last_command_ = static_cast<TabStripModel::ContextMenuCommand>(command_id);
    tab_->delegate()->StartHighlightTabsForCommand(last_command_, tab_);
  }
  virtual void ExecuteCommand(int command_id) {
    if (!tab_)
      return;
    tab_->delegate()->ExecuteCommandForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id),
        tab_);
  }

 private:
  void Build() {
    AddItemWithStringId(TabStripModel::CommandNewTab, IDS_TAB_CXMENU_NEWTAB);
    AddSeparator();
    AddItemWithStringId(TabStripModel::CommandReload, IDS_TAB_CXMENU_RELOAD);
    AddItemWithStringId(TabStripModel::CommandDuplicate,
                        IDS_TAB_CXMENU_DUPLICATE);
    AddSeparator();
    AddItemWithStringId(TabStripModel::CommandCloseTab,
                        IDS_TAB_CXMENU_CLOSETAB);
    AddItemWithStringId(TabStripModel::CommandCloseOtherTabs,
                        IDS_TAB_CXMENU_CLOSEOTHERTABS);
    AddItemWithStringId(TabStripModel::CommandCloseTabsToRight,
                        IDS_TAB_CXMENU_CLOSETABSTORIGHT);
    AddItemWithStringId(TabStripModel::CommandCloseTabsOpenedBy,
                        IDS_TAB_CXMENU_CLOSETABSOPENEDBY);
    AddItemWithStringId(TabStripModel::CommandRestoreTab, IDS_RESTORE_TAB);
    menu_.reset(new views::Menu2(this));
  }

  scoped_ptr<views::Menu2> menu_;

  // The Tab the context menu was brought up for. Set to NULL when the menu
  // is canceled.
  Tab* tab_;

  // The last command that was selected, so that we can start/stop highlighting
  // appropriately as the user moves through the menu.
  TabStripModel::ContextMenuCommand last_command_;

  DISALLOW_COPY_AND_ASSIGN(TabContextMenuContents);
};

///////////////////////////////////////////////////////////////////////////////
// Tab, public:

Tab::Tab(TabDelegate* delegate)
    : TabRenderer(),
      delegate_(delegate),
      closing_(false) {
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
  if (!context_menu_contents_.get())
    context_menu_contents_.reset(new TabContextMenuContents(this));
  context_menu_contents_->RunMenuAt(gfx::Point(x, y));
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
