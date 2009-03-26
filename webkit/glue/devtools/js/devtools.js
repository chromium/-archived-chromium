// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tools is a main class that wires all components of the 
 * DevTools frontend together. It is also responsible for overriding existing
 * WebInspector functionality while it is getting upstreamed into WebCore.
 */
goog.provide('devtools.Tools');

goog.require('devtools.DomAgent');
goog.require('devtools.NetAgent');

devtools.ToolsAgent = function() {
  RemoteToolsAgent.UpdateFocusedNode =
      goog.bind(this.updateFocusedNode, this);
  RemoteToolsAgent.FrameNavigate =
      goog.bind(this.frameNavigate, this);
  this.domAgent_ = new devtools.DomAgent();
  this.netAgent_ = new devtools.NetAgent();
  this.reset();
};


/**
 * Rests tools agent to its initial state.
 */
devtools.ToolsAgent.prototype.reset = function() {
  this.setEnabled(true);
  this.domAgent_.reset();
  this.domAgent_.getDocumentElementAsync();
};


/**
 * DomAgent accessor.
 * @return {devtools.DomAgent} Dom agent instance.
 */
devtools.ToolsAgent.prototype.getDomAgent = function() {
  return this.domAgent_;
};


/**
 * NetAgent accessor.
 * @return {devtools.NetAgent} Net agent instance.
 */
devtools.ToolsAgent.prototype.getNetAgent = function() {
  return this.netAgent_;
};


/**
 * @see tools_agent.h
 */
devtools.ToolsAgent.prototype.updateFocusedNode = function(nodeId) {
  var node = this.domAgent_.getNodeForId(nodeId);
  WebInspector.updateFocusedNode(node);
};


/**
 * @see tools_agent.h
 */
devtools.ToolsAgent.prototype.frameNavigate = function(url, topLevel) {
  this.reset();
  WebInspector.reset();
};


/**
 * @see tools_agent.h
 */
devtools.ToolsAgent.prototype.setEnabled = function(enabled) {
  RemoteToolsAgent.SetEnabled(enabled);
};



/**
 * Evaluates js expression.
 * @param {string} expr
 */
devtools.ToolsAgent.prototype.evaluate = function(expr) {
  RemoteToolsAgent.evaluate(expr);
};


// Frontend global objects.
var devtools.tools;

var context = {};  // Used by WebCore's inspector routines.

///////////////////////////////////////////////////////////////////////////////
// Here and below are overrides to existing WebInspector methods only.
// TODO(pfeldman): Patch WebCore and upstream changes.
var oldLoaded = WebInspector.loaded;
WebInspector.loaded = function() {
  devtools.tools = new devtools.ToolsAgent();

  Preferences.ignoreWhitespace = false;
  oldLoaded.call(this);

  DevToolsHost.loaded();
};


var webkitUpdateChildren =
    WebInspector.ElementsTreeElement.prototype.updateChildren;


WebInspector.ElementsTreeElement.prototype.updateChildren = function() {
  var self = this;
  devtools.tools.getDomAgent().getChildNodesAsync(this.representedObject.id, 
      function() {
        webkitUpdateChildren.call(self);
      });
};


WebInspector.ElementsPanel.prototype.performSearch = function(query) {
  this.searchCanceled();
  var self = this;
  devtools.tools.getDomAgent().performSearch(query, function(node) {
    var treeElement = self.treeOutline.findTreeElement(node);
    if (treeElement)
      treeElement.highlighted = true;
  });
};


WebInspector.ElementsPanel.prototype.searchCanceled = function() {
  var self = this;
  devtools.tools.getDomAgent().searchCanceled(function(node) {
    var treeElement = self.treeOutline.findTreeElement(node);
    if (treeElement)
      treeElement.highlighted = false;
  });
};


WebInspector.ElementsPanel.prototype.jumpToNextSearchResult = function() {
};


WebInspector.ElementsPanel.prototype.jumpToPreviousSearchResult = function() {
};


WebInspector.Console.prototype._evalInInspectedWindow = function(expr) {
  return devtools.tools.evaluate(expr);
};
