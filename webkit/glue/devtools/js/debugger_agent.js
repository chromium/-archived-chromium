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
      
  /**
   * Mapping from script id to script info.
   * @type {Object}
   */
  this.parsedScripts_ = {};
  
  /**
   * Mapping from the request id to the devtools.BreakpointInfo for the
   * breakpoints whose v8 ids are not set yet. These breakpoints are waiting for
   * 'setbreakpoint' responses to learn their ids in the v8 debugger.
   * @see #handleSetBreakpointResponse_
   * @type {!Object}
   */
  this.requestNumberToBreakpointInfo_ = {};
  
  /**
   * Information on current stack top frame.
   * See JavaScriptCallFrame.idl.
   * @type {?Object}
   */
  this.currentCallFrame_ = null;
};


/**
 * Asynchronously requests for all parsed script sources. Response will be
 * processed in handleScriptsResponse_.
 */
devtools.DebuggerAgent.prototype.requestScripts = function() {
  var cmd = new devtools.DebugCommand('scripts', {
    'includeSource': true
  });
  devtools.DebuggerAgent.sendCommand_(cmd);
  // Force v8 execution so that it gets to processing the requested command.
  devtools.tools.evaluateJavaScript("javascript:void(0)");
};


/**
 * Tells the v8 debugger to stop on as soon as possible.
 */
devtools.DebuggerAgent.prototype.pauseExecution = function() {
  RemoteDebuggerAgent.DebugBreak();
};


/**
 * @param {number} sourceId Id of the script fot the breakpoint.
 * @param {number} line Number of the line for the breakpoint.
 */
devtools.DebuggerAgent.prototype.addBreakpoint = function(sourceId, line) {
  var script = this.parsedScripts_[sourceId];
  if (!script) {
    return;
  }
  
  var breakpointInfo = script.getBreakpointInfo(line);
  if (breakpointInfo) {
    return;
  }
  
  breakpointInfo = new devtools.BreakpointInfo(sourceId, line);
  script.addBreakpointInfo(breakpointInfo);

  line += script.getLineOffset() - 1;
  var cmd = new devtools.DebugCommand('setbreakpoint', {
    'type': 'scriptId',
    'target': sourceId,
    'line': line
  });
  
  this.requestNumberToBreakpointInfo_[cmd.getSequenceNumber()] = breakpointInfo;
  
  devtools.DebuggerAgent.sendCommand_(cmd);
};


/**
 * @param {number} sourceId Id of the script fot the breakpoint.
 * @param {number} line Number of the line for the breakpoint.
 */
devtools.DebuggerAgent.prototype.removeBreakpoint = function(sourceId, line) {
  var script = this.parsedScripts_[sourceId];
  if (!script) {
    return;
  }
  
  var breakpointInfo = script.getBreakpointInfo(line);
  script.removeBreakpointInfo(breakpointInfo);
  breakpointInfo.markAsRemoved();

  var id = breakpointInfo.getV8Id();

  // If we don't know id of this breakpoint in the v8 debugger we cannot send
  // 'clearbreakpoint' request. In that case it will be removed in
  // 'setbreakpoint' response handler when we learn the id.
  if (id != -1) {
    this.requestClearBreakpoint_(id);
  }
};


/**
 * Tells the v8 debugger to step into the next statement.
 */
devtools.DebuggerAgent.prototype.stepIntoStatement = function() {
  this.stepCommand_('in');
};


/**
 * Tells the v8 debugger to step out of current function.
 */
devtools.DebuggerAgent.prototype.stepOutOfFunction = function() {
  this.stepCommand_('out');
};


/**
 * Tells the v8 debugger to step over the next statement.
 */
devtools.DebuggerAgent.prototype.stepOverStatement = function() {
  this.stepCommand_('next');
};


/**
 * Tells the v8 debugger to continue execution after it has been stopped on a
 * breakpoint or an exception.
 */
devtools.DebuggerAgent.prototype.resumeExecution = function() {
  var cmd = new devtools.DebugCommand('continue');
  devtools.DebuggerAgent.sendCommand_(cmd);
};


/**
 * Current stack top frame.
 * @return {Object}
 */
devtools.DebuggerAgent.prototype.getCurrentCallFrame = function() {
  return this.currentCallFrame_;
};


/**
 * Removes specified breakpoint from the v8 debugger.
 * @param {number} breakpointId Id of the breakpoint in the v8 debugger.
 */
devtools.DebuggerAgent.prototype.requestClearBreakpoint_ = function(
    breakpointId) {
  var cmd = new devtools.DebugCommand('clearbreakpoint', {
    'breakpoint': breakpointId
  });
  devtools.DebuggerAgent.sendCommand_(cmd);
};


/**
 * Sends 'backtrace' request to v8.
 */
devtools.DebuggerAgent.prototype.requestBacktrace_ = function() {
  var cmd = new devtools.DebugCommand('backtrace');
  devtools.DebuggerAgent.sendCommand_(cmd);
};


/**
 * Sends command to v8 debugger.
 * @param {devtools.DebugCommand} cmd Command to execute.
 */
devtools.DebuggerAgent.sendCommand_ = function(cmd) {
  RemoteDebuggerCommandExecutor.DebuggerCommand(cmd.toJSONProtocol());
};


/**
 * Tells the v8 debugger to make the next execution step.
 * @param {string} action 'in', 'out' or 'next' action.
 */
devtools.DebuggerAgent.prototype.stepCommand_ = function(action) {
  var cmd = new devtools.DebugCommand('continue', {
    'stepaction': action,
    'stepcount': 1
  });
  devtools.DebuggerAgent.sendCommand_(cmd);
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
  
  if (msg.getType() == 'event') {
    if (msg.getEvent() == 'break') {
      this.handleBreakEvent_(msg);
    } else if (msg.getEvent() == 'exception') {
      this.handleExceptionEvent_(msg);
    }
  } else if (msg.getType() == 'response') {
    if (msg.getCommand() == 'scripts') {
      this.handleScriptsResponse_(msg);
    } else if (msg.getCommand() == 'setbreakpoint') {
      this.handleSetBreakpointResponse_(msg);
    } else if (msg.getCommand() == 'clearbreakpoint') {
      this.handleClearBreakpointResponse_(msg);
    } else if (msg.getCommand() == 'backtrace') {
      this.handleBacktraceResponse_(msg);
    }
  }
};


/**
 * @param {devtools.DebuggerMessage} msg
 */
devtools.DebuggerAgent.prototype.handleBreakEvent_ = function(msg) {
  var body = msg.getBody();
  
  this.currentCallFrame_ = {
    'sourceID': body.script.id,
    'line': body.sourceLine - body.script.lineOffset +1,
    'script': body.script,
    'scopeChain': [],
    'thisObject': {}
  };
  
  this.requestBacktrace_();
};


/**
 * @param {devtools.DebuggerMessage} msg
 */
devtools.DebuggerAgent.prototype.handleExceptionEvent_ = function(msg) {
  var body = msg.getBody();
  debugPrint('Uncaught exception in ' + body.script.name + ':' +
             body.sourceLine + '\n' + body.sourceLineText);
  this.resumeExecution();
};


/**
 * @param {devtools.DebuggerMessage} msg
 */
devtools.DebuggerAgent.prototype.handleScriptsResponse_ = function(msg) {
  var scripts = msg.getBody();
  for (var i = 0; i < scripts.length; i++) {
    var script = scripts[i];
    
    this.parsedScripts_[script.id] = new devtools.ScriptInfo(
        script.id, script.lineOffset);
    
    WebInspector.parsedScriptSource(
        script.id, script.name, script.source, script.lineOffset);
  }
};


/**
 * @param {devtools.DebuggerMessage} msg
 */
devtools.DebuggerAgent.prototype.handleSetBreakpointResponse_ = function(msg) {
  var requestSeq = msg.getRequestSeq();
  var breakpointInfo = this.requestNumberToBreakpointInfo_[requestSeq]; 
  if (!breakpointInfo) {
    // TODO(yurys): handle this case
    return;
  }
  delete this.requestNumberToBreakpointInfo_[requestSeq];
  if (!msg.isSuccess()) {
    // TODO(yurys): handle this case
    return;
  }
  var idInV8 = msg.getBody().breakpoint;
  breakpointInfo.setV8Id(idInV8);
  
  if (breakpointInfo.isRemoved()) {
    this.requestClearBreakpoint_(idInV8);
  }
};


/**
 * @param {devtools.DebuggerMessage} msg
 */
devtools.DebuggerAgent.prototype.handleClearBreakpointResponse_ = function(
    msg) {
  // Do nothing.
};


/**
 * Handles response to 'backtrace' command.
 * @param {devtools.DebuggerMessage} msg
 */
devtools.DebuggerAgent.prototype.handleBacktraceResponse_ = function(msg) {
  if (!this.currentCallFrame_) {
    return;
  }
  
  var script = this.currentCallFrame_.script;
  
  var caller = null;
  var f = null;
  var frames = msg.getBody().frames;
  for (var i = frames.length - 1; i>=0; i--) {
    var nextFrame = frames[i];
    var func = msg.lookup(nextFrame.func.ref);
    
    // Format arguments.
    var argv = [];
    for (var j = 0; j < nextFrame.arguments.length; j++) {
      var arg = nextFrame.arguments[j];
      var val = msg.lookup(arg.value.ref);
      if (val) {
        if (val.value) {
          argv.push(arg.name + " = " + val.value);
        } else {
          argv.push(arg.name + " = [" + val.type + "]");
        }

      } else {
        argv.push(arg.name + " = {ref:" + arg.value.ref + "} ");
      }
    }

    var funcName = func.name + "(" + argv.join(", ") + ")";

    var f = {
      'sourceID': script.id,
      'line': nextFrame.line - script.lineOffset +1,
      'type': 'function',
      'functionName': funcName, //nextFrame.text,
      'caller': caller,
      'scopeChain': [],
      'thisObject': {}
    };
    caller = f;
  }
  
  this.currentCallFrame_ = f;
  
  WebInspector.pausedScript();
};


/**
 * @param {number} scriptId Id of the script.
 * @param {number} lineOffset First line 0-based offset in the containing
 *     document.
 * @constructor
 */
devtools.ScriptInfo = function(scriptId, lineOffset) {
  this.scriptId_ = scriptId;
  this.lineOffset_ = lineOffset;
  
  this.lineToBreakpointInfo_ = {};
};


/**
 * @return {number}
 */
devtools.ScriptInfo.prototype.getLineOffset = function() {
  return this.lineOffset_;
};


/**
 * @param {number} line 0-based line number in the script.
 * @return {?devtools.BreakpointInfo} Information on a breakpoint at the
 *     specified line in the script or undefined if there is no breakpoint at
 *     that line.
 */
devtools.ScriptInfo.prototype.getBreakpointInfo = function(line) {
  return this.lineToBreakpointInfo_[line];
};


/**
 * Adds breakpoint info to the script.
 * @param {devtools.BreakpointInfo} breakpoint
 */
devtools.ScriptInfo.prototype.addBreakpointInfo = function(breakpoint) {
  this.lineToBreakpointInfo_[breakpoint.getLine()] = breakpoint;
};


/**
 * @param {devtools.BreakpointInfo} breakpoint Breakpoint info to be removed.
 */
devtools.ScriptInfo.prototype.removeBreakpointInfo = function(breakpoint) {
  var line = breakpoint.getLine();
  delete this.lineToBreakpointInfo_[line];
};



/**
 * @param {number} scriptId Id of the owning script.
 * @param {number} line Breakpoint 0-based line number in the containing script.
 * @constructor
 */
devtools.BreakpointInfo = function(sourceId, line) {
  this.sourceId_ = sourceId;
  this.line_ = line; 
  this.v8id_ = -1;
  this.removed_ = false;
};


/**
 * @return {number}
 */
devtools.BreakpointInfo.prototype.getSourceId = function(n) {
  return this.sourceId_;
};


/**
 * @return {number}
 */
devtools.BreakpointInfo.prototype.getLine = function(n) {
  return this.line_;
};


/**
 * @return {number} Unique identifier of this breakpoint in the v8 debugger.
 */
devtools.BreakpointInfo.prototype.getV8Id = function(n) {
  return this.v8id_;
};


/**
 * Sets id of this breakpoint in the v8 debugger.
 * @param {number} id
 */
devtools.BreakpointInfo.prototype.setV8Id = function(id) {
  this.v8id_ = id;
};


/**
 * Marks this breakpoint as removed from the  front-end.
 */
devtools.BreakpointInfo.prototype.markAsRemoved = function() {
  this.removed_ = true;
};


/**
 * @return {boolean} Whether this breakpoint has been removed from the
 *     front-end.
 */
devtools.BreakpointInfo.prototype.isRemoved = function() {
  return this.removed_;
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
