// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Injects 'injected' object into the inspectable page.
 */

/**
 * Dispatches host calls into the injected function calls.
 */
goog.require('devtools.Injected');


/**
 * Injected singleton.
 */
var devtools$$obj = new devtools.Injected();


/**
 * Main dispatch method, all calls from the host go through this one.
 * @param {string} functionName Function to call
 * @param {Node} node Node context of the call.
 * @param {string} json_args JSON-serialized call parameters.
 * @return {string} JSON-serialized result of the dispatched call.
 */
function devtools$$dispatch(functionName, node, json_args) {
  var params = JSON.parse(json_args);
  params.splice(0, 0, node);
  var result = devtools$$obj[functionName].apply(devtools$$obj, params);
  return JSON.stringify(result);
};


/**
 * This is called by the InspectorFrontend for serialization.
 * We serialize the call and send it to the client over the IPC
 * using dispatchOut bound method.
 */
var dispatch = function(method, var_args) {
  // Handle all messages with non-primitieve arguments here.
  if (method == 'inspectedWindowCleared') {
    return;
  }
  var args = Array.prototype.slice.call(arguments);

  // Serialize objects here.
  if (method == 'addMessageToConsole') {
    // Skip first argument since it is serializable.
    // Method has index 0, first argument has index 1. Skip both.
    for (var i = 2; i < args.length; ++i) {
      var type = typeof args[i];
      if (type == 'object') {
        args[i] = Object.prototype.toString(args[i]);
      } else if (type == 'function') {
        args[i] = args[i].toString();
      }
    }
  }
  var call = JSON.stringify(args);
  RemoteWebInspector.dispatch(call);
};
