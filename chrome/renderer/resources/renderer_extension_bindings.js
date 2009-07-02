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
  native function GetChromeHidden();

  var chromeHidden = GetChromeHidden();

  // Map of port IDs to port object.
  var ports = {};

  // Port object.  Represents a connection to another script context through
  // which messages can be passed.
  chrome.Port = function(portId) {
    if (ports[portId]) {
      throw new Error("Port '" + portId + "' already exists.");
    }
    this.portId_ = portId;  // TODO(mpcomplete): readonly
    this.onDisconnect = new chrome.Event();
    this.onMessage = new chrome.Event();
    ports[portId] = this;

    var port = this;
    chromeHidden.onUnload.addListener(function() {
      port.disconnect();
    });
  };

  chromeHidden.Port = {};

  // Called by native code when a channel has been opened to this context.
  chromeHidden.Port.dispatchOnConnect = function(portId, tab, extensionId) {
    // Only create a new Port if someone is actually listening for a connection.
    // In addition to being an optimization, this also fixes a bug where if 2
    // channels were opened to and from the same process, closing one would
    // close both.
    var connectEvent = "channel-connect:" + (extensionId || "");
    if (chromeHidden.Event.hasListener(connectEvent)) {
      var port = new chrome.Port(portId);
      if (tab) {
        tab = JSON.parse(tab);
      }
      port.tab = tab;
      chromeHidden.Event.dispatch(connectEvent, [port]);
    }
  };

  // Called by native code when a channel has been closed.
  chromeHidden.Port.dispatchOnDisconnect = function(portId) {
    var port = ports[portId];
    if (port) {
      port.onDisconnect.dispatch(port);
      delete ports[portId];
    }
  };

  // Called by native code when a message has been sent to the given port.
  chromeHidden.Port.dispatchOnMessage = function(msg, portId) {
    var port = ports[portId];
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
    delete ports[this.portId_];
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
})();
