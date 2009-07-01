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
 * @param {string} json_args JSON-serialized call parameters.
 * @return {string} JSON-serialized result of the dispatched call.
 */
function devtools$$dispatch(functionName, json_args) {
  var params = JSON.parse(json_args);
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
  var args = Array.prototype.slice.call(arguments);

  // Serialize objects here.
  if (method == 'addMessageToConsole') {
    // Skip first argument since it is serializable.
    // Method has index 0, first argument has index 1. Skip both.
    for (var i = 2; i < args.length; ++i) {
      args[i] = devtools$$obj.wrapConsoleObject(args[i]);
    }
  }
  var call = JSON.stringify(args);
  DevToolsAgentHost.dispatch(call);
};
