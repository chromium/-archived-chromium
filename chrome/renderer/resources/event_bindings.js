var chromium = chromium || {};
(function () {
  native function AttachEvent(eventName);
  native function DetachEvent(eventName);

  // Event object.  If opt_eventName is provided, this object represents
  // the unique instance of that named event, and dispatching an event
  // with that name will route through this object's listeners.
  //
  // Example:
  //   chromium.ontabchanged = new Event('tabchanged');
  //   chromium.ontabchanged.addListener(function(data) { alert(data); });
  //   chromium.Event.dispatch_('tabchanged', 'hi');
  // will result in an alert dialog that says 'hi'.
  chromium.Event = function(opt_eventName) {
    this.eventName_ = opt_eventName;
    this.listeners_ = [];
  };

  // A map of event names to the event object that is registered to that name.
  chromium.Event.attached_ = {};

  // Dispatches a named event with the given JSON data, which is deserialized
  // before dispatch.
  chromium.Event.dispatchJSON_ = function(name, data) {
    if (chromium.Event.attached_[name]) {
      if (data) {
        data = chromium.json.deserialize_(data);
      }
      chromium.Event.attached_[name].dispatch_(data);
    }
  };

  // Dispatches a named event with the given object data.
  chromium.Event.dispatch_ = function(name, data) {
    if (chromium.Event.attached_[name]) {
      chromium.Event.attached_[name].dispatch(data);
    }
  };

  // Registers a callback to be called when this event is dispatched.
  chromium.Event.prototype.addListener = function(cb) {
    this.listeners_.push(cb);
    if (this.listeners_.length == 1) {
      this.attach_();
    }
  };

  // Unregisters a callback.
  chromium.Event.prototype.removeListener = function(cb) {
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
  chromium.Event.prototype.hasListener = function(cb) {
    return this.findListeners_(cb) > -1;
  };

  // Returns the index of the given callback if registered, or -1 if not
  // found.
  chromium.Event.prototype.findListener_ = function(cb) {
    for (var i = 0; i < this.listeners_.length; i++) {
      if (this.listeners_[i] == cb) {
        return i;
      }
    }

    return -1;
  };

  // Dispatches this event object to all listeners, passing all supplied
  // arguments to this function each listener.
  chromium.Event.prototype.dispatch = function(varargs) {
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
  chromium.Event.prototype.attach_ = function() {
    AttachEvent(this.eventName_);
    if (!this.eventName_)
      return;

    if (chromium.Event.attached_[this.eventName_]) {
      throw new Error("chromium.Event '" + this.eventName_ +
                      "' is already attached.");
    }

    chromium.Event.attached_[this.eventName_] = this;
  };

  // Detaches this event object from its name.
  chromium.Event.prototype.detach_ = function() {
    DetachEvent(this.eventName_);
    if (!this.eventName_)
      return;

    if (!chromium.Event.attached_[this.eventName_]) {
      throw new Error("chromium.Event '" + this.eventName_ +
                      "' is not attached.");
    }

    delete chromium.Event.attached_[this.eventName_];
  };
})();
