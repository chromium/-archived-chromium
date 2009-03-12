// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_VIEW_STORAGE_H_
#define CHROME_VIEWS_VIEW_STORAGE_H_

#include "base/singleton.h"
#include "chrome/common/notification_observer.h"
#include "chrome/views/view.h"

// This class is a simple storage place for storing/retrieving views.  It is
// used for example in the FocusManager to store/restore focused views when the
// main window becomes active/inactive.  It supports floating views, meaning
// that when you store a view, it can be retrieved even if it is a floating
// view or the child of a floating view that has been detached since the view
// was stored (in which case the floating view is recreated and reattached).
// It also automatically removes a view from the storage if the view is removed
// from the tree hierarchy (or in the case of a floating view, if the view
// containing the floating view is removed).
//
// To use it, you first need to create a view storage id that can then be used
// to store/retrieve views.

namespace views {

struct ViewLocationInfo;

class ViewStorage : public NotificationObserver {
 public:
  // Returns the global ViewStorage instance.
  // It is guaranted to be non NULL.
  static ViewStorage* GetSharedInstance();

  // Returns a unique storage id that can be used to store/retrieve views.
  int CreateStorageID();

  // Associates |view| with the specified |storage_id|.
  void StoreView(int storage_id, View* view);

  // Returns the view associated with |storage_id| if any, NULL otherwise.
  View* RetrieveView(int storage_id);

  // Removes the view associated with |storage_id| if any.
  void RemoveView(int storage_id);

 private:
  friend struct DefaultSingletonTraits<ViewStorage>;

  ViewStorage();
  ~ViewStorage();

  // NotificationObserver method.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  // Removes the view associated with |storage_id|. If |remove_all_ids| is true,
  // all other mapping pointing to the same view are removed as well.
  void EraseView(int storage_id, bool remove_all_ids);

  // Next id for the view storage.
  int view_storage_next_id_;

  // The association id to View used for the view storage.  The ViewStorage owns
  // the ViewLocationInfo.
  std::map<int, ViewLocationInfo*> id_to_view_location_;

  // Association View to id, used to speed up view notification removal.
  std::map<View*, std::vector<int>*> view_to_ids_;

  DISALLOW_COPY_AND_ASSIGN(ViewStorage);
};

}  // namespace views

#endif  // #ifndef CHROME_VIEWS_VIEW_STORAGE_H_
