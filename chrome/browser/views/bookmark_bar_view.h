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

#ifndef CHROME_BROWSER_VIEWS_BOOKMARK_BAR_VIEW_H_
#define CHROME_BROWSER_VIEWS_BOOKMARK_BAR_VIEW_H_

#include "chrome/browser/bookmark_bar_model.h"
#include "chrome/browser/bookmark_drag_data.h"
#include "chrome/common/slide_animation.h"
#include "chrome/views/label.h"
#include "chrome/views/menu.h"
#include "chrome/views/menu_button.h"
#include "chrome/views/view.h"
#include "chrome/views/view_menu_delegate.h"

class Browser;
class PageNavigator;
class PrefService;

namespace {
class MenuRunner;
class ButtonSeparatorView;
struct DropInfo;
}

namespace ChromeViews {
class MenuItemView;
}

// BookmarkBarView renders the BookmarkBarModel.  Each starred entry
// on the BookmarkBar is rendered as a MenuButton. An additional
// MenuButton aligned to the right allows the user to quickly see
// recently starred entries.
//
// BookmarkBarView shows the bookmarks from a specific Profile. BookmarkBarView
// waits until the HistoryService for the profile has been loaded before
// creating the BookmarkBarModel.
class BookmarkBarView : public ChromeViews::View,
                        public BookmarkBarModelObserver,
                        public ChromeViews::ViewMenuDelegate,
                        public ChromeViews::BaseButton::ButtonListener,
                        public Menu::Delegate,
                        public NotificationObserver,
                        public ChromeViews::ContextMenuController,
                        public ChromeViews::DragController,
                        public AnimationDelegate {
  friend class MenuRunner;
  friend class ShowFolderMenuTask;

 public:
  // Interface implemented by controllers/views that need to be notified any
  // time the model changes, typically to cancel an operation that is showing
  // data from the model such as a menu. This isn't intended as a general
  // way to be notified of changes, rather for cases where a controller/view is
  // showing data from the model in a modal like setting and needs to cleanly
  // exit the modal loop if the model changes out from under it.
  //
  // A controller/view that needs this notification should install itself as the
  // ModelChangeListener via the SetModelChangedListener method when shown and
  // reset the ModelChangeListener of the BookmarkBarView when it closes by way
  // of either the SetModelChangedListener method or the
  // ClearModelChangedListenerIfEquals method.
  class ModelChangedListener {
   public:
    virtual ~ModelChangedListener() {}

    // Invoked when the model changes. Should cancel the edit and close any
    // dialogs.
    virtual void ModelChanged() = 0;
  };

  explicit BookmarkBarView(Profile* profile, Browser* browser);
  virtual ~BookmarkBarView();

  static void RegisterUserPrefs(PrefService* prefs);

  // Resets the profile. This removes any buttons for the current profile and
  // recreates the models.
  void SetProfile(Profile* profile);

  // Returns the current profile.
  Profile* GetProfile() { return profile_; }

  // Sets the PageNavigator that is used when the user selects an entry on
  // the bookmark bar.
  void SetPageNavigator(PageNavigator* navigator);

  // View methods:
  virtual void GetPreferredSize(CSize *out);
  virtual void Layout();
  virtual void DidChangeBounds(const CRect& previous, const CRect& current);
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);
  virtual void Paint(ChromeCanvas* canvas);
  virtual void PaintChildren(ChromeCanvas* canvas);
  virtual bool CanDrop(const OSExchangeData& data);
  virtual void OnDragEntered(const ChromeViews::DropTargetEvent& event);
  virtual int OnDragUpdated(const ChromeViews::DropTargetEvent& event);
  virtual void OnDragExited();
  virtual int OnPerformDrop(const ChromeViews::DropTargetEvent& event);

  // Sets the model change listener to listener.
  void SetModelChangedListener(ModelChangedListener* listener) {
    model_changed_listener_ = listener;
  }

  // If the ModelChangedListener is listener, ModelChangeListener is set to
  // NULL.
  void ClearModelChangedListenerIfEquals(ModelChangedListener* listener) {
    if (model_changed_listener_ == listener)
      model_changed_listener_ = NULL;
  }

  // Returns the model change listener.
  ModelChangedListener* GetModelChangedListener() {
    return model_changed_listener_;
  }

  // Returns the page navigator.
  PageNavigator* GetPageNavigator() { return page_navigator_; }

  // Returns the model.
  BookmarkBarModel* GetModel() { return model_; }

  // Toggles whether the bookmark bar is shown only on the new tab page or on
  // all tabs.
  void ToggleWhenVisible();

  // Returns true if the bookmarks bar preference is set to 'always show', we
  // use this as a shorthand way of knowing what style of bar to draw (if the
  // pref is set to false but we're painting, then we must be on the new tab
  // page).
  bool IsAlwaysShown();

  // True if we're supposed to draw the bookmarks bar in the new tab style.
  bool IsNewTabPage();

  // SlideAnimationDelegate implementation.
  void AnimationProgressed(const Animation* animation);
  void AnimationEnded(const Animation* animation);

  // Returns the button at the specified index.
  ChromeViews::TextButton* GetBookmarkButton(int index);

  // Returns the button responsible for showing bookmarks in the other bookmark
  // folder.
  ChromeViews::TextButton* other_bookmarked_button() const {
    return other_bookmarked_button_;
  }

  // Returns the active MenuItemView, or NULL if a menu isn't showing.
  ChromeViews::MenuItemView* GetMenu();

  // Returns the drop MenuItemView, or NULL if a menu isn't showing.
  ChromeViews::MenuItemView* GetDropMenu();

  // Returns the context menu, or null if one isn't showing.
  ChromeViews::MenuItemView* GetContextMenu();

  // Returns the button used when not all the items on the bookmark bar fit.
  ChromeViews::TextButton* overflow_button() const { return overflow_button_; }

  // Maximum size of buttons on the bookmark bar.
  static const int kMaxButtonWidth;

  // If true we're running tests. This short circuits a couple of animations.
  static bool testing_;

 private:
  // Task that invokes ShowDropFolderForNode when run. ShowFolderDropMenuTask
  // deletes itself once run.
  class ShowFolderDropMenuTask : public Task {
   public:
    ShowFolderDropMenuTask(BookmarkBarView* view,
                           BookmarkBarNode* node)
      : view_(view),
        node_(node) {
    }

    void Cancel() {
      view_->show_folder_drop_menu_task_ = NULL;
      view_ = NULL;
    }

    virtual void Run() {
      if (view_) {
        view_->show_folder_drop_menu_task_ = NULL;
        view_->ShowDropFolderForNode(node_);
      }
      // MessageLoop deletes us.
    }

   private:
    BookmarkBarView* view_;
    BookmarkBarNode* node_;

    DISALLOW_EVIL_CONSTRUCTORS(ShowFolderDropMenuTask);
  };

  // Creates recent bookmark button and when visible button as well as
  // calculating the preferred height.
  void Init();

  // Creates the button showing the other bookmarked items.
  ChromeViews::MenuButton* CreateOtherBookmarkedButton();

  // Creates the button used when not all bookmark buttons fit.
  ChromeViews::MenuButton* CreateOverflowButton();

  // Returns the number of buttons corresponding to starred urls/groups. This
  // is equivalent to the number of children the bookmark bar node from the
  // bookmark bar model has.
  int GetBookmarkButtonCount();

  // Invoked when the bookmark bar model has finished loading. Creates a button
  // for each of the children of the root node from the model.
  virtual void Loaded(BookmarkBarModel* model);

  // Invokes added followed by removed.
  virtual void BookmarkNodeMoved(BookmarkBarModel* model,
                                 BookmarkBarNode* old_parent,
                                 int old_index,
                                 BookmarkBarNode* new_parent,
                                 int new_index);

  // Notifies ModelChangeListener of change.
  // If the node was added to the root node, a button is created and added to
  // this bookmark bar view.
  virtual void BookmarkNodeAdded(BookmarkBarModel* model,
                                 BookmarkBarNode* parent,
                                 int index);

  // Implementation for BookmarkNodeAddedImpl.
  void BookmarkNodeAddedImpl(BookmarkBarModel* model,
                             BookmarkBarNode* parent,
                             int index);

  // Notifies ModelChangeListener of change.
  // If the node was a child of the root node, the button corresponding to it
  // is removed.
  virtual void BookmarkNodeRemoved(BookmarkBarModel* model,
                                   BookmarkBarNode* parent,
                                   int index);

  // Implementation for BookmarkNodeRemoved.
  void BookmarkNodeRemovedImpl(BookmarkBarModel* model,
                               BookmarkBarNode* parent,
                               int index);

  // Notifies ModelChangedListener and invokes BookmarkNodeChangedImpl.
  virtual void BookmarkNodeChanged(BookmarkBarModel* model,
                                   BookmarkBarNode* node);

  // If the node is a child of the root node, the button is updated
  // appropriately.
  void BookmarkNodeChangedImpl(BookmarkBarModel* model, BookmarkBarNode* node);

  // Invoked when the favicon is available. If the node is a child of the
  // root node, the appropriate button is updated. If a menu is showing, the
  // call is forwarded to the menu to allow for it to update the icon.
  virtual void BookmarkNodeFavIconLoaded(BookmarkBarModel* model,
                                         BookmarkBarNode* node);

  // DragController method. Determines the node representing sender and invokes
  // WriteDragData to write the actual data.
  virtual void WriteDragData(ChromeViews::View* sender,
                             int press_x,
                             int press_y,
                             OSExchangeData* data);

  // Writes a BookmarkDragData for node to data.
  void WriteDragData(BookmarkBarNode* node, OSExchangeData* data);

  // Returns the drag operations for the specified button.
  virtual int GetDragOperations(ChromeViews::View* sender, int x, int y);

  // ViewMenuDelegate method. 3 types of menus may be shown:
  // . the menu allowing the user to choose when the bookmark bar is visible.
  // . most recently bookmarked menu
  // . menu for star groups.
  // The latter two are handled by a MenuRunner, which builds the appropriate
  // menu.
  virtual void RunMenu(ChromeViews::View* view, const CPoint& pt, HWND hwnd);

  // Invoked when a star entry corresponding to a URL on the bookmark bar is
  // pressed. Forwards to the PageNavigator to open the URL.
  virtual void ButtonPressed(ChromeViews::BaseButton* sender);

  // Invoked for this View, one of the buttons or the 'other' button. Shows the
  // appropriate context menu.
  virtual void ShowContextMenu(ChromeViews::View* source,
                               int x,
                               int y,
                               bool is_mouse_gesture);

  // Creates the button for rendering the specified bookmark node.
  ChromeViews::View* CreateBookmarkButton(BookmarkBarNode* node);

  // COnfigures the button from the specified node. This sets the text,
  // and icon.
  void ConfigureButton(BookmarkBarNode* node, ChromeViews::TextButton* button);

  // Used when showing the menu allowing the user to choose when the bar is
  // visible. Return value corresponds to the users preference for when the
  // bar is visible.
  virtual bool IsItemChecked(int id) const;

  // Used when showing the menu allowing the user to choose when the bar is
  // visible. Updates the preferences to match the users choice as appropriate.
  virtual void ExecuteCommand(int id);

  // Notification that the HistoryService is up an running. Removes us as
  // a listener on the notification service and invokes
  // ProfileHasValidHistoryService.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Invoked when the profile has a history service. Recreates the models.
  void ProfileHasValidHistoryService();

  // If we have registered an observer on the notification service, this
  // unregisters it. This does nothing if we have not installed ourself as an
  // observer.
  void RemoveNotificationObservers();

  // If the ModelChangedListener is non-null, ModelChanged is invoked on it.
  void NotifyModelChanged();

  // Shows the menu used during drag and drop for the specified node.
  void ShowDropFolderForNode(BookmarkBarNode* node);

  // Cancels the timer used to show a drop menu.
  void StopShowFolderDropMenuTimer();

  // Stars the timer used to show a drop menu for node.
  void StartShowFolderDropMenuTimer(BookmarkBarNode* node);

  // Returns the drop operation and index for the drop based on the event
  // and data. Returns DragDropTypes::DRAG_NONE if not a valid location.
  int CalculateDropOperation(const ChromeViews::DropTargetEvent& event,
                             const BookmarkDragData& data,
                             int* index,
                             bool* drop_on,
                             bool* is_over_overflow,
                             bool* is_over_other);

  // Invokes CanDropAt to determine if this is a valid location for the data,
  // then returns the appropriate drag operation based on the data.
  int CalculateDropOperation(const BookmarkDragData& data,
                             BookmarkBarNode* parent,
                             int index);

  // Returns true if the specified location is a valid drop location for
  // the supplied drag data.
  bool CanDropAt(const BookmarkDragData& data,
                 BookmarkBarNode* parent,
                 int index);

  // Performs a drop of the specified data at the specified location. Returns
  // the result.
  int PerformDropImpl(const BookmarkDragData& data,
                      BookmarkBarNode* parent_node,
                      int index);

  // Creates a new group/entry for data, and recursively invokes itself for
  // all children of data. This is used during drag and drop to clone a
  // group from another profile.
  void CloneDragData(const BookmarkDragData& data,
                     BookmarkBarNode* parent,
                     int index_to_add_at);

  // Returns the index of the first hidden bookmark button. If all buttons are
  // visible, this returns GetBookmarkButtonCount().
  int GetFirstHiddenNodeIndex();

  // If the bookmark bubble is showing this determines which view should throb
  // and starts it throbbing. Does nothing if bookmark bubble isn't showing.
  void StartThrobbing();

  // If a button is currently throbbing, it is stopped. If immediate is true
  // the throb stops immediately, otherwise it stops after a couple more
  // throbs.
  void StopThrobbing(bool immediate);

  Profile* profile_;

  // Used for opening urls.
  PageNavigator* page_navigator_;

  // Model providing details as to the starred entries/groups that should be
  // shown. This is owned by the Profile.
  BookmarkBarModel* model_;

  // Used to manage showing a Menu: either for the most recently bookmarked
  // entries, or for the a starred group.
  scoped_ptr<MenuRunner> menu_runner_;

  // Used when showing a menu for drag and drop. That is, if the user drags
  // over a group this becomes non-null and is the MenuRunner used to manage
  // the menu showing the contents of the node.
  scoped_ptr<MenuRunner> drop_menu_runner_;

  // Shows the other bookmark entries.
  ChromeViews::MenuButton* other_bookmarked_button_;

  // ModelChangeListener.
  ModelChangedListener* model_changed_listener_;

  // Task used to delay showing of the drop menu.
  ShowFolderDropMenuTask* show_folder_drop_menu_task_;

  // Used to track drops on the bookmark bar view.
  scoped_ptr<DropInfo> drop_info_;

  // Visible if not all the bookmark buttons fit.
  ChromeViews::MenuButton* overflow_button_;

  // If no bookmarks are visible, we show some text explaining the bar.
  ChromeViews::Label* instructions_;

  ButtonSeparatorView* bookmarks_separator_view_;

  // Owning browser. This is NULL duing testing.
  Browser* browser_;

  // Animation controlling showing and hiding of the bar.
  scoped_ptr<SlideAnimation> size_animation_;

  // If the bookmark bubble is showing, this is the URL.
  GURL bubble_url_;

  // If the bookmark bubble is showing, this is the visible ancestor of the URL.
  // The visible ancestor is either the other_bookmarked_button_,
  // overflow_button_ or a button on the bar.
  ChromeViews::BaseButton* throbbing_view_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarView);
};

#endif  // CHROME_BROWSER_VIEWS_BOOKMARK_BAR_VIEW_H_
