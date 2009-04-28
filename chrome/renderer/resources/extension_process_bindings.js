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
  native function CreateWindow();
  native function GetWindows();
  native function GetTabsForWindow();
  native function GetTab();
  native function CreateTab();
  native function UpdateTab();
  native function MoveTab();
  native function RemoveTab();
  native function GetBookmarks();
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

  // callback handling
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

  // Tabs
  chromium.tabs = {};

  chromium.tabs.getWindows = function(windowQuery, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetWindows, windowQuery, callback);
  };

  chromium.tabs.getWindows.params = [
    {
      type: "object",
      properties: {
        ids: {
          type: "array",
          items: chromium.types.pInt,
          minItems: 1
        }
      },
      optional: true
    },
    chromium.types.optFun
  ];
  
  chromium.tabs.createWindow = function(createData, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(CreateWindow, createData, callback);
  };
  chromium.tabs.createWindow.params = [
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

  // TODO(aa): This should eventually take an optional windowId param.
  chromium.tabs.getTabsForWindow = function(callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetTabsForWindow, null, callback);
  };

  chromium.tabs.getTabsForWindow.params = [
    chromium.types.optFun
  ];

  chromium.tabs.getTab = function(tabId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetTab, tabId, callback);
  };

  chromium.tabs.getTab.params = [
    chromium.types.pInt,
    chromium.types.optFun
  ];

  chromium.tabs.createTab = function(tab, callback) {  
    validate(arguments, arguments.callee.params);
    sendRequest(CreateTab, tab, callback);
  };

  chromium.tabs.createTab.params = [
    {
      type: "object",
      properties: {
        windowId: chromium.types.optPInt,
        url: chromium.types.optStr,
        selected: chromium.types.optBool
      }
    },
    chromium.types.optFun
  ];

  chromium.tabs.updateTab = function(tab) {
    validate(arguments, arguments.callee.params);
    sendRequest(UpdateTab, tab);
  };

  chromium.tabs.updateTab.params = [
    {
      type: "object",
      properties: {
        id: chromium.types.pInt,
        windowId: chromium.types.optPInt,
        url: chromium.types.optStr,
        selected: chromium.types.optBool
      }
    }
  ];

  chromium.tabs.moveTab = function(tab) {
    validate(arguments, arguments.callee.params);
    sendRequest(MoveTab, tab);
  };

  chromium.tabs.moveTab.params = [
    {
      type: "object",
      properties: {
        id: chromium.types.pInt,
        windowId: chromium.types.optPInt,
        index: chromium.types.pInt
      }
    }
  ];
  
  chromium.tabs.removeTab = function(tabId) {
    validate(arguments, arguments.callee.params);
    sendRequest(RemoveTab, tabId);
  };

  chromium.tabs.removeTab.params = [
    chromium.types.pInt
  ];

  // sends ({tabId, windowId, index}).
  // will *NOT* be followed by tab-attached - it is implied.
  // *MAY* be followed by tab-selection-changed.
  chromium.tabs.onTabCreated = new chromium.Event("tab-created");
  
  // sends ({tabId, windowId, fromIndex, toIndex}).
  // tabs can only "move" within a window.
  chromium.tabs.onTabMoved = new chromium.Event("tab-moved");
 
  // sends ({tabId, windowId, index}).
  chromium.tabs.onTabSelectionChanged = 
       new chromium.Event("tab-selection-changed");
   
  // sends ({tabId, windowId, index}).
  // *MAY* be followed by tab-selection-changed.
  chromium.tabs.onTabAttached = new chromium.Event("tab-attached");
  
  // sends ({tabId, windowId, index}).
  // *WILL* be followed by tab-selection-changed.
  chromium.tabs.onTabDetached = new chromium.Event("tab-detached");
  
  // sends (tabId).
  // *WILL* be followed by tab-selection-changed.
  // will *NOT* be followed or preceded by tab-detached.
  chromium.tabs.onTabRemoved = new chromium.Event("tab-removed");

  //----------------------------------------------------------------------------

  // Bookmarks
  // TODO(erikkay): Call validate() in these functions.
  chromium.bookmarks = {};

  chromium.bookmarks.get = function(ids, callback) {
    sendRequest(GetBookmarks, ids, callback);
  };

  chromium.bookmarks.get.params = [
    {
      type: "array",
      items: {
        type: chromium.types.pInt
      },
      minItems: 1,
      optional: true
    },
    chromium.types.optFun
  ];

  chromium.bookmarks.search = function(query, callback) {
    sendRequest(SearchBookmarks, query, callback);
  };

  chromium.bookmarks.search.params = [
    chromium.types.string,
    chromium.types.optFun
  ];

  chromium.bookmarks.remove = function(bookmark, callback) {
    sendRequest(RemoveBookmark, bookmark, callback);
  };

  chromium.bookmarks.remove.params = [
    {
      type: "object",
      properties: {
        id: chromium.types.pInt,
        recursive: chromium.types.bool
      }
    },
    chromium.types.optFun
  ];

  chromium.bookmarks.create = function(bookmark, callback) {
    sendRequest(CreateBookmark, bookmark, callback);
  };

  chromium.bookmarks.create.params = [
    {
      type: "object",
      properties: {
        parentId: chromium.types.optPInt,
        index: chromium.types.optPInt,
        title: chromium.types.optString,
        url: chromium.types.optString,
      }
    },
    chromium.types.optFun
  ];

  chromium.bookmarks.move = function(obj, callback) {
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
    sendRequest(SetBookmarkTitle, bookmark, callback);
  };

  chromium.bookmarks.setTitle.params = [
    {
      type: "object",
      properties: {
        id: chromium.types.pInt,
        title: chromium.types.optString
      }
    },
    chromium.types.optFun
  ];
  
  //----------------------------------------------------------------------------

  // Self
  chromium.self = {};
  chromium.self.onConnect = new chromium.Event("channel-connect");
})();

