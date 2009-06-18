// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// -----------------------------------------------------------------------------
// NOTE: If you change this file you need to touch renderer_resources.grd to
// have your change take effect.
// -----------------------------------------------------------------------------

var chrome = chrome || {};
(function () {
  native function OpenChannelToExtension(id);
  native function CloseChannel(portId);
  native function PostMessage(portId, msg);

  // Port object.  Represents a connection to another script context through
  // which messages can be passed.
  chrome.Port = function(portId) {
    if (chrome.Port.ports_[portId]) {
      throw new Error("Port '" + portId + "' already exists.");
    }
    this.portId_ = portId;  // TODO(mpcomplete): readonly
    this.onDisconnect = new chrome.Event();
    this.onMessage = new chrome.Event();
    chrome.Port.ports_[portId] = this;
  };

  // Map of port IDs to port object.
  chrome.Port.ports_ = {};

  // Called by native code when a channel has been opened to this context.
  chrome.Port.dispatchOnConnect_ = function(portId, tab) {
    var port = new chrome.Port(portId);
    if (tab) {
      tab = JSON.parse(tab);
    }
    port.tab = tab;
    chrome.Event.dispatch_("channel-connect", [port]);
  };

  // Called by native code when a channel has been closed.
  chrome.Port.dispatchOnDisconnect_ = function(portId) {
    var port = chrome.Port.ports_[portId];
    if (port) {
      port.onDisconnect.dispatch(port);
      delete chrome.Port.ports_[portId];
    }
  };

  // Called by native code when a message has been sent to the given port.
  chrome.Port.dispatchOnMessage_ = function(msg, portId) {
    var port = chrome.Port.ports_[portId];
    if (port) {
      if (msg) {
        msg = JSON.parse(msg);
      }
      port.onMessage.dispatch(msg, port);
    }
  };

  // Sends a message asynchronously to the context on the other end of this
  // port.
  chrome.Port.prototype.postMessage = function(msg) {
    // JSON.stringify doesn't support a root object which is undefined.
    if (msg === undefined)
      msg = null;
    PostMessage(this.portId_, JSON.stringify(msg));
  };

  // Disconnects the port from the other end.
  chrome.Port.prototype.disconnect = function() {
    delete chrome.Port.ports_[this.portId_];
    CloseChannel(this.portId_);
  }

  // Extension object.
  chrome.Extension = function(id) {
    this.id_ = id;
  };

  // Opens a message channel to the extension.  Returns a Port for
  // message passing.
  chrome.Extension.prototype.connect = function() {
    var portId = OpenChannelToExtension(this.id_);
    if (portId == -1)
      throw new Error("No such extension: '" + this.id_ + "'");
    return new chrome.Port(portId);
  };

  // Returns a resource URL that can be used to fetch a resource from this
  // extension.
  chrome.Extension.prototype.getURL = function(path) {
    return "chrome-extension://" + this.id_ + "/" + path;
  };

  chrome.onUnload_.addListener(function() {
    for (var i in chrome.Port.ports_) {
      chrome.Port.ports_[i].disconnect();
    }
  });
})();
