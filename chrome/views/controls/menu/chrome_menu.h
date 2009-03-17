// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_CONTROLS_MENU_CHROME_MENU_H_
#define CHROME_VIEWS_CONTROLS_MENU_CHROME_MENU_H_

#include <list>

#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "chrome/common/drag_drop_types.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/views/controls/menu/controller.h"
#include "chrome/views/view.h"
#include "skia/include/SkBitmap.h"

namespace views {

class WidgetWin;
class MenuController;
class MenuItemView;
class SubmenuView;

namespace {
class MenuHost;
class MenuHostRootView;
class MenuScrollTask;
class MenuScrollViewContainer;
}

// MenuDelegate --------------------------------------------------------------

// Delegate for the menu.

class MenuDelegate : Controller {
 public:
  // Used during drag and drop to indicate where the drop indicator should
  // be rendered.
  enum DropPosition {
    // Indicates a drop is not allowed here.
    DROP_NONE,

    // Indicates the drop should occur before the item.
    DROP_BEFORE,

    // Indicates the drop should occur after the item.
    DROP_AFTER,

    // Indicates the drop should occur on the item.
    DROP_ON
  };

  // Whether or not an item should be shown as checked.
  // TODO(sky): need checked support.
  virtual bool IsItemChecked(int id) const {
    return false;
  }

  // The string shown for the menu item. This is only invoked when an item is
  // added with an empty label.
  virtual std::wstring GetLabel(int id) const {
    return std::wstring();
  }

  // Shows the context menu with the specified id. This is invoked when the
  // user does the appropriate gesture to show a context menu. The id
  // identifies the id of the menu to show the context menu for.
  // is_mouse_gesture is true if this is the result of a mouse gesture.
  // If this is not the result of a mouse gesture x/y is the recommended
  // location to display the content menu at. In either case, x/y is in
  // screen coordinates.
  // Returns true if a context menu was displayed, otherwise false
  virtual bool ShowContextMenu(MenuItemView* source,
                               int id,
                               int x,
                               int y,
                               bool is_mouse_gesture) {
    return false;
  }

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

  // Executes the specified command. mouse_event_flags give the flags of the
  // mouse event that triggered this to be invoked (views::MouseEvent
  // flags). mouse_event_flags is 0 if this is triggered by a user gesture
  // other than a mouse event.
  virtual void ExecuteCommand(int id, int mouse_event_flags) {
    ExecuteCommand(id);
  }

  // Returns true if the specified mouse event is one the user can use
  // to trigger, or accept, the mouse. Defaults to left or right mouse buttons.
  virtual bool IsTriggerableEvent(const MouseEvent& e) {
    return e.IsLeftMouseButton() || e.IsRightMouseButton();
  }

  // Invoked to determine if drops can be accepted for a submenu. This is
  // ONLY invoked for menus that have submenus and indicates whether or not
  // a drop can occur on any of the child items of the item. For example,
  // consider the following menu structure:
  //
  // A
  //   B
  //   C
  //
  // Where A has a submenu with children B and C. This is ONLY invoked for
  // A, not B and C.
  //
  // To restrict which children can be dropped on override GetDropOperation.
  virtual bool CanDrop(MenuItemView* menu, const OSExchangeData& data) {
    return false;
  }

  // Returns the drop operation for the specified target menu item. This is
  // only invoked if CanDrop returned true for the parent menu. position
  // is set based on the location of the mouse, reset to specify a different
  // position.
  //
  // If a drop should not be allowed, returned DragDropTypes::DRAG_NONE.
  virtual int GetDropOperation(MenuItemView* item,
                               const DropTargetEvent& event,
                               DropPosition* position) {
    NOTREACHED() << "If you override CanDrop, you need to override this too";
    return DragDropTypes::DRAG_NONE;
  }

  // Invoked to perform the drop operation. This is ONLY invoked if
  // canDrop returned true for the parent menu item, and GetDropOperation
  // returned an operation other than DragDropTypes::DRAG_NONE.
  //
  // menu indicates the menu the drop occurred on.
  virtual int OnPerformDrop(MenuItemView* menu,
                            DropPosition position,
                            const DropTargetEvent& event) {
    NOTREACHED() << "If you override CanDrop, you need to override this too";
    return DragDropTypes::DRAG_NONE;
  }

  // Invoked to determine if it is possible for the user to drag the specified
  // menu item.
  virtual bool CanDrag(MenuItemView* menu) {
    return false;
  }

  // Invoked to write the data for a drag operation to data. sender is the
  // MenuItemView being dragged.
  virtual void WriteDragData(MenuItemView* sender, OSExchangeData* data) {
    NOTREACHED() << "If you override CanDrag, you must override this too.";
  }

  // Invoked to determine the drag operations for a drag session of sender.
  // See DragDropTypes for possible values.
  virtual int GetDragOperations(MenuItemView* sender) {
    NOTREACHED() << "If you override CanDrag, you must override this too.";
    return 0;
  }

  // Notification the menu has closed. This is only sent when running the
  // menu for a drop.
  virtual void DropMenuClosed(MenuItemView* menu) {
  }

  // Notification that the user has highlighted the specified item.
  virtual void SelectionChanged(MenuItemView* menu) {
  }
};

// MenuItemView --------------------------------------------------------------

// MenuItemView represents a single menu item with a label and optional icon.
// Each MenuItemView may also contain a submenu, which in turn may contain
// any number of child MenuItemViews.
//
// To use a menu create an initial MenuItemView using the constructor that
// takes a MenuDelegate, then create any number of child menu items by way
// of the various AddXXX methods.
//
// MenuItemView is itself a View, which means you can add Views to each
// MenuItemView. This normally NOT want you want, rather add other child Views
// to the submenu of the MenuItemView.
//
// There are two ways to show a MenuItemView:
// 1. Use RunMenuAt. This blocks the caller, executing the selected command
//    on success.
// 2. Use RunMenuForDropAt. This is intended for use during a drop session
//    and does NOT block the caller. Instead the delegate is notified when the
//    menu closes via the DropMenuClosed method.

class MenuItemView : public View {
  friend class MenuController;

 public:
  // ID used to identify menu items.
  static const int kMenuItemViewID;

  // If true SetNestableTasksAllowed(true) is invoked before MessageLoop::Run
  // is invoked. This is only useful for testing and defaults to false.
  static bool allow_task_nesting_during_run_;

  // Different types of menu items.
  enum Type {
    NORMAL,
    SUBMENU,
    CHECKBOX,
    RADIO,
    SEPARATOR
  };

  // Where the menu should be anchored to.
  enum AnchorPosition {
    TOPLEFT,
    TOPRIGHT
  };

  // Constructor for use with the top level menu item. This menu is never
  // shown to the user, rather its use as the parent for all menu items.
  explicit MenuItemView(MenuDelegate* delegate);

  virtual ~MenuItemView();

  // Run methods. See description above class for details. Both Run methods take
  // a rectangle, which is used to position the menu. |has_mnemonics| indicates
  // whether the items have mnemonics. Mnemonics are identified by way of the
  // character following the '&'.
  void RunMenuAt(HWND parent,
                 const gfx::Rect& bounds,
                 AnchorPosition anchor,
                 bool has_mnemonics);
  void RunMenuForDropAt(HWND parent,
                        const gfx::Rect& bounds,
                        AnchorPosition anchor);

  // Hides and cancels the menu. This does nothing if the menu is not open.
  void Cancel();

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
                      Type type) {
    AppendMenuItemInternal(item_id, label, SkBitmap(), type);
  }

  // Append a submenu to this menu.
  // The returned pointer is owned by this menu.
  MenuItemView* AppendSubMenu(int item_id,
                              const std::wstring& label) {
    return AppendMenuItemInternal(item_id, label, SkBitmap(), SUBMENU);
  }

  // Append a submenu with an icon to this menu.
  // The returned pointer is owned by this menu.
  MenuItemView* AppendSubMenuWithIcon(int item_id,
                                      const std::wstring& label,
                                      const SkBitmap& icon) {
    return AppendMenuItemInternal(item_id, label, icon, SUBMENU);
  }

  // This is a convenience for standard text label menu items where the label
  // is provided with this call.
  void AppendMenuItemWithLabel(int item_id,
                               const std::wstring& label) {
    AppendMenuItem(item_id, label, NORMAL);
  }

  // This is a convenience for text label menu items where the label is
  // provided by the delegate.
  void AppendDelegateMenuItem(int item_id) {
    AppendMenuItem(item_id, std::wstring(), NORMAL);
  }

  // Adds a separator to this menu
  void AppendSeparator() {
    AppendMenuItemInternal(0, std::wstring(), SkBitmap(), SEPARATOR);
  }

  // Appends a menu item with an icon. This is for the menu item which
  // needs an icon. Calling this function forces the Menu class to draw
  // the menu, instead of relying on Windows.
  void AppendMenuItemWithIcon(int item_id,
                              const std::wstring& label,
                              const SkBitmap& icon) {
    AppendMenuItemInternal(item_id, label, icon, NORMAL);
  }

  // Returns the view that contains child menu items. If the submenu has
  // not been creates, this creates it.
  virtual SubmenuView* CreateSubmenu();

  // Returns true if this menu item has a submenu.
  virtual bool HasSubmenu() const { return (submenu_ != NULL); }

  // Returns the view containing child menu items.
  virtual SubmenuView* GetSubmenu() const { return submenu_; }

  // Returns the parent menu item.
  MenuItemView* GetParentMenuItem() const { return parent_menu_item_; }

  // Sets the font.
  void SetFont(const ChromeFont& font) { font_ = font; }

  // Sets the title
  void SetTitle(const std::wstring& title) {
    title_ = title;
  }

  // Returns the title.
  const std::wstring& GetTitle() const { return title_; }

  // Sets whether this item is selected. This is invoked as the user moves
  // the mouse around the menu while open.
  void SetSelected(bool selected);

  // Returns true if the item is selected.
  bool IsSelected() const { return selected_; }

  // Sets the icon for the descendant identified by item_id.
  void SetIcon(const SkBitmap& icon, int item_id);

  // Sets the icon of this menu item.
  void SetIcon(const SkBitmap& icon);

  // Returns the icon.
  const SkBitmap& GetIcon() const { return icon_; }

  // Sets the command id of this menu item.
  void SetCommand(int command) { command_ = command; }

  // Returns the command id of this item.
  int GetCommand() const { return command_; }

  // Paints the menu item.
  virtual void Paint(ChromeCanvas* canvas);

  // Returns the preferred size of this item.
  virtual gfx::Size GetPreferredSize();

  // Returns the object responsible for controlling showing the menu.
  MenuController* GetMenuController();

  // Returns the delegate. This returns the delegate of the root menu item.
  MenuDelegate* GetDelegate();

  // Returns the root parent, or this if this has no parent.
  MenuItemView* GetRootMenuItem();

  // Returns the mnemonic for this MenuItemView, or 0 if this MenuItemView
  // doesn't have a mnemonic.
  wchar_t GetMnemonic();

  // Do we have icons? This only has effect on the top menu. Turning this on
  // makes the menus slightly wider and taller.
  void set_has_icons(bool has_icons) {
    has_icons_ = has_icons;
  }

 protected:
  // Creates a MenuItemView. This is used by the various AddXXX methods.
  MenuItemView(MenuItemView* parent, int command, Type type);

 private:
  // Called by the two constructors to initialize this menu item.
  void Init(MenuItemView* parent,
            int command,
            MenuItemView::Type type,
            MenuDelegate* delegate);

  // All the AddXXX methods funnel into this.
  MenuItemView* AppendMenuItemInternal(int item_id,
                                       const std::wstring& label,
                                       const SkBitmap& icon,
                                       Type type);

  // Returns the descendant with the specified command.
  MenuItemView* GetDescendantByID(int id);

  // Invoked by the MenuController when the menu closes as the result of
  // drag and drop run.
  void DropMenuClosed(bool notify_delegate);

  // The RunXXX methods call into this to set up the necessary state before
  // running.
  void PrepareForRun(bool has_mnemonics);

  // Returns the flags passed to DrawStringInt.
  int GetDrawStringFlags();

  // If this menu item has no children a child is added showing it has no
  // children. Otherwise AddEmtpyMenuIfNecessary is recursively invoked on
  // child menu items that have children.
  void AddEmptyMenus();

  // Undoes the work of AddEmptyMenus.
  void RemoveEmptyMenus();

  // Given bounds within our View, this helper routine mirrors the bounds if
  // necessary.
  void AdjustBoundsForRTLUI(RECT* rect) const;

  // Actual paint implementation. If for_drag is true, portions of the menu
  // are not rendered.
  void Paint(ChromeCanvas* canvas, bool for_drag);

  // Destroys the window used to display this menu and recursively destroys
  // the windows used to display all descendants.
  void DestroyAllMenuHosts();

  // Returns the various margins.
  int GetTopMargin();
  int GetBottomMargin();

  // The delegate. This is only valid for the root menu item. You shouldn't
  // use this directly, instead use GetDelegate() which walks the tree as
  // as necessary.
  MenuDelegate* delegate_;

  // Returns the controller for the run operation, or NULL if the menu isn't
  // showing.
  MenuController* controller_;

  // Used to detect when Cancel was invoked.
  bool canceled_;

  // Our parent.
  MenuItemView* parent_menu_item_;

  // Type of menu. NOTE: MenuItemView doesn't itself represent SEPARATOR,
  // that is handled by an entirely different view class.
  Type type_;

  // Whether we're selected.
  bool selected_;

  // Command id.
  int command_;

  // Submenu, created via CreateSubmenu.
  SubmenuView* submenu_;

  // Font.
  ChromeFont font_;

  // Title.
  std::wstring title_;

  // Icon.
  SkBitmap icon_;

  // Does the title have a mnemonic?
  bool has_mnemonics_;

  bool has_icons_;

  DISALLOW_EVIL_CONSTRUCTORS(MenuItemView);
};

// SubmenuView ----------------------------------------------------------------

// SubmenuView is the parent of all menu items.
//
// SubmenuView has the following responsibilities:
// . It positions and sizes all child views (any type of View may be added,
//   not just MenuItemViews).
// . Forwards the appropriate events to the MenuController. This allows the
//   MenuController to update the selection as the user moves the mouse around.
// . Renders the drop indicator during a drop operation.
// . Shows and hides the window (a WidgetWin) when the menu is shown on
//   screen.
//
// SubmenuView is itself contained in a MenuScrollViewContainer.
// MenuScrollViewContainer handles showing as much of the SubmenuView as the
// screen allows. If the SubmenuView is taller than the screen, scroll buttons
// are provided that allow the user to see all the menu items.
class SubmenuView : public View {
 public:
  // Creates a SubmenuView for the specified menu item.
  explicit SubmenuView(MenuItemView* parent);
  ~SubmenuView();

  // Returns the number of child views that are MenuItemViews.
  // MenuItemViews are identified by ID.
  int GetMenuItemCount();

  // Returns the MenuItemView at the specified index.
  MenuItemView* GetMenuItemAt(int index);

  // Positions and sizes the child views. This tiles the views vertically,
  // giving each child the available width.
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

  // View method. Overriden to schedule a paint. We do this so that when
  // scrolling occurs, everything is repainted correctly.
  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);

  // Painting.
  void PaintChildren(ChromeCanvas* canvas);

  // Drag and drop methods. These are forwarded to the MenuController.
  virtual bool CanDrop(const OSExchangeData& data);
  virtual void OnDragEntered(const DropTargetEvent& event);
  virtual int OnDragUpdated(const DropTargetEvent& event);
  virtual void OnDragExited();
  virtual int OnPerformDrop(const DropTargetEvent& event);

  // Scrolls on menu item boundaries.
  virtual bool OnMouseWheel(const MouseWheelEvent& e);

  // Returns true if the menu is showing.
  bool IsShowing();

  // Shows the menu at the specified location. Coordinates are in screen
  // coordinates. max_width gives the max width the view should be.
  void ShowAt(HWND parent, const gfx::Rect& bounds, bool do_capture);

  // Closes the menu, destroying the host.
  void Close();

  // Hides the hosting window.
  //
  // The hosting window is hidden first, then deleted (Close) when the menu is
  // done running. This is done to avoid deletion ordering dependencies. In
  // particular, during drag and drop (and when a modal dialog is shown as
  // a result of choosing a context menu) it is possible that an event is
  // being processed by the host, so that host is on the stack when we need to
  // close the window. If we closed the window immediately (and deleted it),
  // when control returned back to host we would crash as host was deleted.
  void Hide();

  // If mouse capture was grabbed, it is released. Does nothing if mouse was
  // not captured.
  void ReleaseCapture();

  // Returns the parent menu item we're showing children for.
  MenuItemView* GetMenuItem() const { return parent_menu_item_; }

  // Overriden to return true. This prevents tab from doing anything.
  virtual bool CanProcessTabKeyEvents() { return true; }

  // Set the drop item and position.
  void SetDropMenuItem(MenuItemView* item,
                       MenuDelegate::DropPosition position);

  // Returns whether the selection should be shown for the specified item.
  // The selection is NOT shown during drag and drop when the drop is over
  // the menu.
  bool GetShowSelection(MenuItemView* item);

  // Returns the container for the SubmenuView.
  MenuScrollViewContainer* GetScrollViewContainer();

  // Returns the host of the menu. Returns NULL if not showing.
  MenuHost* host() const { return host_; }

 private:
  // Paints the drop indicator. This is only invoked if item is non-NULL and
  // position is not DROP_NONE.
  void PaintDropIndicator(ChromeCanvas* canvas,
                          MenuItemView* item,
                          MenuDelegate::DropPosition position);

  void SchedulePaintForDropIndicator(MenuItemView* item,
                                     MenuDelegate::DropPosition position);

  // Calculates the location of th edrop indicator.
  gfx::Rect CalculateDropIndicatorBounds(MenuItemView* item,
                                         MenuDelegate::DropPosition position);

  // Parent menu item.
  MenuItemView* parent_menu_item_;

  // WidgetWin subclass used to show the children.
  MenuHost* host_;

  // If non-null, indicates a drop is in progress and drop_item is the item
  // the drop is over.
  MenuItemView* drop_item_;

  // Position of the drop.
  MenuDelegate::DropPosition drop_position_;

  // Ancestor of the SubmenuView, lazily created.
  MenuScrollViewContainer* scroll_view_container_;

  DISALLOW_EVIL_CONSTRUCTORS(SubmenuView);
};

// MenuController -------------------------------------------------------------

// MenuController manages showing, selecting and drag/drop for menus.
// All relevant events are forwarded to the MenuController from SubmenuView
// and MenuHost.

class MenuController : public MessageLoopForUI::Dispatcher {
 public:
  friend class MenuHostRootView;
  friend class MenuItemView;
  friend class MenuScrollTask;

  // If a menu is currently active, this returns the controller for it.
  static MenuController* GetActiveInstance();

  // Runs the menu at the specified location. If the menu was configured to
  // block, the selected item is returned. If the menu does not block this
  // returns NULL immediately.
  MenuItemView* Run(HWND parent,
                    MenuItemView* root,
                    const gfx::Rect& bounds,
                    MenuItemView::AnchorPosition position,
                    int* mouse_event_flags);

  // Whether or not Run blocks.
  bool IsBlockingRun() const { return blocking_run_; }

  // Sets the selection to menu_item, a value of NULL unselects everything.
  // If open_submenu is true and menu_item has a submenu, the submenu is shown.
  // If update_immediately is true, submenus are opened immediately, otherwise
  // submenus are only opened after a timer fires.
  //
  // Internally this updates pending_state_ immediatley, and if
  // update_immediately is true, CommitPendingSelection is invoked to
  // show/hide submenus and update state_.
  void SetSelection(MenuItemView* menu_item,
                    bool open_submenu,
                    bool update_immediately);

  // Cancels the current Run. If all is true, any nested loops are canceled
  // as well. This immediately hides all menus.
  void Cancel(bool all);

  // An alternative to Cancel(true) that can be used with a OneShotTimer.
  void CancelAll() { return Cancel(true); }

  // Various events, forwarded from the submenu.
  //
  // NOTE: the coordinates of the events are in that of the
  // MenuScrollViewContainer.
  void OnMousePressed(SubmenuView* source, const MouseEvent& event);
  void OnMouseDragged(SubmenuView* source, const MouseEvent& event);
  void OnMouseReleased(SubmenuView* source, const MouseEvent& event);
  void OnMouseMoved(SubmenuView* source, const MouseEvent& event);
  void OnMouseEntered(SubmenuView* source, const MouseEvent& event);
  bool CanDrop(SubmenuView* source, const OSExchangeData& data);
  void OnDragEntered(SubmenuView* source, const DropTargetEvent& event);
  int OnDragUpdated(SubmenuView* source, const DropTargetEvent& event);
  void OnDragExited(SubmenuView* source);
  int OnPerformDrop(SubmenuView* source, const DropTargetEvent& event);

  // Invoked from the scroll buttons of the MenuScrollViewContainer.
  void OnDragEnteredScrollButton(SubmenuView* source, bool is_up);
  void OnDragExitedScrollButton(SubmenuView* source);

 private:
  // Tracks selection information.
  struct State {
    State() : item(NULL), submenu_open(false) {}

    // The selected menu item.
    MenuItemView* item;

    // If item has a submenu this indicates if the submenu is showing.
    bool submenu_open;

    // Bounds passed to the run menu. Used for positioning the first menu.
    gfx::Rect initial_bounds;

    // Position of the initial menu.
    MenuItemView::AnchorPosition anchor;

    // The direction child menus have opened in.
    std::list<bool> open_leading;

    // Bounds for the monitor we're showing on.
    gfx::Rect monitor_bounds;
  };

  // Used by GetMenuPartByScreenCoordinate to indicate the menu part at a
  // particular location.
  struct MenuPart {
    // Type of part.
    enum Type {
      NONE,
      MENU_ITEM,
      SCROLL_UP,
      SCROLL_DOWN
    };

    MenuPart() : type(NONE), menu(NULL), submenu(NULL) {}

    // Convenience for testing type == SCROLL_DOWN or type == SCROLL_UP.
    bool is_scroll() const { return type == SCROLL_DOWN || type == SCROLL_UP; }

    // Type of part.
    Type type;

    // If type is MENU_ITEM, this is the menu item the mouse is over, otherwise
    // this is NULL.
    // NOTE: if type is MENU_ITEM and the mouse is not over a valid menu item
    //       but is over a menu (for example, the mouse is over a separator or
    //       empty menu), this is NULL.
    MenuItemView* menu;

    // If type is SCROLL_*, this is the submenu the mouse is over.
    SubmenuView* submenu;
  };

  // Sets the active MenuController.
  static void SetActiveInstance(MenuController* controller);

  // Dispatcher method. This returns true if the menu was canceled, or
  // if the message is such that the menu should be closed.
  virtual bool Dispatch(const MSG& msg);

  // Key processing. The return value of these is returned from Dispatch.
  // In other words, if these return false (which they do if escape was
  // pressed, or a matching mnemonic was found) the message loop returns.
  bool OnKeyDown(const MSG& msg);
  bool OnChar(const MSG& msg);

  // Creates a MenuController. If blocking is true, Run blocks the caller
  explicit MenuController(bool blocking);

  ~MenuController();

  // Invoked when the user accepts the selected item. This is only used
  // when blocking. This schedules the loop to quit.
  void Accept(MenuItemView* item, int mouse_event_flags);

  // Closes all menus, including any menus of nested invocations of Run.
  void CloseAllNestedMenus();

  // Gets the enabled menu item at the specified location.
  // If over_any_menu is non-null it is set to indicate whether the location
  // is over any menu. It is possible for this to return NULL, but
  // over_any_menu to be true. For example, the user clicked on a separator.
  MenuItemView* GetMenuItemAt(View* menu, int x, int y);

  // If there is an empty menu item at the specified location, it is returned.
  MenuItemView* GetEmptyMenuItemAt(View* source, int x, int y);

  // Returns true if the coordinate is over the scroll buttons of the
  // SubmenuView's MenuScrollViewContainer. If true is returned, part is set to
  // indicate which scroll button the coordinate is.
  bool IsScrollButtonAt(SubmenuView* source,
                        int x,
                        int y,
                        MenuPart::Type* part);

  // Returns the target for the mouse event.
  MenuPart GetMenuPartByScreenCoordinate(SubmenuView* source,
                                         int source_x,
                                         int source_y);

  // Implementation of GetMenuPartByScreenCoordinate for a single menu. Returns
  // true if the supplied SubmenuView contains the location in terms of the
  // screen. If it does, part is set appropriately and true is returned.
  bool GetMenuPartByScreenCoordinateImpl(SubmenuView* menu,
                                         const gfx::Point& screen_loc,
                                         MenuPart* part);

  // Returns true if the SubmenuView contains the specified location. This does
  // NOT included the scroll buttons, only the submenu view.
  bool DoesSubmenuContainLocation(SubmenuView* submenu,
                                  const gfx::Point& screen_loc);

  // Opens/Closes the necessary menus such that state_ matches that of
  // pending_state_. This is invoked if submenus are not opened immediately,
  // but after a delay.
  void CommitPendingSelection();

  // If item has a submenu, it is closed. This does NOT update the selection
  // in anyway.
  void CloseMenu(MenuItemView* item);

  // If item has a submenu, it is opened. This does NOT update the selection
  // in anyway.
  void OpenMenu(MenuItemView* item);

  // Builds the paths of the two menu items into the two paths, and
  // sets first_diff_at to the location of the first difference between the
  // two paths.
  void BuildPathsAndCalculateDiff(MenuItemView* old_item,
                                  MenuItemView* new_item,
                                  std::vector<MenuItemView*>* old_path,
                                  std::vector<MenuItemView*>* new_path,
                                  size_t* first_diff_at);

  // Builds the path for the specified item.
  void BuildMenuItemPath(MenuItemView* item, std::vector<MenuItemView*>* path);

  // Starts/stops the timer that commits the pending state to state
  // (opens/closes submenus).
  void StartShowTimer();
  void StopShowTimer();

  // Starts/stops the timer cancel the menu. This is used during drag and
  // drop when the drop enters/exits the menu.
  void StartCancelAllTimer();
  void StopCancelAllTimer();

  // Calculates the bounds of the menu to show. is_leading is set to match the
  // direction the menu opened in.
  gfx::Rect CalculateMenuBounds(MenuItemView* item,
                                bool prefer_leading,
                                bool* is_leading);

  // Returns the depth of the menu.
  static int MenuDepth(MenuItemView* item);

  // Selects the next/previous menu item.
  void IncrementSelection(int delta);

  // If the selected item has a submenu and it isn't currently open, the
  // the selection is changed such that the menu opens immediately.
  void OpenSubmenuChangeSelectionIfCan();

  // If possible, closes the submenu.
  void CloseSubmenu();

  // Returns true if window is the window used to show item, or any of
  // items ancestors.
  bool IsMenuWindow(MenuItemView* item, HWND window);

  // Selects by mnemonic, and if that doesn't work tries the first character of
  // the title. Returns true if a match was selected and the menu should exit.
  bool SelectByChar(wchar_t key);

  // If there is a window at the location of the event, a new mouse event is
  // generated and posted to it.
  void RepostEvent(SubmenuView* source, const MouseEvent& event);

  // Sets the drop target to new_item.
  void SetDropMenuItem(MenuItemView* new_item,
                       MenuDelegate::DropPosition position);

  // Starts/stops scrolling as appropriate. part gives the part the mouse is
  // over.
  void UpdateScrolling(const MenuPart& part);

  // Stops scrolling.
  void StopScrolling();

  // The active instance.
  static MenuController* active_instance_;

  // If true, Run blocks. If false, Run doesn't block and this is used for
  // drag and drop. Note that the semantics for drag and drop are slightly
  // different: cancel timer is kicked off any time the drag moves outside the
  // menu, mouse events do nothing...
  bool blocking_run_;

  // If true, we're showing.
  bool showing_;

  // If true, all nested run loops should be exited.
  bool exit_all_;

  // Whether we did a capture. We do a capture only if we're blocking and
  // the mouse was down when Run.
  bool did_capture_;

  // As the user drags the mouse around pending_state_ changes immediately.
  // When the user stops moving/dragging the mouse (or clicks the mouse)
  // pending_state_ is committed to state_, potentially resulting in
  // opening or closing submenus. This gives a slight delayed effect to
  // submenus as the user moves the mouse around. This is done so that as the
  // user moves the mouse all submenus don't immediately pop.
  State pending_state_;
  State state_;

  // If the user accepted the selection, this is the result.
  MenuItemView* result_;

  // The mouse event flags when the user clicked on a menu. Is 0 if the
  // user did not use the mousee to select the menu.
  int result_mouse_event_flags_;

  // If not empty, it means we're nested. When Run is invoked from within
  // Run, the current state (state_) is pushed onto menu_stack_. This allows
  // MenuController to restore the state when the nested run returns.
  std::list<State> menu_stack_;

  // As the mouse moves around submenus are not opened immediately. Instead
  // they open after this timer fires.
  base::OneShotTimer<MenuController> show_timer_;

  // Used to invoke CancelAll(). This is used during drag and drop to hide the
  // menu after the mouse moves out of the of the menu. This is necessitated by
  // the lack of an ability to detect when the drag has completed from the drop
  // side.
  base::OneShotTimer<MenuController> cancel_all_timer_;

  // Drop target.
  MenuItemView* drop_target_;
  MenuDelegate::DropPosition drop_position_;

  // Owner of child windows.
  HWND owner_;

  // Indicates a possible drag operation.
  bool possible_drag_;

  // Location the mouse was pressed at. Used to detect d&d.
  int press_x_;
  int press_y_;

  // We get a slew of drag updated messages as the mouse is over us. To avoid
  // continually processing whether we can drop, we cache the coordinates.
  bool valid_drop_coordinates_;
  int drop_x_;
  int drop_y_;
  int last_drop_operation_;

  // If true, the mouse is over some menu.
  bool any_menu_contains_mouse_;

  // If true, we're in the middle of invoking ShowAt on a submenu.
  bool showing_submenu_;

  // Task for scrolling the menu. If non-null indicates a scroll is currently
  // underway.
  scoped_ptr<MenuScrollTask> scroll_task_;

  DISALLOW_EVIL_CONSTRUCTORS(MenuController);
};

}  // namespace views

#endif  // CHROME_VIEWS_CONTROLS_MENU_CHROME_MENU_H_
