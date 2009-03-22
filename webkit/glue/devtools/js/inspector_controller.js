// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Stub implementation of the InspectorController API.
 * This stub class is supposed to make front-end a standalone WebApp
 * that can be implemented/refactored in isolation from the Web browser
 * backend. Clients need to subclass it in order to wire calls to the
 * non-stub backends.
 */
goog.provide('devtools.InspectorController');


/**
 * Creates inspector controller stub instance.
 * @constructor.
 */
devtools.InspectorController = function() {  
  /**
   * @type {boolean}
   */
  this.searchingForNode_ = false;
  
  /**
   * @type {boolean}
   */
  this.windowVisible_ = true;

  /**
   * @type {number}
   */
  this.attachedWindowHeight_ = 0;

  /**
   * @type {boolean}
   */
  this.debuggerEnabled_ = true;

  /**
   * @type {boolean}
   */
  this.profilerEnabled_ = true;
};


/**
 * Wraps javascript callback.
 * @param {function():undefined} func The callback to wrap.
 * @return {function():undefined} Callback wrapper.
 */
devtools.InspectorController.prototype.wrapCallback = function f(func) {
  // Just return as is.
  return func;
};


/**
 * @return {boolean} True iff inspector window is currently visible.
 */
devtools.InspectorController.prototype.isWindowVisible = function() {
  return this.windowVisible_;
};


/**
 * @return {string} Platform identifier.
 */
devtools.InspectorController.prototype.platform = function() { 
  return "windows"; 
};


/**
 * Closes inspector window.
 */
devtools.InspectorController.prototype.closeWindow =  function() {
  this.windowVisible_ = false;
};


/**
 * Attaches frontend to the backend.
 */
devtools.InspectorController.prototype.attach = function() {
};


/**
 * Detaches frontend from the backend.
 */
devtools.InspectorController.prototype.detach = function() {
};


/**
 * Clears console message log in the backend.
 */
devtools.InspectorController.prototype.clearMessages = function() {
};


/**
 * Returns true iff browser is currently in the search for node mode.
 * @return {boolean} True is currently searching for a node.
 */
devtools.InspectorController.prototype.searchingForNode = function() {
  return this.searchingForNode_;
};


/**
 * Initiates search for a given query starting on a given row.
 * @param {number} sourceRow Row to start searching from.
 * @param {string} query Query string for search for.
 */
devtools.InspectorController.prototype.search = function(sourceRow, query) {
};


/**
 * Toggles node search mode on/off.
 */
devtools.InspectorController.prototype.toggleNodeSearch = function() {
  this.searchingForNode_ = !this.searchingForNode_;
};


/**
 * Sets the inspector window height while in the attached mode.
 * @param {number} height Window height being set.
 */
devtools.InspectorController.prototype.setAttachedWindowHeight =
    function(height) {
  this.attachedWindowHeight_ = height;
};


/**
 * Moves window by the given offset.
 * @param {number} x X offset.
 * @param {number} y Y offset.
 */
devtools.InspectorController.prototype.moveByUnrestricted = function(x, y) {
};


/**
 * Adds resource with given identifier into the given iframe element.
 * @param {number} identifier Identifier of the resource to add into the frame.
 * @param {Element} element Element to add resource content to.
 */
devtools.InspectorController.prototype.addResourceSourceToFrame =
    function(identifier, element) {
};


/**
 * Adds given source of a given mimeType into the given iframe element.
 * @param {string} mimeType MIME type of the content to be added.
 * @param {string} source String content to be added.
 * @param {Element} element Element to add resource content to.
 */
devtools.InspectorController.prototype.addSourceToFrame =
    function(mimeType, source, element) {
  return true;
};


/**
 * Returns document node corresponding to the resource with given id.
 * @return {Node} Node containing the resource.
 */
devtools.InspectorController.prototype.getResourceDocumentNode =
    function(identifier) {
  return undefined;
};


/**
 * Highlights the given node on the page.
 * @param {Node} node Node to highlight.
 */
devtools.InspectorController.prototype.highlightDOMNode = function(node) {
  // Does nothing in stub.
};


/**
 * Clears current highlight.
 */
devtools.InspectorController.prototype.hideDOMNodeHighlight = function() {
  // Does nothing in stub.
};


/**
 * @return {window} Inspectable window instance.
 */
devtools.InspectorController.prototype.inspectedWindow = function() {
  return window;
};


/**
 * Notifies backend that the frontend has been successfully loaded.
 */
devtools.InspectorController.prototype.loaded = function() {
  // Does nothing in stub.
};


/**
 * @return {string} Url of the i18n-ed strings map.
 */
devtools.InspectorController.prototype.localizedStringsURL = function() {
  return undefined; 
};


/**
 * @return {boolean} True iff window is currently unloading.
 */
devtools.InspectorController.prototype.windowUnloading = function() {
  return false;
};


/**
 * @return {string} Identifiers of the panels that should be hidden.
 */
devtools.InspectorController.prototype.hiddenPanels = function() {
  return "";
};


/**
 * @return {boolean} True iff debugger is enabled.
 */
devtools.InspectorController.prototype.debuggerEnabled = function() { 
  return this.debuggerEnabled_;
};


/**
 * Enables debugger.
 */
devtools.InspectorController.prototype.enableDebugger = function() {
  this.debuggerEnabled_ = true;
};


/**
 * Disables debugger.
 */
devtools.InspectorController.prototype.disableDebugger = function() {
  this.debuggerEnabled_ = false;
};


/**
 * Adds breakpoint to the given line of the source with given ID.
 * @param {string} sourceID Source Id to add breakpoint to.
 * @param {number} line Line number to add breakpoint to.
 */
devtools.InspectorController.prototype.addBreakpoint =
    function(sourceID, line) {
};


/**
 * Removes breakpoint from the given line of the source with given ID.
 * @param {string} sourceID Source Id to remove breakpoint from.
 * @param {number} line Line number to remove breakpoint from.
 */
devtools.InspectorController.prototype.removeBreakpoint =
    function(sourceID, line) {
};


/**
 * Tells backend to pause in the debugger.
 */
devtools.InspectorController.prototype.pauseInDebugger = function() {
  // Does nothing in stub.
};


/**
 * Tells backend to pause in the debugger on the exceptions.
 */
devtools.InspectorController.prototype.pauseOnExceptions = function() { 
  // Does nothing in stub.
};


/**
 * Tells backend to resume execution.
 */
devtools.InspectorController.prototype.resumeDebugger = function() {
};


/**
 * @return {boolean} True iff profiler is enabled.
 */
devtools.InspectorController.prototype.profilerEnabled = function() { 
  return true; 
};


/**
 * Enables profiler.
 */
devtools.InspectorController.prototype.enableProfiler = function() {
  this.profilerEnabled_ = true;
};


/**
 * Disables profiler.
 */
devtools.InspectorController.prototype.disableProfiler = function() {
  this.profilerEnabled_ = false;
};


/**
 * Returns given callframe while on a debugger break.
 * @return {Object} Current call frame.
 */
devtools.InspectorController.prototype.currentCallFrame = function() {
  return undefined;
};


/**
 * Tells backend to start collecting profiler data.
 */
devtools.InspectorController.prototype.startProfiling = function() {
};


/**
 * Tells backend to stop collecting profiler data.
 */
devtools.InspectorController.prototype.stopProfiling = function() {
};


/**
 * @return {Array.<Object>} Profile snapshots array.
 */
devtools.InspectorController.prototype.profiles = function() { 
  return []; 
};


/**
 * @return {Array.<string>} Database table names available offline.
 */
devtools.InspectorController.prototype.databaseTableNames =
    function(database) {
  return [];
};


/**
 * Tells backend to step into the function in debugger.
 */
devtools.InspectorController.prototype.stepIntoStatementInDebugger =
    function() {
};


/**
 * Tells backend to step out of the function in debugger.
 */
devtools.InspectorController.prototype.stepOutOfFunctionInDebugger =
    function() {};


/**
 * Tells backend to step over the statement in debugger.
 */
devtools.InspectorController.prototype.stepOverStatementInDebugger =
    function() {
};
