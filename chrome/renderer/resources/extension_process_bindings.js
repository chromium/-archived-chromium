// Copyright (c) 2009 The chrome Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// -----------------------------------------------------------------------------
// NOTE: If you change this file you need to touch renderer_resources.grd to
// have your change take effect.
// -----------------------------------------------------------------------------

var chrome = chrome || {};
(function() {
  native function GetViews();
  native function GetWindow();
  native function GetCurrentWindow();
  native function GetLastFocusedWindow();
  native function CreateWindow();
  native function UpdateWindow();
  native function RemoveWindow();
  native function GetAllWindows();
  native function GetTab();
  native function GetSelectedTab();
  native function GetAllTabsInWindow();
  native function CreateTab();
  native function UpdateTab();
  native function MoveTab();
  native function RemoveTab();
  native function EnablePageAction();
  native function DisablePageAction();
  native function GetBookmarks();
  native function GetBookmarkChildren();
  native function GetBookmarkTree();
  native function SearchBookmarks();
  native function RemoveBookmark();
  native function CreateBookmark();
  native function MoveBookmark();
  native function SetBookmarkTitle();
  native function GetChromeHidden();

  if (!chrome)
    chrome = {};

  var chromeHidden = GetChromeHidden();

  // Validate arguments.
  function validate(args, schemas) {
    if (args.length > schemas.length)
      throw new Error("Too many arguments.");

    for (var i = 0; i < schemas.length; i++) {
      if (i in args && args[i] !== null && args[i] !== undefined) {
        var validator = new chrome.JSONSchemaValidator();
        validator.validate(args[i], schemas[i]);
        if (validator.errors.length == 0)
          continue;

        var message = "Invalid value for argument " + i + ". ";
        for (var i = 0, err; err = validator.errors[i]; i++) {
          if (err.path) {
            message += "Property '" + err.path + "': ";
          }
          message += err.message;
          message = message.substring(0, message.length - 1);
          message += ", ";
        }
        message = message.substring(0, message.length - 2);
        message += ".";

        throw new Error(message);
      } else if (!schemas[i].optional) {
        throw new Error("Parameter " + i + " is required.");
      }
    }
  }

  var sendRequest = chromeHidden.sendRequest;

  //----------------------------------------------------------------------------

  // Windows.
  chrome.windows = {};

  chrome.windows.get = function(windowId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetWindow, windowId, callback);
  };

  chrome.windows.get.params = [
    chrome.types.pInt,
    chrome.types.fun
  ];

  chrome.windows.getCurrent = function(callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetCurrentWindow, null, callback);
  };

  chrome.windows.getCurrent.params = [
    chrome.types.fun
  ];

  chrome.windows.getLastFocused = function(callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetLastFocusedWindow, null, callback);
  };

  chrome.windows.getLastFocused.params = [
    chrome.types.fun
  ];

  chrome.windows.getAll = function(populate, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetAllWindows, populate, callback);
  };

  chrome.windows.getAll.params = [
    chrome.types.optBool,
    chrome.types.fun
  ];

  chrome.windows.create = function(createData, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(CreateWindow, createData, callback);
  };
  chrome.windows.create.params = [
    {
      type: "object",
      properties: {
        url: chrome.types.optStr,
        left: chrome.types.optInt,
        top: chrome.types.optInt,
        width: chrome.types.optPInt,
        height: chrome.types.optPInt
      },
      optional: true
    },
    chrome.types.optFun
  ];

  chrome.windows.update = function(windowId, updateData, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(UpdateWindow, [windowId, updateData], callback);
  };
  chrome.windows.update.params = [
    chrome.types.pInt,
    {
      type: "object",
      properties: {
        left: chrome.types.optInt,
        top: chrome.types.optInt,
        width: chrome.types.optPInt,
        height: chrome.types.optPInt
      },
    },
    chrome.types.optFun
  ];

  chrome.windows.remove = function(windowId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(RemoveWindow, windowId, callback);
  };

  chrome.windows.remove.params = [
    chrome.types.pInt,
    chrome.types.optFun
  ];

  // sends (windowId).
  // *WILL* be followed by tab-attached AND then tab-selection-changed.
  chrome.windows.onCreated = new chrome.Event("window-created");

  // sends (windowId).
  // *WILL* be preceded by sequences of tab-removed AND then
  // tab-selection-changed -- one for each tab that was contained in the window
  // that closed
  chrome.windows.onRemoved = new chrome.Event("window-removed");

  // sends (windowId).
  chrome.windows.onFocusChanged =
        new chrome.Event("window-focus-changed");

  //----------------------------------------------------------------------------

  // Tabs
  chrome.tabs = {};

  chrome.tabs.get = function(tabId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetTab, tabId, callback);
  };

  chrome.tabs.get.params = [
    chrome.types.pInt,
    chrome.types.fun
  ];

  chrome.tabs.getSelected = function(windowId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetSelectedTab, windowId, callback);
  };

  chrome.tabs.getSelected.params = [
    chrome.types.optPInt,
    chrome.types.fun
  ];

  chrome.tabs.getAllInWindow = function(windowId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetAllTabsInWindow, windowId, callback);
  };

  chrome.tabs.getAllInWindow.params = [
    chrome.types.optPInt,
    chrome.types.fun
  ];

  chrome.tabs.create = function(tab, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(CreateTab, tab, callback);
  };

  chrome.tabs.create.params = [
    {
      type: "object",
      properties: {
        windowId: chrome.types.optPInt,
        index: chrome.types.optPInt,
        url: chrome.types.optStr,
        selected: chrome.types.optBool
      }
    },
    chrome.types.optFun
  ];

  chrome.tabs.update = function(tabId, updates, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(UpdateTab, [tabId, updates], callback);
  };

  chrome.tabs.update.params = [
    chrome.types.pInt,
    {
      type: "object",
      properties: {
        url: chrome.types.optStr,
        selected: chrome.types.optBool
      }
    },
    chrome.types.optFun
  ];

  chrome.tabs.move = function(tabId, moveProps, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(MoveTab, [tabId, moveProps], callback);
  };

  chrome.tabs.move.params = [
    chrome.types.pInt,
    {
      type: "object",
      properties: {
        windowId: chrome.types.optPInt,
        index: chrome.types.pInt
      }
    },
    chrome.types.optFun
  ];

  chrome.tabs.remove = function(tabId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(RemoveTab, tabId, callback);
  };

  chrome.tabs.remove.params = [
    chrome.types.pInt,
    chrome.types.optFun
  ];

  // Sends ({Tab}).
  // Will *NOT* be followed by tab-attached - it is implied.
  // *MAY* be followed by tab-selection-changed.
  chrome.tabs.onCreated = new chrome.Event("tab-created");

  // Sends (tabId, {ChangedProps}).
  chrome.tabs.onUpdated = new chrome.Event("tab-updated");

  // Sends (tabId, {windowId, fromIndex, toIndex}).
  // Tabs can only "move" within a window.
  chrome.tabs.onMoved = new chrome.Event("tab-moved");

  // Sends (tabId, {windowId}).
  chrome.tabs.onSelectionChanged =
       new chrome.Event("tab-selection-changed");

  // Sends (tabId, {newWindowId, newPosition}).
  // *MAY* be followed by tab-selection-changed.
  chrome.tabs.onAttached = new chrome.Event("tab-attached");

  // Sends (tabId, {oldWindowId, oldPosition}).
  // *WILL* be followed by tab-selection-changed.
  chrome.tabs.onDetached = new chrome.Event("tab-detached");

  // Sends (tabId).
  // *WILL* be followed by tab-selection-changed.
  // Will *NOT* be followed or preceded by tab-detached.
  chrome.tabs.onRemoved = new chrome.Event("tab-removed");

  //----------------------------------------------------------------------------

  // PageActions.
  chrome.pageActions = {};

  chrome.pageActions.enableForTab = function(pageActionId, action) {
    validate(arguments, arguments.callee.params);
    sendRequest(EnablePageAction, [pageActionId, action]);
  }

  chrome.pageActions.enableForTab.params = [
    chrome.types.str,
    {
      type: "object",
      properties: {
        tabId: chrome.types.pInt,
        url: chrome.types.str
      },
      optional: false
    }
  ];

  chrome.pageActions.disableForTab = function(pageActionId, action) {
    validate(arguments, arguments.callee.params);
    sendRequest(DisablePageAction, [pageActionId, action]);
  }

  chrome.pageActions.disableForTab.params = [
    chrome.types.str,
    {
      type: "object",
      properties: {
        tabId: chrome.types.pInt,
        url: chrome.types.str
      },
      optional: false
    }
  ];

  // Sends ({pageActionId, tabId, tabUrl}).
  chrome.pageActions.onExecute =
       new chrome.Event("page-action-executed");

  //----------------------------------------------------------------------------
  // Bookmarks
  chrome.bookmarks = {};

  chrome.bookmarks.get = function(ids, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetBookmarks, ids, callback);
  };

  chrome.bookmarks.get.params = [
    chrome.types.singleOrListOf(chrome.types.pInt),
    chrome.types.fun
  ];

  chrome.bookmarks.getChildren = function(id, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetBookmarkChildren, id, callback);
  };

  chrome.bookmarks.getChildren.params = [
    chrome.types.pInt,
    chrome.types.fun
  ];

  chrome.bookmarks.getTree = function(callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetBookmarkTree, null, callback);
  };

  // TODO(erikkay): allow it to take an optional id as a starting point
  // BUG=13727
  chrome.bookmarks.getTree.params = [
    chrome.types.fun
  ];

  chrome.bookmarks.search = function(query, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(SearchBookmarks, query, callback);
  };

  chrome.bookmarks.search.params = [
    chrome.types.str,
    chrome.types.fun
  ];

  chrome.bookmarks.remove = function(id, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(RemoveBookmark, [id, false], callback);
  };

  chrome.bookmarks.remove.params = [
    chrome.types.singleOrListOf(chrome.types.pInt),
    chrome.types.optFun
  ];

  chrome.bookmarks.removeTree = function(id, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(RemoveBookmark, [id, true], callback);
  };

  chrome.bookmarks.removeTree.params = [
    chrome.types.pInt,
    chrome.types.optFun
  ];

  chrome.bookmarks.create = function(bookmark, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(CreateBookmark, bookmark, callback);
  };

  chrome.bookmarks.create.params = [
    {
      type: "object",
      properties: {
        parentId: chrome.types.optPInt,
        index: chrome.types.optPInt,
        title: chrome.types.optStr,
        url: chrome.types.optStr,
      }
    },
    chrome.types.optFun
  ];

  chrome.bookmarks.move = function(id, destination, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(MoveBookmark, [id, destination], callback);
  };

  chrome.bookmarks.move.params = [
    chrome.types.pInt,
    {
      type: "object",
      properties: {
        parentId: chrome.types.optPInt,
        index: chrome.types.optPInt
      }
    },
    chrome.types.optFun
  ];

  chrome.bookmarks.update = function(id, changes, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(SetBookmarkTitle, [id, changes], callback);
  };

  chrome.bookmarks.update.params = [
    chrome.types.pInt,
    {
      type: "object",
      properties: {
        title: chrome.types.optStr
      }
    },
    chrome.types.optFun
  ];

  // bookmark events

  // Sends (id, {title, url, parentId, index, dateAdded, dateGroupModified})
  // date values are milliseconds since the UTC epoch.
  chrome.bookmarks.onAdded = new chrome.Event("bookmark-added");

  // Sends (id, {parentId, index})
  chrome.bookmarks.onRemoved = new chrome.Event("bookmark-removed");

  // Sends (id, object) where object has list of properties that have changed.
  // Currently, this only ever includes 'title'.
  chrome.bookmarks.onChanged = new chrome.Event("bookmark-changed");

  // Sends (id, {parentId, index, oldParentId, oldIndex})
  chrome.bookmarks.onMoved = new chrome.Event("bookmark-moved");

  // Sends (id, [childrenIds])
  chrome.bookmarks.onChildrenReordered =
      new chrome.Event("bookmark-children-reordered");


  //----------------------------------------------------------------------------

  // Self.
  chrome.self = chrome.self || {};
  chrome.self.onConnect = new chrome.Event("channel-connect");

  chrome.self.getViews = function() {
    return GetViews();
  }
})();
