// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_drag_utils.h"

#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/common/drag_drop_types.h"
#include "chrome/views/event.h"

namespace {

void CloneDragDataImpl(BookmarkModel* model,
                       const BookmarkDragData::Element& element,
                       BookmarkNode* parent,
                       int index_to_add_at) {
  if (element.is_url) {
    model->AddURL(parent, index_to_add_at, element.title, element.url);
  } else {
    BookmarkNode* new_folder = model->AddGroup(parent, index_to_add_at,
                                               element.title);
    for (int i = 0; i < static_cast<int>(element.children.size()); ++i)
      CloneDragDataImpl(model, element.children[i], new_folder, i);
  }
}

}  // namespace

namespace bookmark_drag_utils {

int PreferredDropOperation(const views::DropTargetEvent& event,
                           int operation) {
  int common_ops = (event.GetSourceOperations() & operation);
  if (!common_ops)
    return 0;
  if (DragDropTypes::DRAG_COPY & common_ops)
    return DragDropTypes::DRAG_COPY;
  if (DragDropTypes::DRAG_LINK & common_ops)
    return DragDropTypes::DRAG_LINK;
  if (DragDropTypes::DRAG_MOVE & common_ops)
    return DragDropTypes::DRAG_MOVE;
  return DragDropTypes::DRAG_NONE;
}

bool IsValidDropLocation(Profile* profile,
                         const BookmarkDragData& data,
                         BookmarkNode* drop_parent,
                         int index) {
  if (!drop_parent->is_folder()) {
    NOTREACHED();
    return false;
  }

  if (!data.is_valid())
    return false;

  if (data.IsFromProfile(profile)) {
    std::vector<BookmarkNode*> nodes = data.GetNodes(profile);
    for (size_t i = 0; i < nodes.size(); ++i) {
      // Don't allow the drop if the user is attempting to drop on one of the
      // nodes being dragged.
      BookmarkNode* node = nodes[i];
      int node_index = (drop_parent == node->GetParent()) ?
          drop_parent->IndexOfChild(nodes[i]) : -1;
      if (node_index != -1 && (index == node_index || index == node_index + 1))
        return false;

      // drop_parent can't accept a child that is an ancestor.
      if (drop_parent->HasAncestor(node))
        return false;
    }
    return true;
  }
  // From the same profile, always accept.
  return true;
}

void CloneDragData(BookmarkModel* model,
                   const std::vector<BookmarkDragData::Element>& elements,
                   BookmarkNode* parent,
                   int index_to_add_at) {
  if (!parent->is_folder() || !model) {
    NOTREACHED();
    return;
  }
  for (size_t i = 0; i < elements.size(); ++i)
    CloneDragDataImpl(model, elements[i], parent, index_to_add_at + i);
}

}  // namespace bookmark_drag_utils
