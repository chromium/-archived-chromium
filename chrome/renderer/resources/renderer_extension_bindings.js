var chromium = chromium || {};
(function () {
  native function OpenChannelToExtension(id);
  native function PostMessage(portId, msg);
  native function RegisterScriptAPI(private);
  // chromium: Public API.

  // Represents info we know about a chrome extension.
  chromium.Extension = function(id) {
    this.id_ = id;
  };

  // Opens a channel to the extension for message passing.
  chromium.Extension.prototype.openChannel = function() {
    portId = OpenChannelToExtension(this.id_);
    if (portId == -1)
      throw new Error('No such extension \"' + this.id_ + '\"');
    return new Port(portId);
  };

  // Adds a listener that fires when a renderer opens a channel to talk
  // to us.
  chromium.addConnectListener = function(callback) {
    chromium.addEventListener('channel-connect',
        function (e) { callback(e.data.port); });
  };

  // Adds a generic event listener.
  chromium.addEventListener = function(type, callback) {
    var listeners = getPrivateData().eventListeners;
    if (!listeners[type])
      listeners[type] = [];
    listeners[type].push(callback);
  };

  // Dispatches the given event to anyone listening for that event.
  chromium.dispatchEvent = function(type, data) {
    var event = {type: type, data: data};
    var listeners = getPrivateData().eventListeners;
    for (var i in listeners[type]) {
      listeners[type][i](event);
    }
  };

  // Private API.

  // Always access privateData through this function, to ensure that we
  // have registered our native API.  We do this lazily to avoid registering
  // on pages that don't use these bindings.
  function getPrivateData() {
    if (!scriptAPI.registered_) {
      RegisterScriptAPI(scriptAPI);
      scriptAPI.registered_ = true;
    }
    return privateData;
  }
  var privateData = {
    eventListeners: {},
    ports: {}
  };

  // Represents a port through which we can send messages to another process.
  var Port = function(portId) {
    // TODO(mpcomplete): we probably want to hide this portId_ so
    // it can't be guessed at.  One idea is to expose v8's SetHiddenValue
    // to our extension.
    this.portId_ = portId;
    getPrivateData().ports[portId] = this;
  };

  // Sends a message to the other side of the channel.
  Port.prototype.postMessage = function(msg) {
    PostMessage(this.portId_, msg);
  };

  // Script API: javascript APIs exposed to C++ only.
  // This object allows our native code to call back to us through a
  // private interface that isn't exposed to web content.
  var scriptAPI = {};

  // Called by native code when a channel has been opened to this process.
  scriptAPI.dispatchOnConnect = function(portId) {
    chromium.dispatchEvent('channel-connect', {port: new Port(portId)});
  };

  // Called by native code when a message has been sent over the given
  // channel.
  scriptAPI.dispatchOnMessage = function(msg, portId) {
    var port = getPrivateData().ports[portId];
    if (port && port.onMessage)
      port.onMessage(msg, port);
  };
})();
