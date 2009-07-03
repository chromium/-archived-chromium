// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview This file contains small testing framework along with the
 * test suite for the frontend. These tests are a part of the continues build
 * and are executed by the devtools_sanity_unittest.cc as a part of the
 * Interactive UI Test suite.
 */

if (window.domAutomationController) {


/**
 * Test suite for interactive UI tests.
 * @constructor
 */
TestSuite = function() {
};


/**
 * Reports test failure.
 * @param {string} message Failure description.
 */
TestSuite.prototype.fail = function(message) {
  throw message;
};


/**
 * Equals assertion tests that expected == actual.
 * @param {Object} expected Expected object.
 * @param {Object} actual Actual object.
 */
TestSuite.prototype.assertEquals = function(expected, actual) {
  if (expected != actual) {
    this.fail('Expected: "' + expected + '", but was "' + actual + '"');
  }
};


/**
 * True assertion tests that value == true.
 * @param {Object} expected Expected object.
 * @param {Object} value Actual object.
 */
TestSuite.prototype.assertTrue = function(value) {
  this.assertEquals(true, value);
};


/**
 * Contains assertion tests that string contains substring.
 * @param {string} string Outer.
 * @param {string} substring Inner.
 */
TestSuite.prototype.assertContains = function(string, substring) {
  if (string.indexOf(substring) == -1) {
    this.fail('Expected to: "' + string + '" to contain "' + substring + '"');
  }
};


/**
 * Runs all global functions starting with 'test' as unit tests.
 */
TestSuite.prototype.runAllTests = function() {
  // For debugging purposes.
  for (var name in this) {
    if (name.substring(0, 4) == 'test' &&
        typeof this[name] == 'function') {
      this.runTest(name);
    }
  }
};


/**
 * Async tests use this one to report that they are completed.
 */
TestSuite.prototype.reportOk = function() {
  window.domAutomationController.send('[OK]');
};


/**
 * Manual controller for async tests.
 * @constructor.
 */
TestSuite.Controller = function() {
  this.controlTaken_ = false;
  this.timerId_ = -1;
};


/**
 * Takes control over execution.
 */
TestSuite.Controller.prototype.takeControl = function() {
  this.controlTaken_ = true;
  // Set up guard timer.
  var self = this;
  this.timerId_ = setTimeout(function() {
    self.reportFailure('Timeout exceeded: 20 sec');
  }, 20000);
};


/**
 * Async tests use this one to report that they are completed.
 */
TestSuite.Controller.prototype.reportOk = function() {
  if (this.timerId_ != -1) {
    this.timerId_ = -1;
    clearTimeout(this.timerId_);
  }
  window.domAutomationController.send('[OK]');
};


/**
 * Async tests use this one to report failures.
 */
TestSuite.Controller.prototype.reportFailure = function(error) {
  if (this.timerId_ != -1) {
    this.timerId_ = -1;
    clearTimeout(this.timerId_);
  }
  window.domAutomationController.send('[FAILED] ' + error);
};


/**
 * Runs all global functions starting with 'test' as unit tests.
 */
TestSuite.prototype.runTest = function(testName) {
  var controller = new TestSuite.Controller();
  try {
    this[testName](controller);
    if (!controller.controlTaken_) {
      controller.reportOk();
    }
  } catch (e) {
    controller.reportFailure(e);
  }
};


/**
 * @param {string} panelName Name of the panel to show.
 */
TestSuite.prototype.showPanel = function(panelName) {
  // Open Scripts panel.
  var toolbar = document.getElementById('toolbar');
  var button = toolbar.getElementsByClassName(panelName)[0];
  button.click();
  this.assertEquals(WebInspector.panels[panelName],
      WebInspector.currentPanel);
};

// UI Tests


/**
 * Tests that the real injected host is present in the context.
 */
TestSuite.prototype.testHostIsPresent = function() {
  var domAgent = devtools.tools.getDomAgent();
  var doc = domAgent.getDocument();
  this.assertTrue(typeof DevToolsHost == 'object' && !DevToolsHost.isStub);
  this.assertTrue(!!doc.documentElement);
};


/**
 * Tests elements tree has an 'HTML' root.
 */
TestSuite.prototype.testElementsTreeRoot = function() {
  var domAgent = devtools.tools.getDomAgent();
  var doc = domAgent.getDocument();
  this.assertEquals('HTML', doc.documentElement.nodeName);
  this.assertTrue(doc.documentElement.hasChildNodes());
};


/**
 * Tests that main resource is present in the system and that it is
 * the only resource.
 */
TestSuite.prototype.testMainResource = function() {
  var tokens = [];
  var resources = WebInspector.resources;
  for (var id in resources) {
    tokens.push(resources[id].lastPathComponent);
  }
  this.assertEquals('simple_page.html', tokens.join(','));
};


/**
 * Tests that resources tab is enabled when corresponding item is selected.
 */
TestSuite.prototype.testEnableResourcesTab = function(controller) {
  this.showPanel('resources');

  var test = this;
  var oldAddResource = WebInspector.addResource;
  WebInspector.addResource = function(identifier, payload) {
    WebInspector.addResource = oldAddResource;
    oldAddResource.call(this, identifier, payload);
    test.assertEquals('simple_page.html', payload.lastPathComponent);
    WebInspector.panels.resources.refresh();
    WebInspector.resources[identifier]._resourcesTreeElement.select();

    controller.reportOk();
  };

  // Following call should lead to reload that we capture in the
  // addResource override.
  WebInspector.panels.resources._enableResourceTracking();

  // We now have some time to report results to controller.
  controller.takeControl();
};


/**
 * Test that profiler works.
 */
TestSuite.prototype.testProfilerTab = function(controller) {
  this.showPanel('profiles');

  var oldAddProfile = WebInspector.addProfile;
  WebInspector.addProfile = function(profile) {
    WebInspector.addProfile = oldAddProfile;
    oldAddProfile.call(this, profile);

    var panel = WebInspector.panels.profiles;
    panel.showProfile(profile);
    var node = panel.visibleView.profileDataGridTree.children[0];
    // Iterate over displayed functions and search for a function
    // that is called 'fib' or 'eternal_fib'. If found, it will mean
    // that we actually have profiled page's code.
    while (node) {
      if (node.functionName.indexOf("fib") != -1) {
        controller.reportOk();
      }
      node = node.traverseNextNode(true, null, true);
    }

    controller.reportFailure();
  };

  InspectorController.startProfiling();
  window.setTimeout('InspectorController.stopProfiling();', 1000);
  controller.takeControl();
};


/**
 * Tests that scripts tab can be open and populated with inspected scripts.
 */
TestSuite.prototype.testShowScriptsTab = function(controller) {
  var parsedDebuggerTestPageHtml = false;
  var parsedDebuggerTestJs = false;

  // Intercept parsedScriptSource calls to check that all expected scripts are
  // added to the debugger.
  var self = this;
  var originalParsedScriptSource = WebInspector.parsedScriptSource;
  WebInspector.parsedScriptSource = function(sourceID, sourceURL, source,
      startingLine) {
    if (sourceURL.search(/debugger_test_page.html$/) != -1) {
      if (parsedDebuggerTestPageHtml) {
        controller.reportFailure('Unexpected parse event: ' + sourceURL);
        return;
      }
      parsedDebuggerTestPageHtml = true;
    } else if (sourceURL.search(/debugger_test.js$/) != -1) {
      if (parsedDebuggerTestJs) {
        controller.reportFailure('Unexpected parse event: ' + sourceURL);
        return;
      }
      parsedDebuggerTestJs = true;
    } else {
      controller.reportFailure('Unexpected script URL: ' + sourceURL);
    }
    originalParsedScriptSource.apply(this, arguments);

    if (!WebInspector.panels.scripts.visibleView) {
      controller.reportFailure('No visible script view: ' + sourceURL);
      return;
    }

    if (parsedDebuggerTestJs && parsedDebuggerTestPageHtml) {
       controller.reportOk();
    }
    controller.reportOk();
  };

  this.showPanel('scripts');

  // Wait until all scripts are added to the debugger.
  controller.takeControl();
};


TestSuite.ConsoleEnter = {
    keyIdentifier : 'Enter',
    preventDefault : function() {},
    stopPropagation : function() {}
};


/**
 * Tests console eval.
 */
TestSuite.prototype.testConsoleEval = function(controller) {
  var test = this;
  var originalConsoleAddMessage = WebInspector.Console.prototype.addMessage;
  WebInspector.Console.prototype.addMessage = function(commandResult) {
    originalConsoleAddMessage.call(this, commandResult);
    WebInspector.Console.prototype.addMessage = originalConsoleAddMessage;
    test.assertEquals('123', commandResult.toMessageElement().textContent);
    controller.reportOk();
  };

  WebInspector.console.visible = true;
  WebInspector.console.prompt.text = '123';
  WebInspector.console.promptElement.handleKeyEvent(TestSuite.ConsoleEnter);

  controller.takeControl();
};


/**
 * Tests console log.
 */
TestSuite.prototype.testConsoleLog = function(controller) {
  WebInspector.console.visible = true;
  var messages = WebInspector.console.messages;
  var index = 0;

  var test = this;
  var assertNext = function(line, message, opt_level, opt_count, opt_substr) {
    var elem = messages[index++].toMessageElement();
    var clazz = elem.getAttribute('class');
    var expectation = (opt_count || '') + 'console_test_page.html:' +
        line + message;
    if (opt_substr) {
      test.assertContains(elem.textContent, expectation);
    } else {
      test.assertEquals(expectation, elem.textContent);
    }
    if (opt_level) {
      test.assertContains(clazz, 'console-' + opt_level + '-level');
    }
  };

  assertNext('5', 'log', 'log');
  assertNext('7', 'debug', 'log');
  assertNext('9', 'info', 'log');
  assertNext('11', 'warn', 'warning');
  assertNext('13', 'error', 'error');
  assertNext('15', 'Message format number 1, 2 and 3.5');
  assertNext('17', 'Message format for string');
  assertNext('19', 'Object Object');
  assertNext('22', 'repeated', 'log', 5);
  assertNext('26', 'count: 1');
  assertNext('26', 'count: 2');
  assertNext('29', 'group', 'group-title');
  index++;
  assertNext('33', 'timer:', 'log', '', true);
};


var uiTests = new TestSuite();
}
