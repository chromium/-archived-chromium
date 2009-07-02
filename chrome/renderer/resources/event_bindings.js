// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// -----------------------------------------------------------------------------
// NOTE: If you change this file you need to touch renderer_resources.grd to
// have your change take effect.
// -----------------------------------------------------------------------------

var chrome = chrome || {};
(function () {
  native function GetChromeHidden();
  native function AttachEvent(eventName);
  native function DetachEvent(eventName);
  native function GetNextRequestId();

  var chromeHidden = GetChromeHidden();

  // Event object.  If opt_eventName is provided, this object represents
  // the unique instance of that named event, and dispatching an event
  // with that name will route through this object's listeners.
  //
  // Example:
  //   chrome.tabs.onChanged = new chrome.Event("tab-changed");
  //   chrome.tabs.onChanged.addListener(function(data) { alert(data); });
  //   chromeHidden.Event.dispatch("tab-changed", "hi");
  // will result in an alert dialog that says 'hi'.
  chrome.Event = function(opt_eventName) {
    this.eventName_ = opt_eventName;
    this.listeners_ = [];
  };

  // A map of event names to the event object that is registered to that name.
  var attachedNamedEvents = {};

  // An array of all attached event objects, used for detaching on unload.
  var allAttachedEvents = [];

  chromeHidden.Event = {};

  // Dispatches a named event with the given JSON array, which is deserialized
  // before dispatch. The JSON array is the list of arguments that will be
  // sent with the event callback.
  chromeHidden.Event.dispatchJSON = function(name, args) {
    if (attachedNamedEvents[name]) {
      if (args) {
        args = JSON.parse(args);
      }
      attachedNamedEvents[name].dispatch.apply(
          attachedNamedEvents[name], args);
    }
  };

  // Dispatches a named event with the given arguments, supplied as an array.
  chromeHidden.Event.dispatch = function(name, args) {
    if (attachedNamedEvents[name]) {
      attachedNamedEvents[name].dispatch.apply(
          attachedNamedEvents[name], args);
    }
  };

  // Test if a named event has any listeners.
  chromeHidden.Event.hasListener = function(name) {
    return (attachedNamedEvents[name] &&
            attachedNamedEvents[name].listeners_.length > 0);
  }

  // Registers a callback to be called when this event is dispatched.
  chrome.Event.prototype.addListener = function(cb) {
    this.listeners_.push(cb);
    if (this.listeners_.length == 1) {
      this.attach_();
    }
  };

  // Unregisters a callback.
  chrome.Event.prototype.removeListener = function(cb) {
    var idx = this.findListener_(cb);
    if (idx == -1) {
      return;
    }

    this.listeners_.splice(idx, 1);
    if (this.listeners_.length == 0) {
      this.detach_();
    }
  };

  // Test if the given callback is registered for this event.
  chrome.Event.prototype.hasListener = function(cb) {
    return this.findListeners_(cb) > -1;
  };

  // Returns the index of the given callback if registered, or -1 if not
  // found.
  chrome.Event.prototype.findListener_ = function(cb) {
    for (var i = 0; i < this.listeners_.length; i++) {
      if (this.listeners_[i] == cb) {
        return i;
      }
    }

    return -1;
  };

  // Dispatches this event object to all listeners, passing all supplied
  // arguments to this function each listener.
  chrome.Event.prototype.dispatch = function(varargs) {
    var args = Array.prototype.slice.call(arguments);
    for (var i = 0; i < this.listeners_.length; i++) {
      try {
        this.listeners_[i].apply(null, args);
      } catch (e) {
        console.error(e);
      }
    }
  };

  // Attaches this event object to its name.  Only one object can have a given
  // name.
  chrome.Event.prototype.attach_ = function() {
    AttachEvent(this.eventName_);
    allAttachedEvents[allAttachedEvents.length] = this;
    if (!this.eventName_)
      return;

    if (attachedNamedEvents[this.eventName_]) {
      throw new Error("chrome.Event '" + this.eventName_ +
                      "' is already attached.");
    }

    attachedNamedEvents[this.eventName_] = this;
  };

  // Detaches this event object from its name.
  chrome.Event.prototype.detach_ = function() {
    var i = allAttachedEvents.indexOf(this);
    if (i >= 0)
      delete allAttachedEvents[i];
    DetachEvent(this.eventName_);
    if (!this.eventName_)
      return;

    if (!attachedNamedEvents[this.eventName_]) {
      throw new Error("chrome.Event '" + this.eventName_ +
                      "' is not attached.");
    }

    delete attachedNamedEvents[this.eventName_];
  };

  // Callback handling.
  var callbacks = [];
  chromeHidden.handleResponse = function(requestId, name,
                                         success, response, error) {
    try {
      if (!success) {
        if (!error)
          error = "Unknown error."
        console.error("Error during " + name + ": " + error);
        return;
      }

      if (callbacks[requestId]) {
        if (response) {
          callbacks[requestId](JSON.parse(response));
        } else {
          callbacks[requestId]();
        }
      }
    } finally {
      delete callbacks[requestId];
    }
  };

  // Send an API request and optionally register a callback.
  chromeHidden.sendRequest = function(request, args, callback) {
    // JSON.stringify doesn't support a root object which is undefined.
    if (args === undefined)
      args = null;
    var sargs = JSON.stringify(args);
    var requestId = GetNextRequestId();
    var hasCallback = false;
    if (callback) {
      hasCallback = true;
      callbacks[requestId] = callback;
    }
    request(sargs, requestId, hasCallback);
  }

  // Special load events: we don't use the DOM unload because that slows
  // down tab shutdown.  On the other hand, onUnload might not always fire,
  // since Chrome will terminate renderers on shutdown (SuddenTermination).
  chromeHidden.onLoad = new chrome.Event();
  chromeHidden.onUnload = new chrome.Event();

  chromeHidden.dispatchOnLoad = function(extensionId) {
    chromeHidden.onLoad.dispatch(extensionId);
  }

  chromeHidden.dispatchOnUnload = function() {
    chromeHidden.onUnload.dispatch();
    for (var i in allAttachedEvents)
      allAttachedEvents[i].detach_();
  }
})();
