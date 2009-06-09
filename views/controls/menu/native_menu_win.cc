// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "views/controls/menu/native_menu_win.h"

#include "app/l10n_util.h"
#include "app/l10n_util_win.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "views/accelerator.h"
#include "views/controls/menu/menu_2.h"

namespace views {

struct NativeMenuWin::ItemData {
  // The Windows API requires that whoever creates the menus must own the
  // strings used for labels, and keep them around for the lifetime of the
  // created menu. So be it.
  std::wstring label;

  // Someone needs to own submenus, it may as well be us.
  scoped_ptr<Menu2> submenu;
};

// A window that receives messages from Windows relevant to the native menu
// structure we have constructed in NativeMenuWin.
class NativeMenuWin::MenuHostWindow {
 public:
  MenuHostWindow() {
    RegisterClass();
    hwnd_ = CreateWindowEx(l10n_util::GetExtendedStyles(), kWindowClassName,
                           L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    SetProp(hwnd_, kMenuHostWindowKey, this);
  }

  ~MenuHostWindow() {
    DestroyWindow(hwnd_);
  }

  HWND hwnd() const { return hwnd_; }

 private:
  static const wchar_t* kMenuHostWindowKey;
  static const wchar_t* kWindowClassName;

  void RegisterClass() {
    static bool registered = false;
    if (registered)
      return;

    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_DBLCLKS;
    wcex.lpfnWndProc = &MenuHostWindowProc;
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    wcex.lpszClassName = kWindowClassName;
    ATOM clazz = RegisterClassEx(&wcex);
    DCHECK(clazz);
    registered = true;
  }

  NativeMenuWin* GetNativeMenuWinFromHMENU(HMENU hmenu) const {
    MENUINFO mi = {0};
    mi.cbSize = sizeof(mi);
    mi.fMask = MIM_MENUDATA;
    GetMenuInfo(hmenu, &mi);
    return reinterpret_cast<NativeMenuWin*>(mi.dwMenuData);
  }

  // Converts the WPARAM value passed to WM_MENUSELECT into an index
  // corresponding to the menu item that was selected.
  int GetMenuItemIndexFromWPARAM(HMENU menu, WPARAM w_param) const {
    int count = GetMenuItemCount(menu);
    // For normal command menu items, Windows passes a command id as the LOWORD
    // of WPARAM for WM_MENUSELECT. We need to walk forward through the menu
    // items to find an item with a matching ID. Ugh!
    for (int i = 0; i < count; ++i) {
      MENUITEMINFO mii = {0};
      mii.cbSize = sizeof(mii);
      mii.fMask = MIIM_ID;
      GetMenuItemInfo(menu, i, MF_BYPOSITION, &mii);
      if (mii.wID == w_param)
        return i;
    }
    // If we didn't find a matching command ID, this means a submenu has been
    // selected instead, and rather than passing a command ID in
    // LOWORD(w_param), Windows has actually passed us a position, so we just
    // return it.
    return w_param;
  }

  // Called when the user selects a specific item.
  void OnMenuCommand(int position, HMENU menu) {
    GetNativeMenuWinFromHMENU(menu)->model_->ActivatedAt(position);
  }

  // Called as the user moves their mouse or arrows through the contents of the
  // menu.
  void OnMenuSelect(WPARAM w_param, HMENU menu) {
    int position = GetMenuItemIndexFromWPARAM(menu, w_param);
    if (position >= 0)
      GetNativeMenuWinFromHMENU(menu)->model_->HighlightChangedTo(position);
  }

  bool ProcessWindowMessage(HWND window,
                            UINT message,
                            WPARAM w_param,
                            LPARAM l_param,
                            LRESULT* l_result) {
    switch (message) {
      case WM_MENUCOMMAND:
        OnMenuCommand(w_param, reinterpret_cast<HMENU>(l_param));
        *l_result = 0;
        return true;
      case WM_MENUSELECT:
        OnMenuSelect(LOWORD(w_param), reinterpret_cast<HMENU>(l_param));
        *l_result = 0;
        return true;
      // TODO(beng): bring over owner draw from old menu system.
    }
    return false;
  }

  static LRESULT CALLBACK MenuHostWindowProc(HWND window,
                                             UINT message,
                                             WPARAM w_param,
                                             LPARAM l_param) {
    MenuHostWindow* host =
        reinterpret_cast<MenuHostWindow*>(GetProp(window, kMenuHostWindowKey));
    LRESULT l_result = 0;
    if (!host || !host->ProcessWindowMessage(window, message, w_param, l_param,
                                             &l_result)) {
      return DefWindowProc(window, message, w_param, l_param);
    }
    return l_result;
  }

  HWND hwnd_;

  DISALLOW_COPY_AND_ASSIGN(MenuHostWindow);
};

// static
const wchar_t* NativeMenuWin::MenuHostWindow::kWindowClassName =
    L"ViewsMenuHostWindow";

const wchar_t* NativeMenuWin::MenuHostWindow::kMenuHostWindowKey =
    L"__MENU_HOST_WINDOW__";


////////////////////////////////////////////////////////////////////////////////
// NativeMenuWin, public:

NativeMenuWin::NativeMenuWin(Menu2Model* model, HWND system_menu_for)
    : model_(model),
      menu_(NULL),
      owner_draw_(false),
      system_menu_for_(system_menu_for),
      first_item_index_(0) {
}

NativeMenuWin::~NativeMenuWin() {
  STLDeleteContainerPointers(items_.begin(), items_.end());
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuWin, MenuWrapper implementation:

void NativeMenuWin::RunMenuAt(const gfx::Point& point, int alignment) {
  CreateHostWindow();
  UpdateStates();
  UINT flags = TPM_LEFTBUTTON | TPM_RECURSE;
  flags |= GetAlignmentFlags(alignment);
  // Command dispatch is done through WM_MENUCOMMAND, handled by the host
  // window.
  TrackPopupMenuEx(menu_, flags, point.x(), point.y(), host_window_->hwnd(),
                   NULL);
}

void NativeMenuWin::CancelMenu() {
  EndMenu();
}

void NativeMenuWin::Rebuild() {
  ResetNativeMenu();
  owner_draw_ = model_->HasIcons();
  first_item_index_ = model_->GetFirstItemIndex(GetNativeMenu());
  for (int menu_index = first_item_index_;
        menu_index < first_item_index_ + model_->GetItemCount(); ++menu_index) {
    int model_index = menu_index - first_item_index_;
    if (model_->GetTypeAt(model_index) == Menu2Model::TYPE_SEPARATOR)
      AddSeparatorItemAt(menu_index, model_index);
    else
      AddMenuItemAt(menu_index, model_index);
  }
}

void NativeMenuWin::UpdateStates() {
  // A depth-first walk of the menu items, updating states.
  for (int menu_index = first_item_index_;
       menu_index < first_item_index_ + model_->GetItemCount(); ++menu_index) {
    int model_index = menu_index - first_item_index_;
    SetMenuItemState(menu_index, model_->IsEnabledAt(model_index),
                     model_->IsItemCheckedAt(model_index), false);
    if (model_->IsLabelDynamicAt(model_index)) {
      SetMenuItemLabel(menu_index, model_index,
                       model_->GetLabelAt(model_index));
    }
    Menu2* submenu = items_.at(model_index)->submenu.get();
    if (submenu)
      submenu->UpdateStates();
  }
}

gfx::NativeMenu NativeMenuWin::GetNativeMenu() const {
  return menu_;
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuWin, private:

bool NativeMenuWin::IsSeparatorItemAt(int menu_index) const {
  MENUITEMINFO mii = {0};
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_FTYPE;
  GetMenuItemInfo(menu_, menu_index, MF_BYPOSITION, &mii);
  return !!(mii.fType & MF_SEPARATOR);
}

void NativeMenuWin::AddMenuItemAt(int menu_index, int model_index) {
  MENUITEMINFO mii = {0};
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_DATA;
  if (!owner_draw_)
    mii.fType = MFT_STRING;
  else
    mii.fType = MFT_OWNERDRAW;
  mii.dwItemData = reinterpret_cast<ULONG_PTR>(this);

  ItemData* item_data = new ItemData;
  Menu2Model::ItemType type = model_->GetTypeAt(model_index);
  if (type == Menu2Model::TYPE_SUBMENU) {
    item_data->submenu.reset(new Menu2(model_->GetSubmenuModelAt(model_index)));
    mii.fMask |= MIIM_SUBMENU;
    mii.hSubMenu = item_data->submenu->GetNativeMenu();
  } else {
    if (type == Menu2Model::TYPE_RADIO)
      mii.fType |= MFT_RADIOCHECK;
    mii.wID = model_->GetCommandIdAt(model_index);
  }
  items_.insert(items_.begin() + model_index, item_data);
  UpdateMenuItemInfoForString(&mii, model_index,
                              model_->GetLabelAt(model_index));
  InsertMenuItem(menu_, menu_index, TRUE, &mii);
}

void NativeMenuWin::AddSeparatorItemAt(int menu_index, int model_index) {
  MENUITEMINFO mii = {0};
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_FTYPE;
  mii.fType = MFT_SEPARATOR;
  // Insert a dummy entry into our label list so we can index directly into it
  // using item indices if need be.
  items_.insert(items_.begin() + model_index, new ItemData);
  InsertMenuItem(menu_, menu_index, TRUE, &mii);
}

void NativeMenuWin::SetMenuItemState(int menu_index, bool enabled, bool checked,
                                     bool is_default) {
  if (IsSeparatorItemAt(menu_index))
    return;

  UINT state = enabled ? MFS_ENABLED : MFS_DISABLED;
  if (checked)
    state |= MFS_CHECKED;
  if (is_default)
    state |= MFS_DEFAULT;

  MENUITEMINFO mii = {0};
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_STATE;
  mii.fState = state;
  SetMenuItemInfo(menu_, menu_index, MF_BYPOSITION, &mii);
}

void NativeMenuWin::SetMenuItemLabel(int menu_index,
                                     int model_index,
                                     const std::wstring& label) {
  if (IsSeparatorItemAt(menu_index))
    return;

  MENUITEMINFO mii = {0};
  mii.cbSize = sizeof(mii);
  UpdateMenuItemInfoForString(&mii, model_index, label);
  if (!owner_draw_)
    SetMenuItemInfo(menu_, menu_index, MF_BYPOSITION, &mii);
}

void NativeMenuWin::UpdateMenuItemInfoForString(
    MENUITEMINFO* mii,
    int model_index,
    const std::wstring& label) {
  std::wstring formatted = label;
  Menu2Model::ItemType type = model_->GetTypeAt(model_index);
  if (type != Menu2Model::TYPE_SUBMENU) {
    // Add accelerator details to the label if provided.
    views::Accelerator accelerator(0, false, false, false);
    if (model_->GetAcceleratorAt(model_index, &accelerator)) {
      formatted += L"\t";
      formatted += accelerator.GetShortcutText();
    }
  }

  // Update the owned string, since Windows will want us to keep this new
  // version around.
  items_[model_index]->label = formatted;

  // Windows only requires a pointer to the label string if it's going to be
  // doing the drawing.
  if (!owner_draw_) {
    mii->fMask |= MIIM_STRING;
    mii->dwTypeData =
        const_cast<wchar_t*>(items_.at(model_index)->label.c_str());
  }
}

NativeMenuWin* NativeMenuWin::GetMenuForCommandId(UINT command_id) const {
  // Menus can have nested submenus. In the views Menu system, each submenu is
  // wrapped in a NativeMenu instance, which may have a different model and
  // delegate from the parent menu. The trouble is, RunMenuAt is called on the
  // parent NativeMenuWin, and so it's not possible to assume that we can just
  // dispatch the command id returned by TrackPopupMenuEx to the parent's
  // delegate. For this reason, we stow a pointer on every menu item we create
  // to the NativeMenuWin that most closely contains it. Fortunately, Windows
  // provides GetMenuItemInfo, which can walk down the menu item tree from
  // the root |menu_| to find the data for a given item even if it's in a
  // submenu.
  MENUITEMINFO mii = {0};
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_DATA;
  GetMenuItemInfo(menu_, command_id, FALSE, &mii);
  return reinterpret_cast<NativeMenuWin*>(mii.dwItemData);
}

UINT NativeMenuWin::GetAlignmentFlags(int alignment) const {
  bool rtl = l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT;
  UINT alignment_flags = TPM_TOPALIGN;
  if (alignment == Menu2::ALIGN_TOPLEFT)
    alignment_flags |= TPM_LEFTALIGN;
  else if (alignment == Menu2::ALIGN_TOPRIGHT)
    alignment_flags |= TPM_RIGHTALIGN;
  return alignment_flags;
}

void NativeMenuWin::ResetNativeMenu() {
  if (IsWindow(system_menu_for_)) {
    if (menu_)
      GetSystemMenu(system_menu_for_, TRUE);
    menu_ = GetSystemMenu(system_menu_for_, FALSE);
  } else {
    if (menu_)
      DestroyMenu(menu_);
    menu_ = CreatePopupMenu();
    // Rather than relying on the return value of TrackPopupMenuEx, which is
    // always a command identifier, instead we tell the menu to notify us via
    // our host window and the WM_MENUCOMMAND message.
    MENUINFO mi = {0};
    mi.cbSize = sizeof(mi);
    mi.fMask = MIM_STYLE | MIM_MENUDATA;
    mi.dwStyle = MNS_NOTIFYBYPOS;
    mi.dwMenuData = reinterpret_cast<ULONG_PTR>(this);
    SetMenuInfo(menu_, &mi);
  }
}

void NativeMenuWin::CreateHostWindow() {
  // This only gets called from RunMenuAt, and as such there is only ever one
  // host window per menu hierarchy, no matter how many NativeMenuWin objects
  // exist wrapping submenus.
  if (!host_window_.get())
    host_window_.reset(new MenuHostWindow());
}

////////////////////////////////////////////////////////////////////////////////
// SystemMenuModel:

SystemMenuModel::SystemMenuModel(SimpleMenuModel::Delegate* delegate)
    : SimpleMenuModel(delegate) {
}

SystemMenuModel::~SystemMenuModel() {
}

int SystemMenuModel::GetFirstItemIndex(gfx::NativeMenu native_menu) const {
  // We allow insertions before last item (Close).
  return std::max(0, GetMenuItemCount(native_menu) - 1);
}

////////////////////////////////////////////////////////////////////////////////
// MenuWrapper, public:

// static
MenuWrapper* MenuWrapper::CreateWrapper(Menu2* menu) {
  return new NativeMenuWin(menu->model(), NULL);
}

}  // namespace views
