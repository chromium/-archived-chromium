// Copyright 2008 Google Inc. All Rights Reserved.
// test_protocol.js

/**
 * @fileoverview Unit tests for testing debugger protocol betweeen front-end JS 
 * and back-end. 
 * Run with the following command line:
 * v8_shell_sample.exe --allow-natives-syntax --expose-debug-as debugContext
 *                     chrome/browser/resources/shell.js
 *                     v8/tests/mjsunit.js
 *                     chrome/test/debugger/test_protocol.js
 */


/**
 * shell() is normally a native function exposed to shell.js in Chrome that
 * sets the global DebugShell (shell_) object used for the debugger front-end.
 */
function shell(sh) {
  shell_ = sh;
};


/**
 * @class The global chrome object has some functions to control some status UI
 * in the debugger window.  Stub them out here.
 */
function chrome() {
};
chrome.setDebuggerReady = function(ready) {};
chrome.setDebuggerBreak = function(brk) {};


/**
 * @constructor a pseudo namespace to wrap the various functions and data for
 * the test
 */
function DebuggerTest() {
};

/**
 * initialize the member 
 */
DebuggerTest.initialize = function() {
  DebuggerTest.pendingCommand = null;
  DebuggerTest.output = [];
  
  // swap out the built-in print with our own so that we can verify output
  DebuggerTest.realPrint = print;
  print = DebuggerTest.print;
  // uncomment this to see more verbose information 
  //dprint = DebuggerTest.realPrint;

  debugContext.Debug.addListener(DebuggerTest.listener);
};

/**
 * Collects calls to print() in an array for test verification.
 */ 
DebuggerTest.print = function(str) {
  DebuggerTest.output.push(str);
  // uncomment this if you need to trace what's happening while it's happening
  // rather than waiting for the end
  //DebuggerTest.realPrint(str);
};

/**
 * Processes pendingCommand and sends response to shell_.  Since that may in
 * turn generate a new pendingCommand, repeat this until there is no 
 * pendingCommand.
 */
DebuggerTest.processCommands = function(dcp) {
  while (DebuggerTest.pendingCommand) {
    var json = DebuggerTest.pendingCommand;
    DebuggerTest.pendingCommand = null;
    var result = dcp.processDebugJSONRequest(json);
    shell_.response(result);
  }
};

/**
 * Handles DebugEvents from the Debug object.  Hooked in via addListener above.
 */
DebuggerTest.listener = function(event, exec_state, event_data, data) {
  try {
    if (event == debugContext.Debug.DebugEvent.Break) {
      var dcp = exec_state.debugCommandProcessor();
      // process any pending commands prior to handling the breakpoint
      DebuggerTest.processCommands(dcp);
      var json = event_data.toJSONProtocol();
      shell_.response(json);
      // response() may have added another command to process
      DebuggerTest.processCommands(dcp);
    } 
  } catch(e) {
    print(e);
  }
};

/**
 * Send the next command from the command-list.
 */
DebuggerTest.sendNextCommand = function() {
  var cmd = DebuggerTest.commandList.shift();
  print("$ " + cmd);
  shell_.command(cmd);
};

/**
 * Verify that the actual output matches the expected output
 * depends on mjsunit
 */
DebuggerTest.verifyOutput = function() {
  // restore print since mjsunit depends on it
  print = DebuggerTest.realPrint;

  var out = DebuggerTest.output;
  var expected = DebuggerTest.expectedOutput;
  if (out.length != expected.length) {
    assertTrue(out.length == expected.length,
               "length mismatch: " + out.length + " == " + expected.length);
  } else {
    var succeeded = true;
    for (var i in out) {
      // match the front of the string so we can avoid testing changes in frames
      // that are in the test harness
      if (out[i].indexOf(expected[i]) != 0) {
        assertTrue(out[i] == expected[i],
                   "actual '" + out[i] + "' == " + "expected '" + expected[i] + "'");
        succeeded = false;
        break;
      }
    }
    if (succeeded)
      print("Success");
  }
  
  // useful for generating a new version of DebuggerTest.expectedOutput
  for (var i in DebuggerTest.output) {
    //print("  \"" + DebuggerTest.output[i] + "\",");
  }
};



/**
 * @class DebugShell is passed a "Tab" object that it uses to communicate with 
 * the debugger.  This mock simulates that.
 * @param {string} title
 */
DebuggerTest.TabMock = function(title) {
  this.title = title;
  this.attach = function() {};
  this.debugBreak = function() {
    // TODO(erikkay)
  };
  this.sendToDebugger = function(str) {
    DebuggerTest.pendingCommand = str;
  };
};


/**
 * @class Uses prototype chaining to allow DebugShell methods to be overridden
 * selectively.
 */
function DebugShellOverrides() {
  this._origPrototype = DebugShell.prototype;
};
DebugShellOverrides.prototype = DebugShell.prototype;
DebugShell.prototype = new DebugShellOverrides;

/**
 * Overrides DebugShell.prototype.response so that we can log the responses
 * and trigger the next command to be processed.
 */
DebugShell.prototype.response = function(str) {
  var msg = eval('(' + str + ')');
  print("< " + msg.type + ":" + (msg.command || msg.event));
  var sendAnother = (msg.type == "event");
  if (!sendAnother && shell_.current_command) {
    sendAnother = (shell_.current_command.from_user &&
        msg.type == "response" && msg.command != "continue")
  }
  this._origPrototype.response.call(this, str)

  // Send the next command, but only if the program is paused (a continue
  // command response means that we're about to be running again) and the
  // command we just processed isn't a continue
  if (!this.running && sendAnother)
    DebuggerTest.sendNextCommand();
};


// The list of commands to be processed
// TODO(erikkay): this doesn't test the full set of debugger commands yet,
// but it should be enough to verify that the protocol is still working.
DebuggerTest.commandList = [
"next", "step", "backtrace", "source", "print x", "args", "locals", "frame 1",
"stepout", "continue"
];

DebuggerTest.expectedOutput = [
  "< event:break",
  "g(), foo.html",
  "60:   debugger;",
  "$ next",
  "< response:continue",
  "< event:break",
  "61:   f(1);",
  "$ step",
  "< response:continue",
  "< event:break",
  "f(x=1, y=undefined), foo.html",
  "29:   return a;",
  "$ backtrace",
  "< response:backtrace",
  "Frames #0 to #3 of 4:",
  "#00 f(x=1, y=undefined) foo.html line 29 column 3 (position 62)",
  "#01 g() foo.html line 61 column 3 (position 30)",
  "#02 function DebuggerTest()", // prefix
  "#03 [anonymous]()", // prefix
  "$ source",
  "< response:source",
  "24: function f(x, y) {",
  "25:   var a=1;",
  "26:   if (x == 1) {",
  "27:     a++;",
  "28:   }",
  ">>>>  return a;",
  "30: };",
  "$ print x",
  "< response:evaluate",
  "1",
  "$ args",
  "< response:frame",
  "x = 1",
  "y = undefined",
  "$ locals",
  "< response:frame",
  "a = 2",
  "$ frame 1",
  "< response:frame",
  // Temporarily allow undefined as the script name.
  // "#01 g, foo.html",
  "#01 g, undefined",  
  "61:   f(1);",
  "$ stepout",
  "< response:continue",
  "< event:break",
  "g(), foo.html",
  "62: };",
  "$ continue",
  "< response:continue"
];


/**
 * Tests the debugger and protocol for a couple of simple scripts.
 */
DebuggerTest.testProtocol = function() {
  DebuggerTest.initialize();

  // Startup the front-end.
  debug(new DebuggerTest.TabMock("testing"));
  
  script1 = 
      "function f(x, y) {\n" + // line 23
      "  var a=1;\n" + // line 24
      "  if (x == 1) {\n" + // line 25
      "    a++;\n  }\n" + // line 26
      "  return a;\n" + // line 27
      "};"; // line 28
  script2 = 
      "function g() {\n" + // line 58
      "  debugger;\n" + // line 59
      "  f(1);\n" + // line 60
      "};"; // line 61
  %CompileScript(script1, "foo.html", 23, 0)();
  %CompileScript(script2, "foo.html", 58, 0)();
  
  try {
    g();
  } catch(e) {
    print(e);
  }

  DebuggerTest.verifyOutput();
}

DebuggerTest.testProtocol();
