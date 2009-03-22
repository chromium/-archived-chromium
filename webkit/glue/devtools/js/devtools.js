// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tools is a main class that wires all components of the 
 * DevTools frontend together. It is also responsible for overriding existing
 * WebInspector functionality while it is getting upstreamed into WebCore.
 */
goog.provide('devtools.Tools');

goog.require('devtools.Dom');
goog.require('devtools.Net');

devtools.Tools = function() {
};


// ToolsAgent implementation.
devtools.Tools.prototype.updateFocusedNode = function(node_id) {
  var node = dom.getNodeForId(node_id);
  WebInspector.updateFocusedNode(node);
};

// Frontend global objects.
var dom = new devtools.DomDocument();
var net = new devtools.Net();
var tools = new devtools.Tools();
var context = {};  // Used by WebCore's inspector routines.

// Overrides for existing WebInspector methods.
// TODO(pfeldman): Patch WebCore and upstream changes.

var oldLoaded = WebInspector.loaded;
WebInspector.loaded = function() {
  oldLoaded.call(this);
  Preferences.ignoreWhitespace = false;
  DevToolsHost.getDocumentElement(function(root) {
    dom.setDocumentElement(eval(root));
  });
};


WebInspector.ElementsTreeElement.prototype.onpopulate = function() {
  if (this.children.length || this.whitespaceIgnored !== 
      Preferences.ignoreWhitespace)
    return;
  this.whitespaceIgnored = Preferences.ignoreWhitespace;
  var self = this;
  var id = this.representedObject.id;
  DevToolsHost.getChildNodes(id, function(children) {
    dom.setChildren(id, eval(children));
    self.updateChildren();
  });
};
