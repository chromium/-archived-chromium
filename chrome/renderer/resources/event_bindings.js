// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// -----------------------------------------------------------------------------
// NOTE: If you change this file you need to touch renderer_resources.grd to
// have your change take effect.
// -----------------------------------------------------------------------------

var chrome = chrome || {};
(function () {
  native function AttachEvent(eventName);
  native function DetachEvent(eventName);

  // Event object.  If opt_eventName is provided, this object represents
  // the unique instance of that named event, and dispatching an event
  // with that name will route through this object's listeners.
  //
  // Example:
  //   chrome.tabs.onChanged = new chrome.Event("tab-changed");
  //   chrome.tabs.onChanged.addListener(function(data) { alert(data); });
  //   chrome.Event.dispatch_("tab-changed", "hi");
  // will result in an alert dialog that says 'hi'.
  chrome.Event = function(opt_eventName) {
    this.eventName_ = opt_eventName;
    this.listeners_ = [];
  };

  // A map of event names to the event object that is registered to that name.
  chrome.Event.attached_ = {};

  // An array of all attached event objects, used for detaching on unload.
  chrome.Event.allAttached_ = [];

  // Dispatches a named event with the given JSON array, which is deserialized
  // before dispatch. The JSON array is the list of arguments that will be
  // sent with the event callback.
  chrome.Event.dispatchJSON_ = function(name, args) {
    if (chrome.Event.attached_[name]) {
      if (args) {
        args = JSON.parse(args);
      }
      chrome.Event.attached_[name].dispatch.apply(
          chrome.Event.attached_[name], args);
    }
  };

  // Dispatches a named event with the given arguments, supplied as an array.
  chrome.Event.dispatch_ = function(name, args) {
    if (chrome.Event.attached_[name]) {
      chrome.Event.attached_[name].dispatch.apply(
          chrome.Event.attached_[name], args);
    }
  };

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
    chrome.Event.allAttached_[chrome.Event.allAttached_.length] = this;
    if (!this.eventName_)
      return;

    if (chrome.Event.attached_[this.eventName_]) {
      throw new Error("chrome.Event '" + this.eventName_ +
                      "' is already attached.");
    }

    chrome.Event.attached_[this.eventName_] = this;
  };

  // Detaches this event object from its name.
  chrome.Event.prototype.detach_ = function() {
    var i = chrome.Event.allAttached_.indexOf(this);
    if (i >= 0)
      delete chrome.Event.allAttached_[i];
    DetachEvent(this.eventName_);
    if (!this.eventName_)
      return;

    if (!chrome.Event.attached_[this.eventName_]) {
      throw new Error("chrome.Event '" + this.eventName_ +
                      "' is not attached.");
    }

    delete chrome.Event.attached_[this.eventName_];
  };

  // Load events.  Note that onUnload_ might not always fire, since Chrome will
  // terminate renderers on shutdown.
  chrome.onLoad_ = new chrome.Event();
  chrome.onUnload_ = new chrome.Event();

  // This is called by native code when the DOM is ready.
  chrome.dispatchOnLoad_ = function() {
    chrome.onLoad_.dispatch();
    delete chrome.dispatchOnLoad_;
  }

  chrome.dispatchOnUnload_ = function() {
    chrome.onUnload_.dispatch();
    for (var i in chrome.Event.allAttached_)
      chrome.Event.allAttached_[i].detach_();
  }
})();
