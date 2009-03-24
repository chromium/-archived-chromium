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
};


// ToolsAgent implementation.
devtools.ToolsAgent.prototype.updateFocusedNode = function(node_id) {
  var node = domAgent.getNodeForId(node_id);
  WebInspector.updateFocusedNode(node);
};


devtools.ToolsAgent.prototype.setDomAgentEnabled = function(enabled) {
  RemoteToolsAgent.SetDomAgentEnabled(enabled);
};


devtools.ToolsAgent.prototype.setNetAgentEnabled = function(enabled) {
  RemoteToolsAgent.SetNetAgentEnabled(enabled);
};


// Frontend global objects.
var domAgent;
var netAgent;
var toolsAgent;

var context = {};  // Used by WebCore's inspector routines.

// Overrides for existing WebInspector methods.
// TODO(pfeldman): Patch WebCore and upstream changes.
var oldLoaded = WebInspector.loaded;
WebInspector.loaded = function() {
  domAgent = new devtools.DomAgent();
  netAgent = new devtools.NetAgent();
  toolsAgent = new devtools.ToolsAgent();

  Preferences.ignoreWhitespace = false;
  toolsAgent.setDomAgentEnabled(true);
  toolsAgent.setNetAgentEnabled(true);
  oldLoaded.call(this);
  domAgent.getDocumentElementAsync();
};


var webkitUpdateChildren =
    WebInspector.ElementsTreeElement.prototype.updateChildren;

WebInspector.ElementsTreeElement.prototype.updateChildren = function() {
  var self = this;
  domAgent.getChildNodesAsync(this.representedObject.id, function() {
      webkitUpdateChildren.call(self);
  });
};
