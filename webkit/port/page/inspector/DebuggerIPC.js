// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implementation of debugger inter-process communication.
 * Debugger UI can send messages to the DebuggerHost object living in 
 * Chrome browser process. The messages are either processed by DebuggerHost
 * itself or trigger IPC message(such as break, evaluate script etc) from 
 * browser process to the renderer where the v8 instance being debugged
 * will process them.
 */
 
var DebuggerIPC = {};

/**
 * Set up default debugger UI.
 * @param {DebuggerPanel|DebuggerConsole} debuggerUI
 */
DebuggerIPC.init = function(debuggerUI) {
  DebuggerIPC.debuggerUI = debuggerUI;
}

/**
 * Sends JSON message to DebuggerHost.
 * @param {Array.<Object>} nameAndArguments
 */
DebuggerIPC.sendMessage = function(nameAndArguments) {
  //alert("DebuggerIPC.sendMessage " + nameAndArguments);
  dprint("DebuggerIPC.callMethod([" + nameAndArguments + "])");
  // convert all arguments to strings
  // TODO(yurys): extend chrome.send to accept array of any value
  // objects not only strings
  for (var i = 0; i < nameAndArguments.length; i++) {
    if (typeof nameAndArguments[i] != "string") {
      nameAndArguments[i] = "" + nameAndArguments[i];
    }
  }

  chrome.send("DebuggerHostMessage", nameAndArguments);
};

/**
 * Handles messages from DebuggerHost.
 * @param {Object} msg An object representing the message.
 */
DebuggerIPC.onMessageReceived = function(msg) {
  //alert("DebuggerIPC.onMessageReceived " + msg.event);
  var ui = DebuggerIPC.debuggerUI;
  dprint("onMessageReceived: " + (msg && msg.event));
  if (msg.type == "event") {
    if (msg.event == "setDebuggerBreak") {
      var val = msg.body.argument;
      ui.setDebuggerBreak(val);
    } else if (msg.event == "appendText") {
      var text = msg.body.text;
      ui.appendText(text);
    } else if (msg.event == "focusOnCommandLine") {
      dprint("focusOnCommandLine event received");
      // messages to DebugShell
    } else if (msg.event == "initDebugShell") {
      // DebugShell.initDebugShell();
      dprint(msg.event + " done");
    } else if (msg.event == "on_attach") {
      if (DebugShell.singleton) {
        var args = msg.body.arguments;
        if (!args || args.length != 1) {
          dprint(msg.event + " failed: invalid arguments");
          return;
        }
        var title = args[0];
        DebugShell.singleton.on_attach(title);
        dprint(msg.event + " done");
      } else {
        dprint(msg.event + " failed: DebugShell.singleton == null");
      }
    } else if (msg.event == "on_disconnect") {
      if (DebugShell.singleton) {
        DebugShell.singleton.on_disconnect();
        dprint(msg.event + " done");
      } else {
        dprint(msg.event + " failed: DebugShell.singleton == null");
      }
    } else if (msg.event == "exit") {
      if (DebugShell.singleton) {
        DebugShell.singleton.exit();
        dprint(msg.event + " done");
      } else {
        dprint(msg.event + " failed: DebugShell.singleton == null");
      }
    } else if (msg.event == "response") {
      if (DebugShell.singleton) {
        var args = msg.body.arguments;
        if (!args || args.length != 1) {
          dprint(msg.event + " failed: invalid argument");
          return;
        }
        DebugShell.singleton.response(args[0]);
        dprint(msg.event + " done");
      } else {
        ui.appendText(msg.event + " failed: DebugShell.singleton == null");
      }
    } else {
      ui.appendText("Unknown event: " + msg.event);
    }
  } else {
    ui.appendText("Unknown message type: " + msg.type);
  }
};
