// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/focus/view_storage.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util-inl.h"

namespace views {

// static
ViewStorage* ViewStorage::GetSharedInstance() {
  return Singleton<ViewStorage>::get();
}

ViewStorage::ViewStorage() : view_storage_next_id_(0) {
}

ViewStorage::~ViewStorage() {
  STLDeleteContainerPairSecondPointers(view_to_ids_.begin(),
                                       view_to_ids_.end());
}

int ViewStorage::CreateStorageID() {
  return view_storage_next_id_++;
}

void ViewStorage::StoreView(int storage_id, View* view) {
  DCHECK(view);
  std::map<int, View*>::iterator iter = id_to_view_.find(storage_id);

  if (iter != id_to_view_.end()) {
    NOTREACHED();
    RemoveView(storage_id);
  }

  id_to_view_[storage_id] = view;

  std::vector<int>* ids = NULL;
  std::map<View*, std::vector<int>*>::iterator id_iter =
      view_to_ids_.find(view);
  if (id_iter == view_to_ids_.end()) {
    ids = new std::vector<int>();
    view_to_ids_[view] = ids;
  } else {
    ids = id_iter->second;
  }
  ids->push_back(storage_id);
}

View* ViewStorage::RetrieveView(int storage_id) {
  std::map<int, View*>::iterator iter = id_to_view_.find(storage_id);
  if (iter == id_to_view_.end())
    return NULL;
  return iter->second;
}

void ViewStorage::RemoveView(int storage_id) {
  EraseView(storage_id, false);
}

void ViewStorage::ViewRemoved(View* parent, View* removed) {
  // Let's first retrieve the ids for that view.
  std::map<View*, std::vector<int>*>::iterator ids_iter =
      view_to_ids_.find(removed);

  if (ids_iter == view_to_ids_.end()) {
    // That view is not in the view storage.
    return;
  }

  std::vector<int>* ids = ids_iter->second;
  DCHECK(!ids->empty());
  EraseView((*ids)[0], true);
}

void ViewStorage::EraseView(int storage_id, bool remove_all_ids) {
  // Remove the view from id_to_view_location_.
  std::map<int, View*>::iterator view_iter = id_to_view_.find(storage_id);
  if (view_iter == id_to_view_.end())
    return;

  View* view = view_iter->second;
  id_to_view_.erase(view_iter);

  // Also update view_to_ids_.
  std::map<View*, std::vector<int>*>::iterator ids_iter =
      view_to_ids_.find(view);
  DCHECK(ids_iter != view_to_ids_.end());
  std::vector<int>* ids = ids_iter->second;

  if (remove_all_ids) {
    for (size_t i = 0; i < ids->size(); ++i) {
      view_iter = id_to_view_.find((*ids)[i]);
      if (view_iter != id_to_view_.end())
        id_to_view_.erase(view_iter);
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

}  // namespace views
