// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// -----------------------------------------------------------------------------
// NOTE: If you change this file you need to touch renderer_resources.grd to
// have your change take effect.
// -----------------------------------------------------------------------------

var chromium = chromium || {};
(function () {
  native function OpenChannelToExtension(id);
  native function PostMessage(portId, msg);

  // Port object.  Represents a connection to another script context through
  // which messages can be passed.
  chromium.Port = function(portId) {
    if (chromium.Port.ports_[portId]) {
      throw new Error("Port '" + portId + "' already exists.");
    }
    this.portId_ = portId;  // TODO(mpcomplete): readonly
    this.onMessage = new chromium.Event();
    chromium.Port.ports_[portId] = this;
    // Note: this object will never get GCed.  If we ever care, we could
    // add an "ondetach" method to the onMessage Event that gets called
    // when there are no more listeners.
  };

  // Map of port IDs to port object.
  chromium.Port.ports_ = {};

  // Called by native code when a channel has been opened to this context.
  chromium.Port.dispatchOnConnect_ = function(portId, tab) {
    var port = new chromium.Port(portId);
    if (tab) {
      tab = goog.json.parse(tab);
    }
    port.tab = tab;
    chromium.Event.dispatch_("channel-connect", [port]);
  };

  // Called by native code when a message has been sent to the given port.
  chromium.Port.dispatchOnMessage_ = function(msg, portId) {
    var port = chromium.Port.ports_[portId];
    if (port) {
      if (msg) {
        msg = goog.json.parse(msg);
      }
      port.onMessage.dispatch(msg, port);
    }
  };

  // Sends a message asynchronously to the context on the other end of this
  // port.
  chromium.Port.prototype.postMessage = function(msg) {
    PostMessage(this.portId_, goog.json.serialize(msg));
  };

  // Extension object.
  chromium.Extension = function(id) {
    this.id_ = id;
  };

  // Opens a message channel to the extension.  Returns a Port for
  // message passing.
  chromium.Extension.prototype.connect = function() {
    var portId = OpenChannelToExtension(this.id_);
    if (portId == -1)
      throw new Error("No such extension: '" + this.id_ + "'");
    return new chromium.Port(portId);
  };

  // Returns a resource URL that can be used to fetch a resource from this
  // extension.
  chromium.Extension.prototype.getURL = function(path) {
    return "chrome-extension://" + this.id_ + "/" + path;
  };
})();
