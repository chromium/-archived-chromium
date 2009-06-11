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
  WebInspector.panels.elements.hide();
  WebInspector.panels.resources.show();

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


var uiTests = new TestSuite();


}
