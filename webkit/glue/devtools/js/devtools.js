// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tools is a main class that wires all components of the 
 * DevTools frontend together. It is also responsible for overriding existing
 * WebInspector functionality while it is getting upstreamed into WebCore.
 */
goog.provide('devtools.Tools');

goog.require('devtools.DebuggerAgent');
goog.require('devtools.DomAgent');
goog.require('devtools.NetAgent');

devtools.ToolsAgent = function() {
  RemoteToolsAgent.DidEvaluateJavaScript = devtools.Callback.processCallback;
  RemoteToolsAgent.DidExecuteUtilityFunction =
      devtools.Callback.processCallback;
  RemoteToolsAgent.UpdateFocusedNode =
      goog.bind(this.updateFocusedNode, this);
  RemoteToolsAgent.FrameNavigate =
      goog.bind(this.frameNavigate, this);
  this.debuggerAgent_ = new devtools.DebuggerAgent();
  this.domAgent_ = new devtools.DomAgent();
  this.netAgent_ = new devtools.NetAgent();
  this.reset();
};


/**
 * Rests tools agent to its initial state.
 */
devtools.ToolsAgent.prototype.reset = function() {
  this.domAgent_.reset();
  this.netAgent_.reset();
  this.domAgent_.getDocumentElementAsync();
};


/**
 * @param {string} script Script exression to be evaluated in the context of the
 *     inspected page.
 * @param {function(string):undefined} callback Function to call with the
 *     result.
 */
devtools.ToolsAgent.prototype.evaluateJavaScript = function(script, callback) {
  var callbackId = devtools.Callback.wrap(callback);
  RemoteToolsAgent.EvaluateJavaScript(callbackId, script);
};


/**
 * Returns all properties of the given node.
 * @param {devtools.DomNode} node Node to get properties for.
 * @param {Array.<string>} path Path to the object.
 * @param {number} protoDepth Depth to the exact proto level.
 * @param {function(string):undefined} callback Function to call with the
 *     result.
 */
devtools.ToolsAgent.prototype.getNodePropertiesAsync = function(nodeId,
    path, protoDepth, callback) {
  var callbackId = devtools.Callback.wrap(callback);
  RemoteToolsAgent.ExecuteUtilityFunction(callbackId,
      'devtools$$getProperties', nodeId,
      goog.json.serialize([path, protoDepth]));
};


/**
 * Returns prototype chain for a given node.
 * @param {devtools.DomNode} node Node to get prototypes for.
 * @param {Function} callback.
 */
devtools.ToolsAgent.prototype.getNodePrototypesAsync = function(nodeId,
    callback) {
  var callbackId = devtools.Callback.wrap(callback);
  RemoteToolsAgent.ExecuteUtilityFunction(callbackId,
      'devtools$$getPrototypes', nodeId, '');
};


/**
 * @return {devtools.DebuggerAgent} Debugger agent instance.
 */
devtools.ToolsAgent.prototype.getDebuggerAgent = function() {
  return this.debuggerAgent_;
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
 * @param {string} url Url frame navigated to.
 * @param {bool} topLevel True iff top level navigation occurred.
 * @see tools_agent.h
 */
devtools.ToolsAgent.prototype.frameNavigate = function(url, topLevel) {
  if (topLevel) {
    this.reset();
    WebInspector.reset();
  }
};


/**
 * Evaluates js expression.
 * @param {string} expr
 */
devtools.ToolsAgent.prototype.evaluate = function(expr) {
  RemoteToolsAgent.evaluate(expr);
};


/**
 * Prints string  to the inspector console or shows alert if the console doesn't
 * exist.
 * @param {string} text
 */
function debugPrint(text) {
  var console = WebInspector.console;
  if (console) {
    console.addMessage(new WebInspector.ConsoleMessage(
        "", undefined, 1, "", undefined, 1, text));
  } else {
    alert(text);
  }
}


/**
 * Global instance of the tools agent.
 * @type {devtools.ToolsAgent}
 */
devtools.tools = null;


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
  devtools.tools.getDebuggerAgent().requestScripts();
};


var webkitUpdateChildren =
    WebInspector.ElementsTreeElement.prototype.updateChildren;


WebInspector.ElementsTreeElement.prototype.updateChildren = function() {
  var self = this;
  devtools.tools.getDomAgent().getChildNodesAsync(this.representedObject,
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


WebInspector.ElementsPanel.prototype.updateStyles = function(forceUpdate) {
  var stylesSidebarPane = this.sidebarPanes.styles;
  if (!stylesSidebarPane.expanded || !stylesSidebarPane.needsUpdate)
    return;
   
  var node = this.focusedDOMNode;
  if (node && node.nodeType === Node.TEXT_NODE && node.parentNode)
    node = node.parentNode;
  
  if (node && node.nodeType == Node.ELEMENT_NODE) {
    var callback = function() {
      stylesSidebarPane.update(node, null, forceUpdate);
      stylesSidebarPane.needsUpdate = false;
    };

    devtools.tools.getDomAgent().getNodeStylesAsync(node,
        !Preferences.showUserAgentStyles, callback);
  } else {
    stylesSidebarPane.update(null, null, forceUpdate);
    stylesSidebarPane.needsUpdate = false;
  }
};


WebInspector.PropertiesSidebarPane.prototype.update = function(object) {
  var body = this.bodyElement;
  body.removeChildren();

  this.sections = [];

  if (!object) {
    return;
  }

  
  var self = this;
  devtools.tools.getNodePrototypesAsync(object.id_, function(json) {
    var prototypes = goog.json.parse(json);
    for (var i = 0; i < prototypes.length; ++i) {
      var prototype = {};
      prototype.id_ = object.id_;
      prototype.protoDepth_ = i;
      var section = new WebInspector.ObjectPropertiesSection(prototype,
          prototypes[i]);
      self.sections.push(section);
      body.appendChild(section.element);
    }
  });
};


WebInspector.ObjectPropertiesSection.prototype.onpopulate = function() {
  var nodeId = this.object.id_;
  var protoDepth = this.object.protoDepth_;
  var path = [];
  devtools.tools.getNodePropertiesAsync(nodeId, path, protoDepth,
      goog.partial(WebInspector.didGetNodePropertiesAsync_,
          this.propertiesTreeOutline,
          this.treeElementConstructor,
          nodeId,
          path));
};


WebInspector.ObjectPropertyTreeElement.prototype.onpopulate = function() {
  var nodeId = this.parentObject.devtools$$nodeId_;
  var path = this.parentObject.devtools$$path_.slice(0);
  path.push(this.propertyName);
  devtools.tools.getNodePropertiesAsync(nodeId, path, -1, goog.partial(
      WebInspector.didGetNodePropertiesAsync_,
      this,
      this.treeOutline.section.treeElementConstructor,
      nodeId, path));
};


/**
 * Dummy object used during properties inspection.
 * @see WebInspector.didGetNodePropertiesAsync_
 */
WebInspector.dummyObject_ = { 'foo' : 'bar' };


/**
 * Dummy function used during properties inspection.
 * @see WebInspector.didGetNodePropertiesAsync_
 */
WebInspector.dummyFunction_ = function() {};


/**
 * Callback function used with the getNodeProperties.
 */
WebInspector.didGetNodePropertiesAsync_ = function(treeOutline, constructor,
    nodeId, path, json) {
  var props = goog.json.parse(json);
  var properties = [];
  var obj = {};
  obj.devtools$$nodeId_ = nodeId;
  obj.devtools$$path_ = path;
  for (var i = 0; i < props.length; i += 3) {
    var type = props[i];
    var name = props[i + 1];
    var value = props[i + 2];
    properties.push(name);
    if (type == 'object') {
      // fake object is going to be replaced on expand.
      obj[name] = WebInspector.dummyObject_;
    } else if (type == 'function') {
      // fake function is going to be replaced on expand.
      obj[name] = WebInspector.dummyFunction_;
    } else {
      obj[name] = value;
    }
  }
  properties.sort();

  treeOutline.removeChildren();

  for (var i = 0; i < properties.length; ++i) {
    var propertyName = properties[i];
    treeOutline.appendChild(new constructor(obj, propertyName));
  }
};
