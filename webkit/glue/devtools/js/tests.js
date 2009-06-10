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
}


/**
 * Equals assertion tests that expected == actual.
 * @param {Object} expected Expected object.
 * @param {Object} actual Actual object.
 */
TestSuite.prototype.assertEquals = function(expected, actual) {
  if (expected != actual) {
    this.fail('Expected: "' + expected + '", but was "' + actual + '"');
  }
}


/**
 * True assertion tests that value == true.
 * @param {Object} expected Expected object.
 * @param {Object} value Actual object.
 */
TestSuite.prototype.assertTrue = function(value) {
  this.assertEquals(true, value);
}


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
}


/**
 * Runs all global functions starting with 'test' as unit tests.
 */
TestSuite.prototype.runTest = function(testName) {
  try {
    this[testName]();
    window.domAutomationController.send('[OK]');
  } catch (e) {
    window.domAutomationController.send('[FAILED] ' + testName + ': ' + e);
  }
}


// UI Tests


/**
 * Tests that the real injected host is present in the context.
 */
TestSuite.prototype.testHostIsPresent = function() {
  var domAgent = devtools.tools.getDomAgent();
  var doc = domAgent.getDocument();
  this.assertTrue(typeof DevToolsHost == 'object' && !DevToolsHost.isStub);
  this.assertTrue(!!doc.documentElement);
}


/**
 * Tests elements tree has an 'HTML' root.
 */
TestSuite.prototype.testElementsTreeRoot = function() {
  var domAgent = devtools.tools.getDomAgent();
  var doc = domAgent.getDocument();
  this.assertEquals('HTML', doc.documentElement.nodeName); 
  this.assertTrue(doc.documentElement.hasChildNodes());
}


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
}


var uiTests = new TestSuite();


}
