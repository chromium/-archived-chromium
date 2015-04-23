// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_bookmarks_module_constants.h"

namespace extension_bookmarks_module_constants {

const wchar_t kIdKey[] = L"id";
const wchar_t kIndexKey[] = L"index";
const wchar_t kParentIdKey[] = L"parentId";
const wchar_t kOldIndexKey[] = L"oldIndex";
const wchar_t kOldParentIdKey[] = L"oldParentId";
const wchar_t kUrlKey[] = L"url";
const wchar_t kTitleKey[] = L"title";
const wchar_t kChildrenKey[] = L"children";
const wchar_t kRecursiveKey[] = L"recursive";
const wchar_t kDateAddedKey[] = L"dateAdded";
const wchar_t kDateGroupModifiedKey[] = L"dateGroupModified";

const char kNoNodeError[] = "Can't find bookmark for id.";
const char kNoParentError[] = "Can't find parent bookmark for id.";
const char kFolderNotEmptyError[] =
    "Can't remove non-empty folder (use recursive to force).";
const char kInvalidIndexError[] = "Index out of bounds.";
const char kInvalidUrlError[] = "Invalid URL.";
const char kModifySpecialError[] = "Can't modify the root bookmark folders.";

const char kOnBookmarkAdded[] = "bookmark-added";
const char kOnBookmarkRemoved[] = "bookmark-removed";
const char kOnBookmarkChanged[] = "bookmark-changed";
const char kOnBookmarkMoved[] = "bookmark-moved";
const char kOnBookmarkChildrenReordered[] = "bookmark-children-reordered";

const char kGetBookmarksFunction[] = "GetBookmarks";
const char kGetBookmarkChildrenFunction[] = "GetBookmarkChildren";
const char kGetBookmarkTreeFunction[] = "GetBookmarkTree";
const char kSearchBookmarksFunction[] = "SearchBookmarks";
const char kRemoveBookmarkFunction[] = "RemoveBookmark";
const char kCreateBookmarkFunction[] = "CreateBookmark";
const char kMoveBookmarkFunction[] = "MoveBookmark";
const char kSetBookmarkTitleFunction[] = "SetBookmarkTitle";

}  // namespace extension_bookmarks_module_constants
