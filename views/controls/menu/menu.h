// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTROLS_MENU_VIEWS_MENU_H_
#define CONTROLS_MENU_VIEWS_MENU_H_

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#include "views/controls/menu/controller.h"

class SkBitmap;

namespace views {

class Accelerator;

class Menu {
 public:
  /////////////////////////////////////////////////////////////////////////////
  //
  // Delegate Interface
  //
  //  Classes implement this interface to tell the menu system more about each
  //  item as it is created.
  //
  /////////////////////////////////////////////////////////////////////////////
  class Delegate : public Controller {
   public:
    virtual ~Delegate() { }

    // Whether or not an item should be shown as checked.
    virtual bool IsItemChecked(int id) const {
      return false;
    }

    // Whether or not an item should be shown as the default (using bold).
    // There can only be one default menu item.
    virtual bool IsItemDefault(int id) const {
      return false;
    }

    // The string shown for the menu item.
    virtual std::wstring GetLabel(int id) const {
      return std::wstring();
    }

    // The delegate needs to implement this function if it wants to display
    // the shortcut text next to each menu item. If there is an accelerator
    // for a given item id, the implementor must return it.
    virtual bool GetAcceleratorInfo(int id, views::Accelerator* accel) {
      return false;
    }

    // The icon shown for the menu item.
    virtual const SkBitmap& GetIcon(int id) const {
      return GetEmptyIcon();
    }

    // The number of items to show in the menu
    virtual int GetItemCount() const {
      return 0;
    }

    // Whether or not an item is a separator.
    virtual bool IsItemSeparator(int id) const {
      return false;
    }

    // Shows the context menu with the specified id. This is invoked when the
    // user does the appropriate gesture to show a context menu. The id
    // identifies the id of the menu to show the context menu for.
    // is_mouse_gesture is true if this is the result of a mouse gesture.
    // If this is not the result of a mouse gesture x/y is the recommended
    // location to display the content menu at. In either case, x/y is in
    // screen coordinates.
    virtual void ShowContextMenu(Menu* source,
                                 int id,
                                 int x,
                                 int y,
                                 bool is_mouse_gesture) {
    }

    // Whether an item has an icon.
    virtual bool HasIcon(int id) const {
      return false;
    }

    // Notification that the menu is about to be popped up.
    virtual void MenuWillShow() {
    }

    // Whether to create a right-to-left menu. The default implementation
    // returns true if the locale's language is a right-to-left language (such
    // as Hebrew) and false otherwise. This is generally the right behavior
    // since there is no reason to show left-to-right menus for right-to-left
    // locales. However, subclasses can override this behavior so that the menu
    // is a right-to-left menu only if the view's layout is right-to-left
    // (since the view can use a different layout than the locale's language
    // layout).
    virtual bool IsRightToLeftUILayout() const;

    // Controller
    virtual bool SupportsCommand(int id) const {
      return true;
    }
    virtual bool IsCommandEnabled(int id) const {
      return true;
    }
    virtual bool GetContextualLabel(int id, std::wstring* out) const {
      return false;
    }
    virtual void ExecuteCommand(int id) {
    }

   protected:
    // Returns an empty icon.
    const SkBitmap& GetEmptyIcon() const;
  };

  // This class is a helper that simply wraps a controller and forwards all
  // state and execution actions to it.  Use this when you're not defining your
  // own custom delegate, but just hooking a context menu to some existing
  // controller elsewhere.
  class BaseControllerDelegate : public Delegate {
   public:
    explicit BaseControllerDelegate(Controller* wrapped)
      : controller_(wrapped) {
    }

    // Overridden from Menu::Delegate
    virtual bool SupportsCommand(int id) const {
      return controller_->SupportsCommand(id);
    }
    virtual bool IsCommandEnabled(int id) const {
      return controller_->IsCommandEnabled(id);
    }
    virtual void ExecuteCommand(int id) {
      controller_->ExecuteCommand(id);
    }
    virtual bool GetContextualLabel(int id, std::wstring* out) const {
      return controller_->GetContextualLabel(id, out);
    }

   private:
    // The internal controller that we wrap to forward state and execution
    // actions to.
    Controller* controller_;

    DISALLOW_COPY_AND_ASSIGN(BaseControllerDelegate);
  };

  // How this popup should align itself relative to the point it is run at.
  enum AnchorPoint {
    TOPLEFT,
    TOPRIGHT
  };

  // Different types of menu items
  enum MenuItemType {
    NORMAL,
    CHECKBOX,
    RADIO,
    SEPARATOR
  };

  // Construct a Menu using the specified controller to determine command
  // state.
  // delegate     A Menu::Delegate implementation that provides more
  //              information about the Menu presentation.
  // anchor       An alignment hint for the popup menu.
  // owner        The window that the menu is being brought up relative
  //              to. Not actually used for anything but must not be
  //              NULL.
  Menu(Delegate* delegate, AnchorPoint anchor);
  Menu();
  virtual ~Menu();

  static Menu* Create(Delegate* delegate,
                      AnchorPoint anchor,
                      gfx::NativeView parent);

  // Creates a new menu with the contents of the system menu for the given
  // parent window. The caller owns the returned pointer.
  static Menu* GetSystemMenu(gfx::NativeWindow parent);

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }
  Delegate* delegate() const { return delegate_; }

  AnchorPoint anchor() const { return anchor_; }

  // Adds an item to this menu.
  // item_id    The id of the item, used to identify it in delegate callbacks
  //            or (if delegate is NULL) to identify the command associated
  //            with this item with the controller specified in the ctor. Note
  //            that this value should not be 0 as this has a special meaning
  //            ("NULL command, no item selected")
  // label      The text label shown.
  // type       The type of item.
  void AppendMenuItem(int item_id,
                      const std::wstring& label,
                      MenuItemType type);
  void AddMenuItem(int index,
                   int item_id,
                   const std::wstring& label,
                   MenuItemType type);

  // Append a submenu to this menu.
  // The returned pointer is owned by this menu.
  Menu* AppendSubMenu(int item_id,
                      const std::wstring& label);
  Menu* AddSubMenu(int index, int item_id, const std::wstring& label);

  // Append a submenu with an icon to this menu
  // The returned pointer is owned by this menu.
  // Unless the icon is empty, calling this function forces the Menu class
  // to draw the menu, instead of relying on Windows.
  Menu* AppendSubMenuWithIcon(int item_id,
                              const std::wstring& label,
                              const SkBitmap& icon);
  virtual Menu* AddSubMenuWithIcon(int index,
                                   int item_id,
                                   const std::wstring& label,
                                   const SkBitmap& icon) = 0;

  // This is a convenience for standard text label menu items where the label
  // is provided with this call.
  void AppendMenuItemWithLabel(int item_id, const std::wstring& label);
  void AddMenuItemWithLabel(int index, int item_id, const std::wstring& label);

  // This is a convenience for text label menu items where the label is
  // provided by the delegate.
  void AppendDelegateMenuItem(int item_id);
  void AddDelegateMenuItem(int index, int item_id);

  // Adds a separator to this menu
  void AppendSeparator();
  virtual void AddSeparator(int index) = 0;

  // Appends a menu item with an icon. This is for the menu item which
  // needs an icon. Calling this function forces the Menu class to draw
  // the menu, instead of relying on Windows.
  void AppendMenuItemWithIcon(int item_id,
                              const std::wstring& label,
                              const SkBitmap& icon);
  virtual void AddMenuItemWithIcon(int index,
                                   int item_id,
                                   const std::wstring& label,
                                   const SkBitmap& icon);

  // Enables or disables the item with the specified id.
  virtual void EnableMenuItemByID(int item_id, bool enabled) = 0;
  virtual void EnableMenuItemAt(int index, bool enabled) = 0;

  // Sets menu label at specified index.
  virtual void SetMenuLabel(int item_id, const std::wstring& label) = 0;

  // Sets an icon for an item with a given item_id. Calling this function
  // also forces the Menu class to draw the menu, instead of relying on Windows.
  // Returns false if the item with |item_id| is not found.
  virtual bool SetIcon(const SkBitmap& icon, int item_id) = 0;

  // Shows the menu, blocks until the user dismisses the menu or selects an
  // item, and executes the command for the selected item (if any).
  // Warning: Blocking call. Will implicitly run a message loop.
  virtual void RunMenuAt(int x, int y) = 0;

  // Cancels the menu.
  virtual void Cancel() = 0;

  // Returns the number of menu items.
  virtual int ItemCount() = 0;

#if defined(OS_WIN)
  // Returns the underlying menu handle
  virtual HMENU GetMenuHandle() const = 0;
#endif  // defined(OS_WIN)

 protected:
  explicit Menu(Menu* parent);

  virtual void AddMenuItemInternal(int index,
                                   int item_id,
                                   const std::wstring& label,
                                   const SkBitmap& icon,
                                   MenuItemType type) = 0;

 private:
  // The delegate that is being used to get information about the presentation.
  Delegate* delegate_;

  // How this popup menu should be aligned relative to the point it is run at.
  AnchorPoint anchor_;

  DISALLOW_COPY_AND_ASSIGN(Menu);
};

}  // namespace views

#endif  // CONTROLS_MENU_VIEWS_MENU_H_
