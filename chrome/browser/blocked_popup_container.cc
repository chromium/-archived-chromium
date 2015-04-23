// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/blocked_popup_container.h"

#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/notification_service.h"

// static
BlockedPopupContainer* BlockedPopupContainer::Create(
    TabContents* owner, Profile* profile) {
  BlockedPopupContainer* container =
      new BlockedPopupContainer(owner, profile);
  BlockedPopupContainerView* view =
      BlockedPopupContainerView::Create(container);
  container->set_view(view);
  return container;
}

// static
BlockedPopupContainer* BlockedPopupContainer::Create(
    TabContents* owner, Profile* profile, BlockedPopupContainerView* view) {
  BlockedPopupContainer* container =
      new BlockedPopupContainer(owner, profile);
  container->set_view(view);
  return container;
}

// static
void BlockedPopupContainer::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterListPref(prefs::kPopupWhitelistedHosts);
}

void BlockedPopupContainer::AddTabContents(TabContents* tab_contents,
                                           const gfx::Rect& bounds,
                                           const std::string& host) {
  // Show whitelisted popups immediately.
  bool whitelisted = !!whitelist_.count(host);
  if (whitelisted)
    owner_->AddNewContents(tab_contents, NEW_POPUP, bounds, true, GURL());

  if (has_been_dismissed_) {
    // Don't want to show any other UI.
    if (!whitelisted)
      delete tab_contents;  // Discard blocked popups entirely.
    return;
  }

  if (whitelisted) {
    // Listen for this popup's destruction, so if the user closes it manually,
    // we'll know to stop caring about it.
    registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(tab_contents));

    unblocked_popups_[tab_contents] = host;
  } else {
    if (blocked_popups_.size() >= kImpossibleNumberOfPopups) {
      delete tab_contents;
      LOG(INFO) << "Warning: Renderer is sending more popups to us than should "
          "be possible. Renderer compromised?";
      return;
    }
    blocked_popups_.push_back(BlockedPopup(tab_contents, bounds, host));

    tab_contents->set_delegate(this);
  }

  PopupHosts::const_iterator i(popup_hosts_.find(host));
  if (i == popup_hosts_.end())
    popup_hosts_[host] = whitelisted;
  else
    DCHECK_EQ(whitelisted, i->second);

  UpdateView();
  view_->ShowView();
  owner_->PopupNotificationVisibilityChanged(true);
}

void BlockedPopupContainer::LaunchPopupAtIndex(size_t index) {
  if (index >= blocked_popups_.size())
    return;

  // Open the popup.
  BlockedPopups::iterator i(blocked_popups_.begin() + index);
  TabContents* tab_contents = i->tab_contents;
  tab_contents->set_delegate(NULL);
  owner_->AddNewContents(tab_contents, NEW_POPUP, i->bounds, true, GURL());

  const std::string& host = i->host;
  if (!host.empty()) {
    // Listen for this popup's destruction, so if the user closes it manually,
    // we'll know to stop caring about it.
    registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(tab_contents));

    // Add the popup to the unblocked list.  (Do this before the below call!)
    unblocked_popups_[tab_contents] = i->host;
  }

  // Remove the popup from the blocked list.
  EraseDataForPopupAndUpdateUI(i);
}

size_t BlockedPopupContainer::GetBlockedPopupCount() const {
  return blocked_popups_.size();
}

bool BlockedPopupContainer::IsHostWhitelisted(size_t index) const {
  PopupHosts::const_iterator i(ConvertHostIndexToIterator(index));
  return (i == popup_hosts_.end()) ? false : i->second;
}

void BlockedPopupContainer::ToggleWhitelistingForHost(size_t index) {
  PopupHosts::const_iterator i(ConvertHostIndexToIterator(index));
  const std::string& host = i->first;
  bool should_whitelist = !i->second;
  popup_hosts_[host] = should_whitelist;

  ListValue* whitelist_pref =
      prefs_->GetMutableList(prefs::kPopupWhitelistedHosts);
  if (should_whitelist) {
    whitelist_.insert(host);
    whitelist_pref->Append(new StringValue(host));

    // Open the popups in order.
    for (size_t j = 0; j < blocked_popups_.size();) {
      if (blocked_popups_[j].host == host)
        LaunchPopupAtIndex(j);  // This shifts the rest of the entries down.
      else
        ++j;
    }
  } else {
    // Remove from whitelist.
    whitelist_.erase(host);
    StringValue host_as_string(host);
    whitelist_pref->Remove(host_as_string);

    for (UnblockedPopups::iterator i(unblocked_popups_.begin());
         i != unblocked_popups_.end(); ) {
      TabContents* tab_contents = i->first;
      TabContentsDelegate* delegate = tab_contents->delegate();
      if ((i->second == host) && delegate->IsPopup(tab_contents)) {
        // Convert the popup back into a blocked popup.
        delegate->DetachContents(tab_contents);
        tab_contents->set_delegate(this);

        // Add the popup to the blocked list.  (Do this before the below call!)
        gfx::Rect bounds;
        tab_contents->GetContainerBounds(&bounds);
        blocked_popups_.push_back(BlockedPopup(tab_contents, bounds, host));

        // Remove the popup from the unblocked list.
        UnblockedPopups::iterator to_erase = i;
        ++i;
        EraseDataForPopupAndUpdateUI(to_erase);
      } else {
        ++i;
      }
    }
  }
}

void BlockedPopupContainer::CloseAll() {
  ClearData();
  HideSelf();
}

void BlockedPopupContainer::Destroy() {
  view_->Destroy();

  ClearData();
  GetConstrainingContents(NULL)->WillCloseBlockedPopupContainer(this);

  delete this;
}

void BlockedPopupContainer::RepositionBlockedPopupContainer() {
  view_->SetPosition();
}

TabContents* BlockedPopupContainer::GetTabContentsAt(size_t index) const {
  return blocked_popups_[index].tab_contents;
}

std::vector<std::string> BlockedPopupContainer::GetHosts() const {
  std::vector<std::string> hosts;
  for (PopupHosts::const_iterator i(popup_hosts_.begin());
       i != popup_hosts_.end(); ++i)
    hosts.push_back(i->first);
  return hosts;
}

// Overridden from TabContentsDelegate:
void BlockedPopupContainer::OpenURLFromTab(TabContents* source,
                                           const GURL& url,
                                           const GURL& referrer,
                                           WindowOpenDisposition disposition,
                                           PageTransition::Type transition) {
  owner_->OpenURL(url, referrer, disposition, transition);
}

void BlockedPopupContainer::AddNewContents(TabContents* source,
                                           TabContents* new_contents,
                                           WindowOpenDisposition disposition,
                                           const gfx::Rect& initial_position,
                                           bool user_gesture) {
  owner_->AddNewContents(new_contents, disposition, initial_position,
                         user_gesture, GURL());
}

void BlockedPopupContainer::CloseContents(TabContents* source) {
  for (BlockedPopups::iterator it = blocked_popups_.begin();
       it != blocked_popups_.end(); ++it) {
    TabContents* tab_contents = it->tab_contents;
    if (tab_contents == source) {
      tab_contents->set_delegate(NULL);
      EraseDataForPopupAndUpdateUI(it);
      delete tab_contents;
      break;
    }
  }
}

void BlockedPopupContainer::MoveContents(TabContents* source,
                                         const gfx::Rect& new_bounds) {
  for (BlockedPopups::iterator it = blocked_popups_.begin();
       it != blocked_popups_.end(); ++it) {
    if (it->tab_contents == source) {
      it->bounds = new_bounds;
      break;
    }
  }
}

bool BlockedPopupContainer::IsPopup(TabContents* source) {
  return true;
}

TabContents* BlockedPopupContainer::GetConstrainingContents(
    TabContents* source) {
  return owner_;
}

void BlockedPopupContainer::HideSelf() {
  view_->HideView();
  owner_->PopupNotificationVisibilityChanged(false);
}

void BlockedPopupContainer::ClearData() {
  for (BlockedPopups::iterator i(blocked_popups_.begin());
       i != blocked_popups_.end(); ++i) {
    TabContents* tab_contents = i->tab_contents;
    tab_contents->set_delegate(NULL);
    delete tab_contents;
  }
  blocked_popups_.clear();

  registrar_.RemoveAll();
  unblocked_popups_.clear();

  popup_hosts_.clear();
}

BlockedPopupContainer::PopupHosts::const_iterator
    BlockedPopupContainer::ConvertHostIndexToIterator(size_t index) const {
  if (index >= popup_hosts_.size())
    return popup_hosts_.end();
  // If only there was a std::map::const_iterator::operator +=() ...
  PopupHosts::const_iterator i(popup_hosts_.begin());
  for (size_t j = 0; j < index; ++j)
    ++i;
  return i;
}

void BlockedPopupContainer::EraseDataForPopupAndUpdateUI(
    BlockedPopups::iterator i) {
  // Erase the host if this is the last popup for that host.
  const std::string& host = i->host;
  if (!host.empty()) {
    bool should_erase_host = true;
    for (BlockedPopups::const_iterator j(blocked_popups_.begin());
         j != blocked_popups_.end(); ++j) {
      if ((j != i) && (j->host == host)) {
        should_erase_host = false;
        break;
      }
    }
    if (should_erase_host) {
      for (UnblockedPopups::const_iterator j(unblocked_popups_.begin());
           j != unblocked_popups_.end(); ++j) {
        if (j->second == host) {
          should_erase_host = false;
          break;
        }
      }
      if (should_erase_host)
        popup_hosts_.erase(host);
    }
  }

  // Erase the popup and update the UI.
  blocked_popups_.erase(i);
  UpdateView();
}

void BlockedPopupContainer::EraseDataForPopupAndUpdateUI(
    UnblockedPopups::iterator i) {
  // Stop listening for this popup's destruction.
  registrar_.Remove(this, NotificationType::TAB_CONTENTS_DESTROYED,
                    Source<TabContents>(i->first));

  // Erase the host if this is the last popup for that host.
  const std::string& host = i->second;
  if (!host.empty()) {
    bool should_erase_host = true;
    for (UnblockedPopups::const_iterator j(unblocked_popups_.begin());
         j != unblocked_popups_.end(); ++j) {
      if ((j != i) && (j->second == host)) {
        should_erase_host = false;
        break;
      }
    }
    if (should_erase_host) {
      for (BlockedPopups::const_iterator j(blocked_popups_.begin());
           j != blocked_popups_.end(); ++j) {
        if (j->host == host) {
          should_erase_host = false;
          break;
        }
      }
      if (should_erase_host)
        popup_hosts_.erase(host);
    }
  }

  // Erase the popup and update the UI.
  unblocked_popups_.erase(i);
  UpdateView();
}


// private:

BlockedPopupContainer::BlockedPopupContainer(TabContents* owner,
                                             Profile* profile)
    : owner_(owner),
      prefs_(profile->GetPrefs()),
      has_been_dismissed_(false),
      view_(NULL),
      profile_(profile) {
  // Copy whitelist pref into local member that's easier to use.
  const ListValue* whitelist_pref =
      prefs_->GetList(prefs::kPopupWhitelistedHosts);
  // Careful: The returned value could be NULL if the pref has never been set.
  if (whitelist_pref != NULL) {
    for (ListValue::const_iterator i(whitelist_pref->begin());
         i != whitelist_pref->end(); ++i) {
      std::string host;
      (*i)->GetAsString(&host);
      whitelist_.insert(host);
    }
  }
}

void BlockedPopupContainer::UpdateView() {
  if (blocked_popups_.empty() && unblocked_popups_.empty())
    HideSelf();
  else
    view_->UpdateLabel();
}

void BlockedPopupContainer::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_DESTROYED);
  TabContents* tab_contents = Source<TabContents>(source).ptr();
  UnblockedPopups::iterator i(unblocked_popups_.find(tab_contents));
  DCHECK(i != unblocked_popups_.end());
  EraseDataForPopupAndUpdateUI(i);
}
