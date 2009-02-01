// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/view_storage.h"

#include <algorithm>

#include "chrome/common/notification_service.h"
#include "chrome/common/stl_util-inl.h"

namespace views {

ViewStorage* ViewStorage::shared_instance_ = NULL;

// This struct contains the information to locate a specific view.
// Locating a view is not always straight-forward as a view can be a floating
// view, or the child of a floating view. Floating views are frequently deleted
// and recreated (typically when their container is laid out).
struct ViewLocationInfo {
  // True if the view is floating or the child of a floating view. False
  // otherwise.
  bool is_floating_view;

  // If non floating, this is the stored view. If floating, the parent of the
  // floating view.
  View*  view;

  // The id of the floating view.
  int floating_view_id;

  // The path from the floating view to the stored view.  The path is composed
  // of the indexes of the views in the hierarchy.
  std::vector<int> floating_view_to_view_path;
};

// static
ViewStorage* ViewStorage::GetSharedInstance() {
  if (shared_instance_)
    return shared_instance_;

  shared_instance_ = new ViewStorage();
  return shared_instance_;
}

// static
void ViewStorage::DeleteSharedInstance() {
  if (!shared_instance_)
    return;
  delete shared_instance_;
  shared_instance_ = NULL;
}

ViewStorage::ViewStorage() : view_storage_next_id_(0) {
  NotificationService::current()->AddObserver(
      this, NotificationType::VIEW_REMOVED, NotificationService::AllSources());
}

ViewStorage::~ViewStorage() {
  NotificationService::current()->RemoveObserver(
    this, NotificationType::VIEW_REMOVED, NotificationService::AllSources());

  STLDeleteContainerPairSecondPointers(id_to_view_location_.begin(),
                                       id_to_view_location_.end());

  STLDeleteContainerPairSecondPointers(view_to_ids_.begin(),
                                       view_to_ids_.end());
}

int ViewStorage::CreateStorageID() {
  return view_storage_next_id_++;
}

void ViewStorage::StoreView(int storage_id, View* view) {
  DCHECK(view);
  std::map<int, ViewLocationInfo*>::iterator iter =
      id_to_view_location_.find(storage_id);
  DCHECK(iter == id_to_view_location_.end());

  if (iter != id_to_view_location_.end())
    RemoveView(storage_id);

  ViewLocationInfo* view_location_info = new ViewLocationInfo();
  View* floating_view_parent = view->RetrieveFloatingViewParent();
  if (floating_view_parent) {
    // The view is a floating view or is a child of a floating view.
    view_location_info->is_floating_view = true;
    view_location_info->view = floating_view_parent->GetParent();
    view_location_info->floating_view_id =
        floating_view_parent->GetFloatingViewID();
    // Ley's store the path from the floating view to the actual view so we can
    // locate it when restoring.
    View::GetViewPath(floating_view_parent, view,
                      &(view_location_info->floating_view_to_view_path));
  } else {
    // It is a non floating view, it can be stored as is.
    view_location_info->is_floating_view = false;
    view_location_info->view = view;
  }
  id_to_view_location_[storage_id] = view_location_info;

  std::vector<int>* ids = NULL;
  std::map<View*, std::vector<int>*>::iterator id_iter =
      view_to_ids_.find(view_location_info->view);
  if (id_iter == view_to_ids_.end()) {
    ids = new std::vector<int>();
    view_to_ids_[view_location_info->view] = ids;
  } else {
    ids = id_iter->second;
  }
  ids->push_back(storage_id);
}

View* ViewStorage::RetrieveView(int storage_id) {
  std::map<int, ViewLocationInfo*>::iterator iter =
      id_to_view_location_.find(storage_id);
  if (iter == id_to_view_location_.end())
    return NULL;

  ViewLocationInfo* view_location_info = iter->second;
  if (view_location_info->is_floating_view) {
    View* floating_view = view_location_info->view->
        RetrieveFloatingViewForID(view_location_info->floating_view_id);
    View* v = NULL;
    if (floating_view) {
      v = View::GetViewForPath(floating_view,
                               view_location_info->floating_view_to_view_path);
    }
    if (!v) {
      // If we have not found the view, it means either the floating view with
      // the id we have is gone, or it has changed and the actual child view
      // we have the path for is not accessible.  In that case, let's make sure
      // we don't leak the ViewLocationInfo.
      RemoveView(storage_id);
    }
    return v;
  } else {
    return view_location_info->view;
  }
}

void ViewStorage::RemoveView(int storage_id) {
  EraseView(storage_id, false);
}

void ViewStorage::EraseView(int storage_id, bool remove_all_ids) {
  // Remove the view from id_to_view_location_.
  std::map<int, ViewLocationInfo*>::iterator location_iter =
      id_to_view_location_.find(storage_id);
  if (location_iter == id_to_view_location_.end())
    return;

  ViewLocationInfo* view_location = location_iter->second;
  View* view = view_location->view;
  delete view_location;
  id_to_view_location_.erase(location_iter);

  // Also update view_to_ids_.
  std::map<View*, std::vector<int>*>::iterator ids_iter =
      view_to_ids_.find(view);
  DCHECK(ids_iter != view_to_ids_.end());
  std::vector<int>* ids = ids_iter->second;

  if (remove_all_ids) {
    for (size_t i = 0; i < ids->size(); ++i) {
      location_iter = id_to_view_location_.find((*ids)[i]);
      if (location_iter != id_to_view_location_.end()) {
        delete location_iter->second;
        id_to_view_location_.erase(location_iter);
      }
    }
    ids->clear();
  } else {
    std::vector<int>::iterator id_iter =
        std::find(ids->begin(), ids->end(), storage_id);
    DCHECK(id_iter != ids->end());
    ids->erase(id_iter);
  }

  if (ids->empty()) {
    delete ids;
    view_to_ids_.erase(ids_iter);
  }
}

void ViewStorage::Observe(NotificationType type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  DCHECK(type == NotificationType::VIEW_REMOVED);

  // Let's first retrieve the ids for that view.
  std::map<View*, std::vector<int>*>::iterator ids_iter =
      view_to_ids_.find(Source<View>(source).ptr());

  if (ids_iter == view_to_ids_.end()) {
    // That view is not in the view storage.
    return;
  }

  std::vector<int>* ids = ids_iter->second;
  DCHECK(!ids->empty());
  EraseView((*ids)[0], true);
}

}  // namespace views

