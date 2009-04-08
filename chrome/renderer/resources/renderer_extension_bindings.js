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
    this.onmessage = new chromium.Event();
    chromium.Port.ports_[portId] = this;
    // Note: this object will never get GCed.  If we ever care, we could
    // add an "ondetach" method to the onmessage Event that gets called
    // when there are no more listeners.
  };

  // Map of port IDs to port object.
  chromium.Port.ports_ = {};

  // Called by native code when a channel has been opened to this context.
  chromium.Port.dispatchOnConnect_ = function(portId) {
    var port = new chromium.Port(portId);
    chromium.Event.dispatch_("channel-connect", port);
  };

  // Called by native code when a message has been sent to the given port.
  chromium.Port.dispatchOnMessage_ = function(msg, portId) {
    var port = chromium.Port.ports_[portId];
    if (port) {
      port.onmessage.dispatch(msg, port);
    }
  };

  // Sends a message asynchronously to the context on the other end of this
  // port.
  chromium.Port.prototype.postMessage = function(msg) {
    PostMessage(this.portId_, msg);
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

  chromium.onconnect = new chromium.Event("channel-connect");
})();
