var chromium;
(function() {
  if (!chromium)
    chromium = {};

  // callback handling
  var callbacks = [];
  chromium._dispatchCallback = function(callbackId, str) {
    // We shouldn't be receiving evil JSON unless the browser is owned, but just
    // to be safe, we sanitize it. This regex mania was borrowed from json2,
    // from json.org.
    if (!/^[\],:{}\s]*$/.test(
      str.replace(/\\(?:["\\\/bfnrt]|u[0-9a-fA-F]{4})/g, '@').
          replace(/"[^"\\\n\r]*"|true|false|null|-?\d+(?:\.\d*)?(?:[eE][+\-]?\d+)?/g, ']').
          replace(/(?:^|:|,)(?:\s*\[)+/g, '')))
      throw new Error("Unexpected characters in incoming JSON response.");

    // This is lame. V8 disallows direct access to eval() in extensions (see:
    // v8::internal::Parser::ParseLeftHandSideExpression()). So we must use
    // this supa-jank hack instead. We really need native JSON.
    str = 'return ' + str;
    callbacks[callbackId](new Function(str)());
    delete callbacks[callbackId];
  };

  // Quick and dirty json serialization.
  // TODO(aa): Did I mention we need native JSON?
  function serialize(thing) {
    switch (typeof thing) {
      case 'string':
        return '\"' + thing.replace('\\', '\\\\').replace('\"', '\\\"') + '\"';
      case 'boolean':
      case 'number':
        return String(thing);
      case 'object':
        if (thing === null)
          return String(thing)
        var items = [];
        if (thing.constructor == Array) {
          for (var i = 0; i < thing.length; i++)
            items.push(serialize(thing[i]));
          return '[' + items.join(',') + ']';
        } else {
          for (var p in thing)
            items.push(serialize(p) + ':' + serialize(thing[p]));
          return '{' + items.join(',') + '}';
        }
      default:
        return '';
    }
  }

  // Send an API request and optionally register a callback.
  function sendRequest(request, args, callback) {
    var sargs = serialize(args);
    var callbackId = -1;
    if (callback) {
      native function GetNextCallbackId();
      callbackId = GetNextCallbackId();
      callbacks[callbackId] = callback;
    }
    request(sargs, callbackId);
  }

  // Tabs
  chromium.tabs = {};
  chromium.tabs.createTab = function(tab, callback) {
    native function CreateTab();
    sendRequest(CreateTab, tab, callback);
  };
})();
