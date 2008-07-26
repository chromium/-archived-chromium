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

#ifndef CHROME_VIEWS_VIEW_STORAGE_H__
#define CHROME_VIEWS_VIEW_STORAGE_H__

#include "chrome/common/notification_service.h"
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

namespace ChromeViews {

struct ViewLocationInfo;

class ViewStorage : public NotificationObserver {
 public:
  // Returns the global ViewStorage instance.
  // It is guaranted to be non NULL.
  static ViewStorage* GetSharedInstance();

  // Deletes the global instance of the ViewStorage.
  static void DeleteSharedInstance();

  // Returns a unique storage id that can be used to store/retrieve views.
  int CreateStorageID();

  // Associates |view| with the specified |storage_id|.
  void StoreView(int storage_id, View* view);

  // Returns the view associated with |storage_id| if any, NULL otherwise.
  View* RetrieveView(int storage_id);

  // Removes the view associated with |storage_id| if any.
  void RemoveView(int storage_id);

 private:
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

  // The singleton instance.
  static ViewStorage* shared_instance_;

  DISALLOW_EVIL_CONSTRUCTORS(ViewStorage);
};

}

#endif  // #ifndef CHROME_VIEWS_VIEW_STORAGE_H__
