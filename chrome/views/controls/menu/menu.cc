// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/controls/menu/menu.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>
#include <string>

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "chrome/views/accelerator.h"
#include "base/string_util.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/l10n_util_win.h"
#include "chrome/common/stl_util-inl.h"

const SkBitmap* Menu::Delegate::kEmptyIcon = 0;

// The width of an icon, including the pixels between the icon and
// the item label.
static const int kIconWidth = 23;
// Margins between the top of the item and the label.
static const int kItemTopMargin = 3;
// Margins between the bottom of the item and the label.
static const int kItemBottomMargin = 4;
// Margins between the left of the item and the icon.
static const int kItemLeftMargin = 4;
// Margins between the right of the item and the label.
static const int kItemRightMargin = 10;
// The width for displaying the sub-menu arrow.
static const int kArrowWidth = 10;

// Current active MenuHostWindow. If NULL, no menu is active.
static MenuHostWindow* active_host_window = NULL;

// The data of menu items needed to display.
struct Menu::ItemData {
  std::wstring label;
  SkBitmap icon;
  bool submenu;
};

namespace {

static int ChromeGetMenuItemID(HMENU hMenu, int pos) {
  // The built-in Windows ::GetMenuItemID doesn't work for submenus,
  // so here's our own implementation.
  MENUITEMINFO mii = {0};
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_ID;
  GetMenuItemInfo(hMenu, pos, TRUE, &mii);
  return mii.wID;
}

// MenuHostWindow -------------------------------------------------------------

// MenuHostWindow is the HWND the HMENU is parented to. MenuHostWindow is used
// to intercept right clicks on the HMENU and notify the delegate as well as
// for drawing icons.
//
class MenuHostWindow : public CWindowImpl<MenuHostWindow, CWindow,
                                          CWinTraits<WS_CHILD>> {
 public:
  MenuHostWindow(Menu* menu, HWND parent_window) : menu_(menu) {
    int extended_style = 0;
    // If the menu needs to be created with a right-to-left UI layout, we must
    // set the appropriate RTL flags (such as WS_EX_LAYOUTRTL) property for the
    // underlying HWND.
    if (menu_->delegate_->IsRightToLeftUILayout())
      extended_style |= l10n_util::GetExtendedStyles();
    Create(parent_window, gfx::Rect().ToRECT(), 0, 0, extended_style);
  }

  ~MenuHostWindow() {
    DestroyWindow();
  }

  DECLARE_FRAME_WND_CLASS(L"MenuHostWindow", NULL);
  BEGIN_MSG_MAP(MenuHostWindow);
    MSG_WM_RBUTTONUP(OnRButtonUp)
    MSG_WM_MEASUREITEM(OnMeasureItem)
    MSG_WM_DRAWITEM(OnDrawItem)
  END_MSG_MAP();

 private:
  // NOTE: I really REALLY tried to use WM_MENURBUTTONUP, but I ran into
  // two problems in using it:
  // 1. It doesn't contain the coordinates of the mouse.
  // 2. It isn't invoked for menuitems representing a submenu that have children
  //   menu items (not empty).

  void OnRButtonUp(UINT w_param, const CPoint& loc) {
    int id;
    if (menu_->delegate_ && FindMenuIDByLocation(menu_, loc, &id))
      menu_->delegate_->ShowContextMenu(menu_, id, loc.x, loc.y, true);
  }

  void OnMeasureItem(WPARAM w_param, MEASUREITEMSTRUCT* lpmis) {
    Menu::ItemData* data = reinterpret_cast<Menu::ItemData*>(lpmis->itemData);
    if (data != NULL) {
      ChromeFont font;
      lpmis->itemWidth = font.GetStringWidth(data->label) + kIconWidth +
          kItemLeftMargin + kItemRightMargin -
          GetSystemMetrics(SM_CXMENUCHECK);
      if (data->submenu)
        lpmis->itemWidth += kArrowWidth;
      lpmis->itemHeight = font.height() + kItemBottomMargin + kItemTopMargin;
    } else {
      // Measure separator size.
      lpmis->itemHeight = GetSystemMetrics(SM_CYMENU) / 2;
      lpmis->itemWidth = 0;
    }
  }

  void OnDrawItem(UINT wParam, DRAWITEMSTRUCT* lpdis) {
    HDC hDC = lpdis->hDC;
    COLORREF prev_bg_color, prev_text_color;

    // Set background color and text color
    if (lpdis->itemState & ODS_SELECTED) {
      prev_bg_color = SetBkColor(hDC, GetSysColor(COLOR_HIGHLIGHT));
      prev_text_color = SetTextColor(hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
    } else {
      prev_bg_color = SetBkColor(hDC, GetSysColor(COLOR_MENU));
      if (lpdis->itemState & ODS_DISABLED)
        prev_text_color = SetTextColor(hDC, GetSysColor(COLOR_GRAYTEXT));
      else
        prev_text_color = SetTextColor(hDC, GetSysColor(COLOR_MENUTEXT));
    }

    if (lpdis->itemData) {
      Menu::ItemData* data =
          reinterpret_cast<Menu::ItemData*>(lpdis->itemData);
      wchar_t* str = const_cast<wchar_t*>(data->label.c_str());

      // Draw the background.
      HBRUSH hbr = CreateSolidBrush(GetBkColor(hDC));
      FillRect(hDC, &lpdis->rcItem, hbr);
      DeleteObject(hbr);

      // Draw the label.
      RECT rect = lpdis->rcItem;
      rect.top += kItemTopMargin;
      rect.left += kItemLeftMargin + kIconWidth;
      UINT format = DT_TOP | DT_LEFT | DT_SINGLELINE;
      // Check whether the mnemonics should be underlined.
      BOOL underline_mnemonics;
      SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &underline_mnemonics, 0);
      if (!underline_mnemonics)
        format |= DT_HIDEPREFIX;
      DrawTextEx(hDC, str, static_cast<int>(data->label.size()),
                 &rect, format, NULL);

      // Draw the icon after the label, otherwise it would be covered
      // by the label.
      if (data->icon.width() != 0 && data->icon.height() != 0) {
        ChromeCanvas canvas(data->icon.width(), data->icon.height(), false);
        canvas.drawColor(SK_ColorBLACK, SkPorterDuff::kClear_Mode);
        canvas.DrawBitmapInt(data->icon, 0, 0);
        canvas.getTopPlatformDevice().drawToHDC(hDC, lpdis->rcItem.left +
            kItemLeftMargin, lpdis->rcItem.top + (lpdis->rcItem.bottom -
                lpdis->rcItem.top - data->icon.height()) / 2, NULL);
      }

    } else {
      // Draw the separator
      lpdis->rcItem.top += (lpdis->rcItem.bottom - lpdis->rcItem.top) / 3;
      DrawEdge(hDC, &lpdis->rcItem, EDGE_ETCHED, BF_TOP);
    }

    SetBkColor(hDC, prev_bg_color);
    SetTextColor(hDC, prev_text_color);
  }

  bool FindMenuIDByLocation(Menu* menu, const CPoint& loc, int* id) {
    int index = MenuItemFromPoint(NULL, menu->menu_, loc);
    if (index != -1) {
      *id = ChromeGetMenuItemID(menu->menu_, index);
      return true;
    } else {
      for (std::vector<Menu*>::iterator i = menu->submenus_.begin();
           i != menu->submenus_.end(); ++i) {
        if (FindMenuIDByLocation(*i, loc, id))
          return true;
      }
    }
    return false;
  }

  // The menu that created us.
  Menu* menu_;

  DISALLOW_EVIL_CONSTRUCTORS(MenuHostWindow);
};

}  // namespace

bool Menu::Delegate::IsRightToLeftUILayout() const {
  return l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT;
}

const SkBitmap& Menu::Delegate::GetEmptyIcon() const {
  if (kEmptyIcon == NULL)
    kEmptyIcon = new SkBitmap();
  return *kEmptyIcon;
}

Menu::Menu(Delegate* delegate, AnchorPoint anchor, HWND owner)
    : delegate_(delegate),
      menu_(CreatePopupMenu()),
      anchor_(anchor),
      owner_(owner),
      is_menu_visible_(false),
      owner_draw_(false) {
  DCHECK(delegate_);
}

Menu::Menu(Menu* parent)
    : delegate_(parent->delegate_),
      menu_(CreatePopupMenu()),
      anchor_(parent->anchor_),
      owner_(parent->owner_),
      is_menu_visible_(false),
      owner_draw_(false) {
}

Menu::Menu(HMENU hmenu)
    : delegate_(NULL),
      menu_(hmenu),
      anchor_(TOPLEFT),
      owner_(NULL),
      is_menu_visible_(false),
      owner_draw_(false) {
  DCHECK(menu_);
}

Menu::~Menu() {
  STLDeleteContainerPointers(submenus_.begin(), submenus_.end());
  STLDeleteContainerPointers(item_data_.begin(), item_data_.end());
  DestroyMenu(menu_);
}

UINT Menu::GetStateFlagsForItemID(int item_id) const {
  // Use the delegate to get enabled and checked state.
  UINT flags =
    delegate_->IsCommandEnabled(item_id) ? MFS_ENABLED : MFS_DISABLED;

  if (delegate_->IsItemChecked(item_id))
    flags |= MFS_CHECKED;

  if (delegate_->IsItemDefault(item_id))
    flags |= MFS_DEFAULT;

  return flags;
}

void Menu::AddMenuItemInternal(int index,
                               int item_id,
                               const std::wstring& label,
                               const SkBitmap& icon,
                               HMENU submenu,
                               MenuItemType type) {
  DCHECK(type != SEPARATOR) << "Call AddSeparator instead!";

  if (label.empty() && !delegate_) {
    // No label and no delegate; don't add an empty menu.
    // It appears under some circumstance we're getting an empty label
    // (l10n_util::GetString(IDS_TASK_MANAGER) returns ""). This shouldn't
    // happen, but I'm working over the crash here.
    NOTREACHED();
    return;
  }

  MENUITEMINFO mii;
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_FTYPE | MIIM_ID;
  if (submenu) {
    mii.fMask |= MIIM_SUBMENU;
    mii.hSubMenu = submenu;
  }

  // Set the type and ID.
  if (!owner_draw_) {
    mii.fType = MFT_STRING;
    mii.fMask |= MIIM_STRING;
  } else {
    mii.fType = MFT_OWNERDRAW;
  }

  if (type == RADIO)
    mii.fType |= MFT_RADIOCHECK;

  mii.wID = item_id;

  // Set the item data.
  Menu::ItemData* data = new ItemData;
  item_data_.push_back(data);
  data->submenu = submenu != NULL;

  std::wstring actual_label(label.empty() ?
      delegate_->GetLabel(item_id) : label);

  // Find out if there is a shortcut we need to append to the label.
  views::Accelerator accelerator(0, false, false, false);
  if (delegate_ && delegate_->GetAcceleratorInfo(item_id, &accelerator)) {
    actual_label += L'\t';
    actual_label += accelerator.GetShortcutText();
  }
  labels_.push_back(actual_label);

  if (owner_draw_) {
    if (icon.width() != 0 && icon.height() != 0)
      data->icon = icon;
    else
      data->icon = delegate_->GetIcon(item_id);
  } else {
    mii.dwTypeData = const_cast<wchar_t*>(labels_.back().c_str());
  }

  InsertMenuItem(menu_, index, TRUE, &mii);
}

void Menu::AppendMenuItem(int item_id,
                          const std::wstring& label,
                          MenuItemType type) {
  AddMenuItem(-1, item_id, label, type);
}

void Menu::AddMenuItem(int index,
                       int item_id,
                       const std::wstring& label,
                       MenuItemType type) {
  if (type == SEPARATOR)
    AddSeparator(index);
  else
    AddMenuItemInternal(index, item_id, label, SkBitmap(), NULL, type);
}

Menu* Menu::AppendSubMenu(int item_id, const std::wstring& label) {
  return AddSubMenu(-1, item_id, label);
}

Menu* Menu::AddSubMenu(int index, int item_id, const std::wstring& label) {
  return AddSubMenuWithIcon(index, item_id, label, SkBitmap());
}

Menu* Menu::AppendSubMenuWithIcon(int item_id,
                                  const std::wstring& label,
                                  const SkBitmap& icon) {
  return AddSubMenuWithIcon(-1, item_id, label, icon);
}

Menu* Menu::AddSubMenuWithIcon(int index,
                               int item_id,
                               const std::wstring& label,
                               const SkBitmap& icon) {
  if (!owner_draw_ && icon.width() != 0 && icon.height() != 0)
    owner_draw_ = true;

  Menu* submenu = new Menu(this);
  submenus_.push_back(submenu);
  AddMenuItemInternal(index, item_id, label, icon, submenu->menu_, NORMAL);
  return submenu;
}

void Menu::AppendMenuItemWithLabel(int item_id, const std::wstring& label) {
  AddMenuItemWithLabel(-1, item_id, label);
}

void Menu::AddMenuItemWithLabel(int index, int item_id,
                                const std::wstring& label) {
  AddMenuItem(index, item_id, label, Menu::NORMAL);
}

void Menu::AppendDelegateMenuItem(int item_id) {
  AddDelegateMenuItem(-1, item_id);
}

void Menu::AddDelegateMenuItem(int index, int item_id) {
  AddMenuItem(index, item_id, std::wstring(), Menu::NORMAL);
}

void Menu::AppendSeparator() {
  AddSeparator(-1);
}

void Menu::AddSeparator(int index) {
  MENUITEMINFO mii;
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_FTYPE;
  mii.fType = MFT_SEPARATOR;
  InsertMenuItem(menu_, index, TRUE, &mii);
}

void Menu::AppendMenuItemWithIcon(int item_id,
                                  const std::wstring& label,
                                  const SkBitmap& icon) {
  AddMenuItemWithIcon(-1, item_id, label, icon);
}

void Menu::AddMenuItemWithIcon(int index,
                               int item_id,
                               const std::wstring& label,
                               const SkBitmap& icon) {
  if (!owner_draw_)
    owner_draw_ = true;
  AddMenuItemInternal(index, item_id, label, icon, NULL, Menu::NORMAL);
}

void Menu::EnableMenuItemByID(int item_id, bool enabled) {
  UINT enable_flags = enabled ? MF_ENABLED : MF_DISABLED | MF_GRAYED;
  EnableMenuItem(menu_, item_id, MF_BYCOMMAND | enable_flags);
}

void Menu::EnableMenuItemAt(int index, bool enabled) {
  UINT enable_flags = enabled ? MF_ENABLED : MF_DISABLED | MF_GRAYED;
  EnableMenuItem(menu_, index, MF_BYPOSITION | enable_flags);
}

DWORD Menu::GetTPMAlignFlags() const {
  // The manner in which we handle the menu alignment depends on whether or not
  // the menu is displayed within a mirrored view. If the UI is mirrored, the
  // alignment needs to be fliped so that instead of aligning the menu to the
  // right of the point, we align it to the left and vice versa.
  DWORD align_flags = TPM_TOPALIGN;
  switch (anchor_) {
    case TOPLEFT:
      if (delegate_->IsRightToLeftUILayout()) {
        align_flags |= TPM_RIGHTALIGN;
      } else {
        align_flags |= TPM_LEFTALIGN;
      }
      break;

    case TOPRIGHT:
      if (delegate_->IsRightToLeftUILayout()) {
        align_flags |= TPM_LEFTALIGN;
      } else {
        align_flags |= TPM_RIGHTALIGN;
      }
      break;

    default:
      NOTREACHED();
      return 0;
  }
  return align_flags;
}

bool Menu::SetIcon(const SkBitmap& icon, int item_id) {
  if (!owner_draw_)
    owner_draw_ = true;

  const int num_items = GetMenuItemCount(menu_);
  int sep_count = 0;
  for (int i = 0; i < num_items; ++i) {
    if (!(GetMenuState(menu_, i, MF_BYPOSITION) & MF_SEPARATOR)) {
      if (ChromeGetMenuItemID(menu_, i) == item_id) {
        item_data_[i - sep_count]->icon = icon;
        // When the menu is running, we use SetMenuItemInfo to let Windows
        // update the item information so that the icon being displayed
        // could change immediately.
        if (active_host_window) {
          MENUITEMINFO mii;
          mii.cbSize = sizeof(mii);
          mii.fMask = MIIM_FTYPE | MIIM_DATA;
          mii.fType = MFT_OWNERDRAW;
          mii.dwItemData =
              reinterpret_cast<ULONG_PTR>(item_data_[i - sep_count]);
          SetMenuItemInfo(menu_, item_id, false, &mii);
        }
        return true;
      }
    } else {
      ++sep_count;
    }
  }

  // Continue searching for the item in submenus.
  for (size_t i = 0; i < submenus_.size(); ++i) {
    if (submenus_[i]->SetIcon(icon, item_id))
      return true;
  }

  return false;
}

void Menu::SetMenuInfo() {
  const int num_items = GetMenuItemCount(menu_);
  int sep_count = 0;
  for (int i = 0; i < num_items; ++i) {
    MENUITEMINFO mii_info;
    mii_info.cbSize = sizeof(mii_info);
    // Get the menu's original type.
    mii_info.fMask = MIIM_FTYPE;
    GetMenuItemInfo(menu_, i, MF_BYPOSITION, &mii_info);
    // Set item states.
    if (!(mii_info.fType & MF_SEPARATOR)) {
      const int id = ChromeGetMenuItemID(menu_, i);

      MENUITEMINFO mii;
      mii.cbSize = sizeof(mii);
      mii.fMask = MIIM_STATE | MIIM_FTYPE | MIIM_DATA | MIIM_STRING;
      // We also need MFT_STRING for owner drawn items in order to let Windows
      // handle the accelerators for us.
      mii.fType = MFT_STRING;
      if (owner_draw_)
        mii.fType |= MFT_OWNERDRAW;
      // If the menu originally has radiocheck type, we should follow it.
      if (mii_info.fType & MFT_RADIOCHECK)
        mii.fType |= MFT_RADIOCHECK;
      mii.fState = GetStateFlagsForItemID(id);

      // Validate the label. If there is a contextual label, use it, otherwise
      // default to the static label
      std::wstring label;
      if (!delegate_->GetContextualLabel(id, &label))
        label = labels_[i - sep_count];

      if (owner_draw_) {
        item_data_[i - sep_count]->label = label;
        mii.dwItemData = reinterpret_cast<ULONG_PTR>(item_data_[i - sep_count]);
      }
      mii.dwTypeData = const_cast<wchar_t*>(label.c_str());
      mii.cch = static_cast<UINT>(label.size());
      SetMenuItemInfo(menu_, i, true, &mii);
    } else {
      // Set data for owner drawn separators. Set dwItemData NULL to indicate
      // a separator.
      if (owner_draw_) {
        MENUITEMINFO mii;
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_FTYPE;
        mii.fType = MFT_SEPARATOR | MFT_OWNERDRAW;
        mii.dwItemData = NULL;
        SetMenuItemInfo(menu_, i, true, &mii);
      }
      ++sep_count;
    }
  }

  for (size_t i = 0; i < submenus_.size(); ++i)
    submenus_[i]->SetMenuInfo();
}

void Menu::RunMenuAt(int x, int y) {
  SetMenuInfo();

  delegate_->MenuWillShow();

  // NOTE: we don't use TPM_RIGHTBUTTON here as it breaks selecting by way of
  // press, drag, release. See bugs 718 and 8560.
  UINT flags =
      GetTPMAlignFlags() | TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_RECURSE;
  is_menu_visible_ = true;
  DCHECK(owner_);
  // In order for context menus on menus to work, the context menu needs to
  // share the same window as the first menu is parented to.
  bool created_host = false;
  if (!active_host_window) {
    created_host = true;
    active_host_window = new MenuHostWindow(this, owner_);
  }
  UINT selected_id =
      TrackPopupMenuEx(menu_, flags, x, y, active_host_window->m_hWnd, NULL);
  if (created_host) {
    delete active_host_window;
    active_host_window = NULL;
  }
  is_menu_visible_ = false;

  // Execute the chosen command
  if (selected_id != 0)
    delegate_->ExecuteCommand(selected_id);
}

void Menu::Cancel() {
  DCHECK(is_menu_visible_);
  EndMenu();
}

int Menu::ItemCount() {
  return GetMenuItemCount(menu_);
}
