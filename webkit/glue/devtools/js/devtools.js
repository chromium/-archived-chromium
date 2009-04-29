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
  RemoteToolsAgent.AddMessageToConsole =
      goog.bind(this.addMessageToConsole, this);
  this.debuggerAgent_ = new devtools.DebuggerAgent();
  this.domAgent_ = new devtools.DomAgent();
  this.netAgent_ = new devtools.NetAgent();
};


/**
 * Resets tools agent to its initial state.
 */
devtools.ToolsAgent.prototype.reset = function() {
  this.domAgent_.reset();
  this.netAgent_.reset();
  this.debuggerAgent_.reset();
  
  this.domAgent_.getDocumentElementAsync();
  this.debuggerAgent_.requestScripts();
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
 * @param {Object} message Message object to add.
 * @see tools_agent.h
 */
devtools.ToolsAgent.prototype.addMessageToConsole = function(message) {
  var console = WebInspector.console;
  if (console) {
    console.addMessage(new WebInspector.ConsoleMessage(
        message.source, message.level, message.line, message.sourceId,
        undefined, 1, message.text));
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
        '', undefined, 1, '', undefined, 1, text));
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
  devtools.tools.reset();

  Preferences.ignoreWhitespace = false;
  oldLoaded.call(this);

  DevToolsHost.loaded();
};


var webkitUpdateChildren =
    WebInspector.ElementsTreeElement.prototype.updateChildren;


/**
 * @override
 */
WebInspector.ElementsTreeElement.prototype.updateChildren = function() {
  var self = this;
  devtools.tools.getDomAgent().getChildNodesAsync(this.representedObject,
      function() {
        webkitUpdateChildren.call(self);
      });
};


/**
 * @override
 */
WebInspector.ElementsPanel.prototype.performSearch = function(query) {
  this.searchCanceled();
  devtools.tools.getDomAgent().performSearch(query,
      goog.bind(this.performSearchCallback_, this));
};


WebInspector.ElementsPanel.prototype.performSearchCallback_ = function(nodes) {
  for (var i = 0; i < nodes.length; ++i) {
    var treeElement = this.treeOutline.findTreeElement(nodes[i]);
    if (treeElement)
      treeElement.highlighted = true;
  }
  
  if (nodes.length) {
    this.currentSearchResultIndex_ = 0;
    this.focusedDOMNode = nodes[0];
  }
  
  this.searchResultCount_ = nodes.length;
};


/**
 * @override
 */
WebInspector.ElementsPanel.prototype.searchCanceled = function() {
  this.currentSearchResultIndex_ = 0;
  this.searchResultCount_ = 0;
  devtools.tools.getDomAgent().searchCanceled(
      goog.bind(this.searchCanceledCallback_, this));
};


WebInspector.ElementsPanel.prototype.searchCanceledCallback_ = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    var treeElement = this.treeOutline.findTreeElement(nodes[i]);
    if (treeElement)
      treeElement.highlighted = false;
  }
};


/**
 * @override
 */
WebInspector.ElementsPanel.prototype.jumpToNextSearchResult = function() {
  if (!this.searchResultCount_)
    return;

  if (++this.currentSearchResultIndex_ >= this.searchResultCount_)
    this.currentSearchResultIndex_ = 0;

  this.focusedDOMNode = devtools.tools.getDomAgent().
      getSearchResultNode(this.currentSearchResultIndex_);
};


/**
 * @override
 */
WebInspector.ElementsPanel.prototype.jumpToPreviousSearchResult = function() {
  if (!this.searchResultCount_)
    return;

  if (--this.currentSearchResultIndex_ < 0)
    this.currentSearchResultIndex_ = this.searchResultCount_ - 1;

  this.focusedDOMNode = devtools.tools.getDomAgent().
      getSearchResultNode(this.currentSearchResultIndex_);
};


/**
 * @override
 */
WebInspector.Console.prototype._evalInInspectedWindow = function(expr) {
  return devtools.tools.evaluate(expr);
};


/**
 * @override
 */
WebInspector.ElementsPanel.prototype.updateStyles = function(forceUpdate) {
  var stylesSidebarPane = this.sidebarPanes.styles;
  if (!stylesSidebarPane.expanded || !stylesSidebarPane.needsUpdate)
    return;
   
  var node = this.focusedDOMNode;
  if (node && node.nodeType === Node.TEXT_NODE && node.parentNode)
    node = node.parentNode;
  
  if (node && node.nodeType == Node.ELEMENT_NODE) {
    var callback = function(stylesStr) {
      var styles = goog.json.parse(stylesStr);
      if (!styles.computedStyle) {
        return;
      }
      node.setStyles(styles.computedStyle, styles.inlineStyle,
          styles.styleAttributes, styles.matchedCSSRules);
      stylesSidebarPane.update(node, null, forceUpdate);
      stylesSidebarPane.needsUpdate = false;
      node.clearStyles();
    };

    devtools.tools.getDomAgent().getNodeStylesAsync(
        node,
        !Preferences.showUserAgentStyles,
        callback);
  } else {
    stylesSidebarPane.update(null, null, forceUpdate);
    stylesSidebarPane.needsUpdate = false;
  }
};


/**
 * @override
 */
WebInspector.PropertiesSidebarPane.prototype.update = function(object) {
  var body = this.bodyElement;
  body.removeChildren();

  this.sections = [];

  if (!object) {
    return;
  }

  
  var self = this;
  devtools.tools.getDomAgent().getNodePrototypesAsync(object.id_, 
      function(json) {
        // Get array of prototype user-friendly names.
        var prototypes = goog.json.parse(json);
        for (var i = 0; i < prototypes.length; ++i) {
          var prototype = {};
          prototype.id_ = object.id_;
          prototype.protoDepth_ = i;
          var section = new WebInspector.SidebarObjectPropertiesSection(
              prototype,
              prototypes[i]);
          self.sections.push(section);
          body.appendChild(section.element);
        }
      });
};


/**
 * Our implementation of ObjectPropertiesSection for Elements tab.
 * @constructor
 */
WebInspector.SidebarObjectPropertiesSection = function(object, title) {
  WebInspector.ObjectPropertiesSection.call(this, object, title,
      null /* subtitle */, null /* emptyPlaceholder */,
      null /* ignoreHasOwnProperty */, null /* extraProperties */,
      WebInspector.SidebarObjectPropertyTreeElement /* treeElementConstructor */
      );
};
goog.inherits(WebInspector.SidebarObjectPropertiesSection,
    WebInspector.ObjectPropertiesSection);


/**
 * @override
 */
WebInspector.SidebarObjectPropertiesSection.prototype.onpopulate = function() {
  var nodeId = this.object.id_;
  var protoDepth = this.object.protoDepth_;
  var path = [];
  devtools.tools.getDomAgent().getNodePropertiesAsync(nodeId, path, protoDepth,
      goog.partial(WebInspector.didGetNodePropertiesAsync_,
          this.propertiesTreeOutline,
          this.treeElementConstructor,
          nodeId,
          path));
};


/**
 * Our implementation of ObjectPropertyTreeElement for Elements tab.
 * @constructor
 */
WebInspector.SidebarObjectPropertyTreeElement = function(parentObject,
    propertyName) {
  WebInspector.ObjectPropertyTreeElement.call(this, parentObject,
      propertyName);
};
goog.inherits(WebInspector.SidebarObjectPropertyTreeElement,
    WebInspector.ObjectPropertyTreeElement);


/**
 * @override
 */
WebInspector.SidebarObjectPropertyTreeElement.prototype.onpopulate =
    function() {
  var nodeId = this.parentObject.devtools$$nodeId_;
  var path = this.parentObject.devtools$$path_.slice(0);
  path.push(this.propertyName);
  devtools.tools.getDomAgent().getNodePropertiesAsync(nodeId, path, -1, 
      goog.partial(
          WebInspector.didGetNodePropertiesAsync_,
          this,
          this.treeOutline.section.treeElementConstructor,
          nodeId, path));
};


/**
 * This override is necessary for starting highlighting after the resource
 * was added into the frame.
 * @override
 */
WebInspector.SourceView.prototype.setupSourceFrameIfNeeded = function() {
  if (!this._frameNeedsSetup) {
    return;
  }

  this.attach();

  var self = this;
  var identifier = this.resource.identifier;
  var element = this.sourceFrame.element;
  var netAgent = devtools.tools.getNetAgent();

  netAgent.getResourceContentAsync(identifier, function(source) {
    var resource = netAgent.getResource(identifier);
    if (InspectorController.addSourceToFrame(resource.mimeType, source,
                                             element)) {
      delete self._frameNeedsSetup;
      if (resource.type === WebInspector.Resource.Type.Script) {
        self.sourceFrame.addEventListener('syntax highlighting complete',
            self._syntaxHighlightingComplete, self);
        self.sourceFrame.syntaxHighlightJavascript();
      }
    } else {
      self._sourceFrameSetupFinished();
    }
  });
  return true;
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


/**
 * Replace WebKit method with our own implementation to use our call stack
 * representation. Original method uses Object.prototype.toString.call to
 * learn if scope object is a JSActivation which doesn't work in Chrome.
 */
WebInspector.ScopeChainSidebarPane.prototype.update = function(callFrame) {
  this.bodyElement.removeChildren();

  this.sections = [];
  this.callFrame = callFrame;

  if (!callFrame) {
      var infoElement = document.createElement('div');
      infoElement.className = 'info';
      infoElement.textContent = WebInspector.UIString('Not Paused');
      this.bodyElement.appendChild(infoElement);
      return;
  }

  if (!callFrame._expandedProperties) {
    callFrame._expandedProperties = {};
  }

  var scopeObject = callFrame.localScope;
  var title = WebInspector.UIString('Local');
  var subtitle = Object.describe(scopeObject, true);
  var emptyPlaceholder = null;
  var extraProperties = null;

  var section = new WebInspector.ObjectPropertiesSection(scopeObject, title,
      subtitle, emptyPlaceholder, true, extraProperties,
      WebInspector.ScopeChainSidebarPane.TreeElement);
  section.editInSelectedCallFrameWhenPaused = true;
  section.pane = this;

  section.expanded = true;

  this.sections.push(section);
  this.bodyElement.appendChild(section.element);
};


/**
 * Custom implementation of TreeElement that asynchronously resolves children
 * using the debugger agent.
 * @constructor
 */
WebInspector.ScopeChainSidebarPane.TreeElement = function(parentObject,
    propertyName) {
  WebInspector.ScopeVariableTreeElement.call(this, parentObject, propertyName);
}
WebInspector.ScopeChainSidebarPane.TreeElement.inherits(
    WebInspector.ScopeVariableTreeElement);


/**
 * @override
 */
WebInspector.ScopeChainSidebarPane.TreeElement.prototype.onpopulate =
    function() {
  var obj = this.parentObject[this.propertyName].value;
  devtools.tools.getDebuggerAgent().resolveChildren(obj,
      goog.bind(this.didResolveChildren_, this));
};


/**
 * Callback function used with the resolveChildren.
 */
WebInspector.ScopeChainSidebarPane.TreeElement.prototype.didResolveChildren_ =
    function(object) {
  this.removeChildren();
  
  var constructor = this.treeOutline.section.treeElementConstructor;
  for (var name in object) {
    this.appendChild(new constructor(object, name));
  }
};


/**
 * @override
 */
WebInspector.StylePropertyTreeElement.prototype.toggleEnabled =
    function(event) {
  var disabled = !event.target.checked;
  var self = this;
  devtools.tools.getDomAgent().toggleNodeStyleAsync(this.style, !disabled,
      this.name,
      function() {
        WebInspector.panels.elements.updateStyles(true);
      });
};
