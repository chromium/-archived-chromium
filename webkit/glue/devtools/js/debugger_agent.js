// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides communication interface to remote v8 debugger. See
 * protocol decription at http://code.google.com/p/v8/wiki/DebuggerProtocol
 */
goog.provide('devtools.DebuggerAgent');


/**
 * @constructor
 */
devtools.DebuggerAgent = function() {
  RemoteDebuggerAgent.DebuggerOutput =
      goog.bind(this.handleDebuggerOutput_, this);
};


/**
 * Asynchronously requests for all parsed script sources. Response will be
 * processed in handleScriptsResponse_.
 */
devtools.DebuggerAgent.prototype.requestScripts = function() {
  var cmd = new devtools.DebugCommand('scripts', {
    'includeSource': true
  });
  RemoteDebuggerCommandExecutor.DebuggerCommand(cmd.toJSONProtocol());
  // Force v8 execution so that it gets to processing the requested command.
  devtools.tools.evaluateJavaSctipt('javascript:void(0)');
};


/**
 * Handles output sent by v8 debugger. The output is either asynchronous event
 * or response to a previously sent request.  See protocol definitioun for more
 * details on the output format.
 * @param {string} output
 */
devtools.DebuggerAgent.prototype.handleDebuggerOutput_ = function(output) {
  var msg;
  try {
    msg = new devtools.DebuggerMessage(output);
  } catch(e) {
    debugPrint('Failed to handle debugger reponse:\n' + e);
    throw e;
  }
  
  if (msg.getType() == 'response') {
    if (msg.getCommand() == 'scripts') {
      this.handleScriptsResponse_(msg);
    }
  }
};


/**
 * @param {devtools.DebuggerMessage} msg
 */
devtools.DebuggerAgent.prototype.handleScriptsResponse_ = function(msg) {
  var scripts = msg.getBody();
  for (var i = 0; i < scripts.length; i++) {
    var script = scripts[i];
    WebInspector.parsedScriptSource(
        script.id, script.name, script.source, script.lineOffset);
  }
};


/**
 * JSON based commands sent to v8 debugger.
 * @param {string} command Name of the command to execute.
 * @param {Object} opt_arguments Command-specific arguments map.
 * @constructor
 */
devtools.DebugCommand = function(command, opt_arguments) {
  this.command_ = command;
  this.type_ = 'request';	
  this.seq_ = ++devtools.DebugCommand.nextSeq_;
  if (opt_arguments) {
    this.arguments_ = opt_arguments;
  }
};


/**
 * Next unique number to be used as debugger request sequence number.
 * @type {number}
 */
devtools.DebugCommand.nextSeq_ = 1;


/**
 * @return {number}
 */
devtools.DebugCommand.prototype.getSequenceNumber = function() {
  return this.seq_;
};


/**
 * @return {string}
 */
devtools.DebugCommand.prototype.toJSONProtocol = function() {
  var json = {
    'seq': this.seq_,
    'type': this.type_,
    'command': this.command_
  }
  if (this.arguments_) {
    json.arguments = this.arguments_;
  }
  return goog.json.serialize(json);
};



/**
 * JSON messages sent from v8 debugger. See protocol definition for more
 * details: http://code.google.com/p/v8/wiki/DebuggerProtocol
 * @param {string} msg Raw protocol packet as JSON string.
 * @constructor
 */
devtools.DebuggerMessage = function(msg) {
  var jsExpression = '[' + msg + '][0]';
  this.packet_ = eval(jsExpression);
  this.refs_ = [];
  if (this.packet_.refs) {
    for (var i = 0; i < this.packet_.refs.length; i++) {
      this.refs_[this.packet_.refs[i].handle] = this.packet_.refs[i];
    }
  }
};


/**
 * @return {string} The packet type.
 */
devtools.DebuggerMessage.prototype.getType = function() {
  return this.packet_.type;
};


/**
 * @return {?string} The packet event if the message is an event.
 */
devtools.DebuggerMessage.prototype.getEvent = function() {
  return this.packet_.event;
};


/**
 * @return {?string} The packet command if the message is a response to a
 *     command.
 */
devtools.DebuggerMessage.prototype.getCommand = function() {
  return this.packet_.command;
};


/**
 * @return {number} The packet request sequence.
 */
devtools.DebuggerMessage.prototype.getRequestSeq = function() {
  return this.packet_.request_seq;
};


/**
 * @return {number} Whether the v8 is running after processing the request.
 */
devtools.DebuggerMessage.prototype.isRunning = function() {
  return this.packet_.running ? true : false;
};


/**
 * @return {boolean} Whether the request succeeded.
 */
devtools.DebuggerMessage.prototype.isSuccess = function() {
  return this.packet_.success ? true : false;
};


/**
 * @return {string}
 */
devtools.DebuggerMessage.prototype.getMessage = function() {
  return this.packet_.message;
};


/**
 * @return {Object} Parsed message body json.
 */
devtools.DebuggerMessage.prototype.getBody = function() {
  return this.packet_.body;
};


/**
 * @param {number} handle Object handle.
 * @return {?Object} Returns the object with the handle if it was sent in this
 *    message(some objects referenced by handles may be missing in the message).
 */
devtools.DebuggerMessage.prototype.lookup = function(handle) {
  return this.refs_[handle];
};
