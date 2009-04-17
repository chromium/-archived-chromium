// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BOOKMARKS_MODULE_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BOOKMARKS_MODULE_H__

#include "chrome/browser/extensions/extension_function.h"

class GetBookmarksFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};

class SearchBookmarksFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};

class RemoveBookmarkFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};

class CreateBookmarkFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};

class MoveBookmarkFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};

class SetBookmarkTitleFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BOOKMARKS_MODULE_H__

