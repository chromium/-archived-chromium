// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TABS_TAB_STRIP_MODEL_H_
#define CHROME_BROWSER_TABS_TAB_STRIP_MODEL_H_

#include <vector>

#include "base/observer_list.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/page_transition_types.h"

namespace gfx {
class Point;
class Rect;
}
class DockInfo;
class GURL;
class NavigationController;
class Profile;
class SiteInstance;
class TabContents;
class TabStripModelOrderController;
class TabStripModel;

////////////////////////////////////////////////////////////////////////////////
//
// TabStripModelObserver
//
//  Objects implement this interface when they wish to be notified of changes
//  to the TabStripModel.
//
//  Two major implementers are the TabStrip, which uses notifications sent
//  via this interface to update the presentation of the strip, and the Browser
//  object, which updates bookkeeping and shows/hides individual TabContentses.
//
//  Register your TabStripModelObserver with the TabStripModel using its
//  Add/RemoveObserver methods.
//
////////////////////////////////////////////////////////////////////////////////
class TabStripModelObserver {
 public:
  // A new TabContents was inserted into the TabStripModel at the specified
  // index. |foreground| is whether or not it was opened in the foreground
  // (selected).
  virtual void TabInsertedAt(TabContents* contents,
                             int index,
                             bool foreground) { }
  // The specified TabContents at |index| is being closed (and eventually
  // destroyed).
  virtual void TabClosingAt(TabContents* contents, int index) { }
  // The specified TabContents at |index| is being detached, perhaps to be
  // inserted in another TabStripModel. The implementer should take whatever
  // action is necessary to deal with the TabContents no longer being present.
  virtual void TabDetachedAt(TabContents* contents, int index) { }
  // The selected TabContents changed from |old_contents| to |new_contents| at
  // |index|. |user_gesture| specifies whether or not this was done by a user
  // input event (e.g. clicking on a tab, keystroke) or as a side-effect of
  // some other function.
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* new_contents,
                             int index,
                             bool user_gesture) { }
  // The specified TabContents at |from_index| was moved to |to_index|.
  virtual void TabMoved(TabContents* contents,
                        int from_index,
                        int to_index) { }
  // The specified TabContents at |index| changed in some way.
  virtual void TabChangedAt(TabContents* contents, int index) { }
  // The TabStripModel now no longer has any "significant" (user created or
  // user manipulated) tabs. The implementer may use this as a trigger to try
  // and close the window containing the TabStripModel, for example...
  virtual void TabStripEmpty() { }
};

///////////////////////////////////////////////////////////////////////////////
//
// TabStripModelDelegate
//
//  A delegate interface that the TabStripModel uses to perform work that it
//  can't do itself, such as obtain a container HWND for creating new
//  TabContents, creating new TabStripModels for detached tabs, etc.
//
//  This interface is typically implemented by the controller that instantiates
//  the TabStripModel (in our case the Browser object).
//
///////////////////////////////////////////////////////////////////////////////
class TabStripModelDelegate {
 public:
  // Retrieve the URL that should be used to construct blank tabs.
  virtual GURL GetBlankTabURL() const = 0;

  // Ask for a new TabStripModel to be created and the given tab contents to
  // be added to it. Its size and position are reflected in |window_bounds|.
  // If |dock_info|'s type is other than NONE, the newly created window should
  // be docked as identified by |dock_info|.
  virtual void CreateNewStripWithContents(TabContents* contents,
                                          const gfx::Rect& window_bounds,
                                          const DockInfo& dock_info) = 0;

  enum {
    TAB_MOVE_ACTION = 1,
    TAB_TEAROFF_ACTION = 2
  };

  // Determine what drag actions are possible for the specified strip.
  virtual int GetDragActions() const = 0;

  // Creates an appropriate TabContents for the given URL. This is handled by
  // the delegate since the TabContents may require special circumstances to
  // exist for it to be constructed (e.g. a parent HWND).
  // If |defer_load| is true, the navigation controller doesn't load the url.
  // If |instance| is not null, its process is used to render the tab.
  virtual TabContents* CreateTabContentsForURL(
      const GURL& url,
      const GURL& referrer,
      Profile* profile,
      PageTransition::Type transition,
      bool defer_load,
      SiteInstance* instance) const = 0;

  // Return whether some contents can be duplicated.
  virtual bool CanDuplicateContentsAt(int index) = 0;

  // Duplicate the contents at the provided index and places it into its own
  // window.
  virtual void DuplicateContentsAt(int index) = 0;

  // Called when a drag session has completed and the frame that initiated the
  // the session should be closed.
  virtual void CloseFrameAfterDragSession() = 0;

  // Creates an entry in the historical tab database for the specified
  // TabContents.
  virtual void CreateHistoricalTab(TabContents* contents) = 0;

  // Runs any unload listeners associated with the specified TabContents before
  // it is closed. If there are unload listeners that need to be run, this
  // function returns true and the TabStripModel will wait before closing the
  // TabContents. If it returns false, there are no unload listeners and the
  // TabStripModel can close the TabContents immediately.
  virtual bool RunUnloadListenerBeforeClosing(TabContents* contents) = 0;
};

////////////////////////////////////////////////////////////////////////////////
//
// TabStripModel
//
//  A model & low level controller of a Browser Window tabstrip. Holds a vector
//  of TabContents, and provides an API for adding, removing and shuffling
//  them, as well as a higher level API for doing specific Browser-related
//  tasks like adding new Tabs from just a URL, etc.
//
//  A TabStripModel has one delegate that it relies on to perform certain tasks
//  like creating new TabStripModels (probably hosted in Browser windows) when
//  required. See TabStripDelegate above for more information.
//
//  A TabStripModel also has N observers (see TabStripModelObserver above),
//  which can be registered via Add/RemoveObserver. An Observer is notified of
//  tab creations, removals, moves, and other interesting events. The
//  TabStrip implements this interface to know when to create new tabs in
//  the View, and the Browser object likewise implements to be able to update
//  its bookkeeping when such events happen.
//
////////////////////////////////////////////////////////////////////////////////
class TabStripModel : public NotificationObserver {
 public:
  // Construct a TabStripModel with a delegate to help it do certain things
  // (See TabStripModelDelegate documentation). |delegate| cannot be NULL.
  TabStripModel(TabStripModelDelegate* delegate, Profile* profile);
  virtual ~TabStripModel();

  // Retrieves the TabStripModelDelegate associated with this TabStripModel.
  TabStripModelDelegate* delegate() const { return delegate_; }

  // Add and remove observers to changes within this TabStripModel.
  void AddObserver(TabStripModelObserver* observer);
  void RemoveObserver(TabStripModelObserver* observer);

  // Retrieve the number of TabContentses/emptiness of the TabStripModel.
  int count() const { return static_cast<int>(contents_data_.size()); }
  bool empty() const { return contents_data_.empty(); }

  // Retrieve the Profile associated with this TabStripModel.
  Profile* profile() const { return profile_; }

  // Retrieve/set the active TabStripModelOrderController associated with this
  // TabStripModel
  TabStripModelOrderController* order_controller() const {
    return order_controller_;
  }
  void SetOrderController(TabStripModelOrderController* order_controller);

  // Retrieve the index of the currently selected TabContents.
  int selected_index() const { return selected_index_; }

  // See documentation for |next_selected_index_| below.
  int next_selected_index() const { return next_selected_index_; }

  // Returns true if the tabstrip is currently closing all open tabs (via a
  // call to CloseAllTabs). As tabs close, the selection in the tabstrip
  // changes which notifies observers, which can use this as an optimization to
  // avoid doing meaningless or unhelpful work.
  bool closing_all() const { return closing_all_; }

  // Basic API /////////////////////////////////////////////////////////////////

  static const int kNoTab = -1;

  // Determines if the specified index is contained within the TabStripModel.
  bool ContainsIndex(int index) const;

  // Adds the specified TabContents in the default location. Tabs opened in the
  // foreground inherit the group of the previously selected tab.
  void AppendTabContents(TabContents* contents, bool foreground);

  // Adds the specified TabContents in the specified location. If
  // |inherit_group| is true, the new contents is linked to the current tab's
  // group.
  void InsertTabContentsAt(int index,
                           TabContents* contents,
                           bool foreground,
                           bool inherit_group);

  // Closes the TabContents at the specified index. This causes the TabContents
  // to be destroyed, but it may not happen immediately (e.g. if it's a
  // WebContents).
  // Returns true if the TabContents was closed immediately, false if we are
  // waiting for a response from an onunload handler.
  bool CloseTabContentsAt(int index) {
    return InternalCloseTabContentsAt(index, true);
  }

  // Replaces the entire state of a the tab at index by switching in a
  // different NavigationController. This is used through the recently
  // closed tabs list, which needs to replace a tab's current state
  // and history with another set of contents and history.
  //
  // The old NavigationController is deallocated and this object takes
  // ownership of the passed in controller.
  void ReplaceNavigationControllerAt(int index,
                                     NavigationController* controller);

  // Detaches the TabContents at the specified index from this strip. The
  // TabContents is not destroyed, just removed from display. The caller is
  // responsible for doing something with it (e.g. stuffing it into another
  // strip).
  TabContents* DetachTabContentsAt(int index);

  // Select the TabContents at the specified index. |user_gesture| is true if
  // the user actually clicked on the tab or navigated to it using a keyboard
  // command, false if the tab was selected as a by-product of some other
  // action.
  void SelectTabContentsAt(int index, bool user_gesture);

  // Replace the TabContents at the specified index with another TabContents.
  // This is used when a navigation causes a different TabContentsType to be
  // required, e.g. the transition from New Tab to a web page.
  void ReplaceTabContentsAt(int index, TabContents* replacement_contents);

  // Move the TabContents at the specified index to another index. This method
  // does NOT send Detached/Attached notifications, rather it moves the
  // TabContents inline and sends a Moved notification instead.
  void MoveTabContentsAt(int index, int to_position);

  // Returns the currently selected TabContents, or NULL if there is none.
  TabContents* GetSelectedTabContents() const;

  // Returns the TabContents at the specified index, or NULL if there is none.
  TabContents* GetTabContentsAt(int index) const;

  // Returns the index of the specified TabContents, or -1 if the TabContents
  // is not in this TabStripModel.
  int GetIndexOfTabContents(const TabContents* contents) const;

  // Returns the index of the specified NavigationController, or -1 if it is
  // not in this TabStripModel.
  int GetIndexOfController(const NavigationController* controller) const;

  // Notify any observers that the TabContents at the specified index has
  // changed in some way.
  void UpdateTabContentsStateAt(int index);

  // Make sure there is an auto-generated New Tab tab in the TabStripModel.
  // If |force_create| is true, the New Tab will be created even if the
  // preference is set to false (used by startup).
  void EnsureNewTabVisible(bool force_create);

  // Close all tabs at once. Code can use closing_all() above to defer
  // operations that might otherwise by invoked by the flurry of detach/select
  // notifications this method causes.
  void CloseAllTabs();

  // Returns true if there are any TabContents that are currently loading.
  bool TabsAreLoading() const;

  // Returns the controller controller that opened the TabContents at |index|.
  NavigationController* GetOpenerOfTabContentsAt(int index);

  // Returns the index of the next TabContents in the sequence of TabContentses
  // spawned by the specified NavigationController after |start_index|.
  // If |use_group| is true, the group property of the tab is used instead of
  // the opener to find the next tab. Under some circumstances the group
  // relationship may exist but the opener may not.
  int GetIndexOfNextTabContentsOpenedBy(NavigationController* opener,
                                        int start_index,
                                        bool use_group);

  // Returns the index of the last TabContents in the model opened by the
  // specified opener, starting at |start_index|.
  int GetIndexOfLastTabContentsOpenedBy(NavigationController* opener,
                                        int start_index);

  // Forget all Opener relationships that are stored (but _not_ group
  // relationships!) This is to reduce unpredictable tab switching behavior
  // in complex session states. The exact circumstances under which this method
  // is called are left up to the implementation of the selected
  // TabStripModelOrderController.
  void ForgetAllOpeners();

  // Forgets the group affiliation of the specified TabContents. This should be
  // called when a TabContents that is part of a logical group of tabs is
  // moved to a new logical context by the user (e.g. by typing a new URL or
  // selecting a bookmark). This also forgets the opener, which is considered
  // a weaker relationship than group.
  void ForgetGroup(TabContents* contents);

  // Returns true if the group/opener relationships present for |contents|
  // should be reset when _any_ selection change occurs in the model.
  bool ShouldResetGroupOnSelect(TabContents* contents) const;

  // Command level API /////////////////////////////////////////////////////////

  // Adds a blank tab to the TabStripModel.
  TabContents* AddBlankTab(bool foreground);
  TabContents* AddBlankTabAt(int index, bool foreground);

  // Adds a TabContents at the best position in the TabStripModel given the
  // specified insertion index, transition, etc. Ultimately, the insertion
  // index of the TabContents is left up to the Order Controller associated
  // with this TabStripModel, so the final insertion index may differ from
  // |index|.
  void AddTabContents(TabContents* contents,
                      int index,
                      PageTransition::Type transition,
                      bool foreground);

  // Closes the selected TabContents.
  void CloseSelectedTab();

  // Select adjacent tabs
  void SelectNextTab();
  void SelectPreviousTab();

  // Selects the last tab in the tab strip.
  void SelectLastTab();

  // View API //////////////////////////////////////////////////////////////////

  // The specified contents should be opened in a new tabstrip.
  void TearOffTabContents(TabContents* detached_contents,
                          const gfx::Rect& window_bounds,
                          const DockInfo& dock_info);

  // Context menu functions.
  enum ContextMenuCommand {
    CommandFirst = 0,
    CommandNewTab,
    CommandReload,
    CommandDuplicate,
    CommandCloseTab,
    CommandCloseOtherTabs,
    CommandCloseTabsToRight,
    CommandCloseTabsOpenedBy,
    CommandLast
  };

  // Returns true if the specified command is enabled.
  bool IsContextMenuCommandEnabled(int context_index,
                                   ContextMenuCommand command_id);

  // Performs the action associated with the specified command for the given
  // TabStripModel index |context_index|.
  void ExecuteContextMenuCommand(int context_index,
                                 ContextMenuCommand command_id);

  // Returns a vector of indices of TabContentses opened from the TabContents
  // at the specified |index|.
  std::vector<int> GetIndexesOpenedBy(int index) const;

  // Overridden from notificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // We cannot be constructed without a delegate.
  TabStripModel();

  // Closes the TabContents at the specified index. This causes the TabContents
  // to be destroyed, but it may not happen immediately (e.g. if it's a
  // WebContents). If the page in question has an unload event the TabContents
  // will not be destroyed until after the event has completed, which will then
  // call back into this method.
  //
  // The boolean parameter create_historical_tab controls whether to
  // record this tab and its history for reopening recently closed
  // tabs.
  //
  // Returns true if the TabContents was closed immediately, false if we are
  // waiting for the result of an onunload handler.
  bool InternalCloseTabContentsAt(int index, bool create_historical_tab);

  TabContents* GetContentsAt(int index) const;

  // The actual implementation of SelectTabContentsAt. Takes the previously
  // selected contents in |old_contents|, which may actually not be in
  // |contents_| anymore because it may have been removed by a call to say
  // DetachTabContentsAt...
  void ChangeSelectedContentsFrom(
      TabContents* old_contents, int to_index, bool user_gesture);

  // Returns the number of New Tab tabs in the TabStripModel.
  int GetNewTabCount() const;

  // Convenience for setting the opener pointer for the specified |contents| to
  // be |opener|'s NavigationController.
  void SetOpenerForContents(TabContents* contents, TabContents* opener);

  // Returns true if the tab represented by the specified data has an opener
  // that matches the specified one. If |use_group| is true, then this will
  // fall back to check the group relationship as well.
  struct TabContentsData;
  static bool OpenerMatches(TabContentsData* data,
                            NavigationController* opener,
                            bool use_group);

  // Our delegate.
  TabStripModelDelegate* delegate_;

  // A hunk of data representing a TabContents and (optionally) the
  // NavigationController that spawned it. This memory only sticks around while
  // the TabContents is in the current TabStripModel, unless otherwise
  // specified in code.
  struct TabContentsData {
    TabContents* contents;
    // We use NavigationControllers here since they more closely model the
    // "identity" of a Tab, TabContents can change depending on the URL loaded
    // in the Tab.
    // The group is used to model a set of tabs spawned from a single parent
    // tab. This value is preserved for a given tab as long as the tab remains
    // navigated to the link it was initially opened at or some navigation from
    // that page (i.e. if the user types or visits a bookmark or some other
    // navigation within that tab, the group relationship is lost). This
    // property can safely be used to implement features that depend on a
    // logical group of related tabs.
    NavigationController* group;
    // The owner models the same relationship as group, except it is more
    // easily discarded, e.g. when the user switches to a tab not part of the
    // same group. This property is used to determine what tab to select next
    // when one is closed.
    NavigationController* opener;
    // True if our group should be reset the moment selection moves away from
    // this Tab. This is the case for tabs opened in the foreground at the end
    // of the TabStrip while viewing another Tab. If these tabs are closed
    // before selection moves elsewhere, their opener is selected. But if
    // selection shifts to _any_ tab (including their opener), the group
    // relationship is reset to avoid confusing close sequencing.
    bool reset_group_on_select;

    explicit TabContentsData(TabContents* a_contents)
        : contents(a_contents),
          reset_group_on_select(false) {
      SetGroup(NULL);
    }

    // Create a relationship between this TabContents and other TabContentses.
    // Used to identify which TabContents to select next after one is closed.
    void SetGroup(NavigationController* a_group) {
      group = a_group;
      opener = a_group;
    }

    // Forget the opener relationship so that when this TabContents is closed
    // unpredictable re-selection does not occur.
    void ForgetOpener() {
      opener = NULL;
    }
  };

  // The TabContents data currently hosted within this TabStripModel.
  typedef std::vector<TabContentsData*> TabContentsDataVector;
  TabContentsDataVector contents_data_;

  // The index of the TabContents in |contents_| that is currently selected.
  int selected_index_;

  // The index of the TabContnets in |contents_| that will be selected when the
  // current composite operation completes. A Tab Detach is an example of a
  // composite operation - it not only removes a tab from the strip, but also
  // causes the selection to shift. Some code needs to know what the next
  // selected index will be. In other cases, this value is equal to
  // selected_index_.
  int next_selected_index_;

  // A profile associated with this TabStripModel, used when creating new Tabs.
  Profile* profile_;

  // True if all tabs are currently being closed via CloseAllTabs.
  bool closing_all_;

  // An object that determines where new Tabs should be inserted and where
  // selection should move when a Tab is closed.
  TabStripModelOrderController* order_controller_;

  // Our observers.
  typedef ObserverList<TabStripModelObserver> TabStripModelObservers;
  TabStripModelObservers observers_;

  DISALLOW_COPY_AND_ASSIGN(TabStripModel);
};

#endif  // CHROME_BROWSER_TABS_TAB_STRIP_MODEL_H_
