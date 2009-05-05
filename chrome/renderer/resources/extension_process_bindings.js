// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// -----------------------------------------------------------------------------
// NOTE: If you change this file you need to touch renderer_resources.grd to
// have your change take effect.
// -----------------------------------------------------------------------------

var chromium;
(function() {
  native function GetNextCallbackId();
  native function GetWindow();
  native function GetCurrentWindow();
  native function GetFocusedWindow();
  native function CreateWindow();
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
  native function GetBookmarks();
  native function GetBookmarkChildren();
  native function GetBookmarkTree();
  native function SearchBookmarks();
  native function RemoveBookmark();
  native function CreateBookmark();
  native function MoveBookmark();
  native function SetBookmarkTitle();

  if (!chromium)
    chromium = {};

  // Validate arguments.
  function validate(args, schemas) {
    if (args.length > schemas.length)
      throw new Error("Too many arguments.");

    for (var i = 0; i < schemas.length; i++) {
      if (i in args && args[i] !== null && args[i] !== undefined) {
        var validator = new chromium.JSONSchemaValidator();
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

  // Callback handling.
  // TODO(aa): This function should not be publicly exposed. Pass it into V8
  // instead and hold one per-context. See the way event_bindings.js works.
  var callbacks = [];
  chromium.dispatchCallback_ = function(callbackId, str) {
    try {
      if (str) {
        callbacks[callbackId](goog.json.parse(str));
      } else {
        callbacks[callbackId]();
      }
    } finally {
      delete callbacks[callbackId];
    }
  };

  // Send an API request and optionally register a callback.
  function sendRequest(request, args, callback) {
    var sargs = goog.json.serialize(args);
    var callbackId = -1;
    if (callback) {
      callbackId = GetNextCallbackId();
      callbacks[callbackId] = callback;
    }
    request(sargs, callbackId);
  }

  //----------------------------------------------------------------------------

  // Windows.
  chromium.windows = {};

  chromium.windows.get = function(windowId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetWindow, windowId, callback);
  };

  chromium.windows.get.params = [
    chromium.types.pInt,
    chromium.types.fun
  ];
  
  chromium.windows.getCurrent = function(callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetCurrentWindow, null, callback);
  };

  chromium.windows.getCurrent.params = [
    chromium.types.fun
  ];
  
  chromium.windows.getFocused = function(callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetFocusedWindow, null, callback);
  };

  chromium.windows.getFocused.params = [
    chromium.types.fun
  ];

  chromium.windows.getAll = function(populate, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetAllWindows, populate, callback);
  };

  chromium.windows.getAll.params = [
    chromium.types.optBool,
    chromium.types.fun
  ];
  
  chromium.windows.createWindow = function(createData, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(CreateWindow, createData, callback);
  };
  chromium.windows.createWindow.params = [
    {
      type: "object",
      properties: {
        url: chromium.types.optStr,
        left: chromium.types.optInt,
        top: chromium.types.optInt,
        width: chromium.types.optPInt,
        height: chromium.types.optPInt
      },
      optional: true
    },
    chromium.types.optFun
  ];
  
  chromium.windows.removeWindow = function(windowId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(RemoveWindow, windowId, callback);
  };

  chromium.windows.removeWindow.params = [
    chromium.types.pInt,
    chromium.types.optFun
  ];
  
  // sends (windowId).
  // *WILL* be followed by tab-attached AND then tab-selection-changed.
  chromium.windows.onCreated = new chromium.Event("window-created");

  // sends (windowId).
  // *WILL* be preceded by sequences of tab-removed AND then
  // tab-selection-changed -- one for each tab that was contained in the window
  // that closed
  chromium.windows.onRemoved = new chromium.Event("window-removed");
  
  // sends (windowId).
  chromium.windows.onFocusChanged =
        new chromium.Event("window-focus-changed");

  //----------------------------------------------------------------------------

  // Tabs
  chromium.tabs = {};

  chromium.tabs.get = function(tabId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetTab, tabId, callback);
  };

  chromium.tabs.get.params = [
    chromium.types.pInt,
    chromium.types.fun
  ];
  
  chromium.tabs.getSelected = function(windowId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetSelectedTab, windowId, callback);
  };

  chromium.tabs.getSelected.params = [
    chromium.types.optPInt,
    chromium.types.fun
  ];

  chromium.tabs.getAllInWindow = function(windowId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetAllTabsInWindow, windowId, callback);
  };

  chromium.tabs.getAllInWindow.params = [
    chromium.types.optPInt,
    chromium.types.fun
  ];

  chromium.tabs.create = function(tab, callback) {  
    validate(arguments, arguments.callee.params);
    sendRequest(CreateTab, tab, callback);
  };

  chromium.tabs.create.params = [
    {
      type: "object",
      properties: {
        windowId: chromium.types.optPInt,
        index: chromium.types.optPInt,
        url: chromium.types.optStr,
        selected: chromium.types.optBool
      }
    },
    chromium.types.optFun
  ];

  chromium.tabs.update = function(tabId, updates, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(UpdateTab, [tabId, updates], callback);
  };

  chromium.tabs.update.params = [
    chromium.types.pInt,
    {
      type: "object",
      properties: {
        url: chromium.types.optStr,
        selected: chromium.types.optBool
      }
    },
    chromium.types.optFun
  ];

  chromium.tabs.move = function(tabId, moveProps, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(MoveTab, [tabId, moveProps], callback);
  };

  chromium.tabs.move.params = [
    chromium.types.pInt,
    {
      type: "object",
      properties: {
        windowId: chromium.types.optPInt,
        index: chromium.types.pInt
      }
    },
    chromium.types.optFun
  ];
  
  chromium.tabs.remove = function(tabId) {
    validate(arguments, arguments.callee.params);
    sendRequest(RemoveTab, tabId);
  };

  chromium.tabs.remove.params = [
    chromium.types.pInt
  ];

  // Sends ({Tab}).
  // Will *NOT* be followed by tab-attached - it is implied.
  // *MAY* be followed by tab-selection-changed.
  chromium.tabs.onCreated = new chromium.Event("tab-created");
  
  // Sends (tabId, {windowId, fromIndex, toIndex}).
  // Tabs can only "move" within a window.
  chromium.tabs.onMoved = new chromium.Event("tab-moved");
 
  // Sends (tabId, {windowId}).
  chromium.tabs.onSelectionChanged = 
       new chromium.Event("tab-selection-changed");
   
  // Sends (tabId, {newWindowId, newPosition}).
  // *MAY* be followed by tab-selection-changed.
  chromium.tabs.onAttached = new chromium.Event("tab-attached");
  
  // Sends (tabId, {oldWindowId, oldPosition}).
  // *WILL* be followed by tab-selection-changed.
  chromium.tabs.onDetached = new chromium.Event("tab-detached");
  
  // Sends (tabId).
  // *WILL* be followed by tab-selection-changed.
  // Will *NOT* be followed or preceded by tab-detached.
  chromium.tabs.onRemoved = new chromium.Event("tab-removed");

  //----------------------------------------------------------------------------

  // PageActions.
  chromium.pageActions = {};

  chromium.pageActions.enableForTab = function(pageActionId, action) {
    validate(arguments, arguments.callee.params);
    sendRequest(EnablePageAction, [pageActionId, action]);
  }

  chromium.pageActions.enableForTab.params = [  
    chromium.types.str,
    {
      type: "object",
      properties: {
        tabId: chromium.types.pInt,
        url: chromium.types.str
      },
      optional: false
    }
  ];

  // Sends ({pageActionId, tabId, tabUrl}).
  chromium.pageActions.onExecute =
       new chromium.Event("page-action-executed");

  //----------------------------------------------------------------------------
  // Bookmarks
  chromium.bookmarks = {};

  chromium.bookmarks.get = function(ids, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetBookmarks, ids, callback);
  };

  chromium.bookmarks.get.params = [
    {
      type: "array",
      items: chromium.types.pInt,
      optional: true
    },
    chromium.types.fun
  ];
  
  chromium.bookmarks.getChildren = function(id, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetBookmarkChildren, id, callback);
  };

  chromium.bookmarks.getChildren.params = [
    chromium.types.pInt,
    chromium.types.fun
  ];
  
  chromium.bookmarks.getTree = function(callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetBookmarkTree, null, callback);
  };
  
  // TODO(erikkay): allow it to take an optional id as a starting point
  chromium.bookmarks.getTree.params = [
    chromium.types.fun
  ];

  chromium.bookmarks.search = function(query, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(SearchBookmarks, query, callback);
  };

  chromium.bookmarks.search.params = [
    chromium.types.str,
    chromium.types.fun
  ];

  chromium.bookmarks.remove = function(bookmark, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(RemoveBookmark, bookmark, callback);
  };

  chromium.bookmarks.remove.params = [
    {
      type: "object",
      properties: {
        id: chromium.types.pInt,
        recursive: chromium.types.optBool
      }
    },
    chromium.types.optFun
  ];

  chromium.bookmarks.create = function(bookmark, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(CreateBookmark, bookmark, callback);
  };

  chromium.bookmarks.create.params = [
    {
      type: "object",
      properties: {
        parentId: chromium.types.optPInt,
        index: chromium.types.optPInt,
        title: chromium.types.optStr,
        url: chromium.types.optStr,
      }
    },
    chromium.types.optFun
  ];

  chromium.bookmarks.move = function(obj, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(MoveBookmark, obj, callback);
  };

  chromium.bookmarks.move.params = [
    {
      type: "object",
      properties: {
        id: chromium.types.pInt,
        parentId: chromium.types.optPInt,
        index: chromium.types.optPInt
      }
    },
    chromium.types.optFun
  ];

  chromium.bookmarks.setTitle = function(bookmark, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(SetBookmarkTitle, bookmark, callback);
  };

  chromium.bookmarks.setTitle.params = [
    {
      type: "object",
      properties: {
        id: chromium.types.pInt,
        title: chromium.types.optStr
      }
    },
    chromium.types.optFun
  ];

  // bookmark events

  // Sends ({id, title, url, parentId, index})
  chromium.bookmarks.onBookmarkAdded = new chromium.Event("bookmark-added");

  // Sends ({parentId, index})
  chromium.bookmarks.onBookmarkRemoved = new chromium.Event("bookmark-removed");

  // Sends (id, object) where object has list of properties that have changed.
  // Currently, this only ever includes 'title'.
  chromium.bookmarks.onBookmarkChanged = new chromium.Event("bookmark-changed");

  // Sends ({id, parentId, index, oldParentId, oldIndex})
  chromium.bookmarks.onBookmarkMoved = new chromium.Event("bookmark-moved");
  
  // Sends (id, [childrenIds])
  chromium.bookmarks.onBookmarkChildrenReordered =
      new chromium.Event("bookmark-children-reordered");


  //----------------------------------------------------------------------------

  // Self.
  chromium.self = {};
  chromium.self.onConnect = new chromium.Event("channel-connect");
})();

