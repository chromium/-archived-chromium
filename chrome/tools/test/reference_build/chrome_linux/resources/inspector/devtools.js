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


/**
 * Dispatches raw message from the host.
 * @param {Object} msg Message to dispatch.
 */
devtools.dispatch = function(msg) {
  var delegate = msg[0];
  var methodName = msg[1];
  var remoteName = 'Remote' + delegate.substring(0, delegate.length - 8);
  var agent = window[remoteName];
  if (!agent) {
    debugPrint('No remote agent "' + remoteName + '" found.');
    return;
  }
  var method = agent[methodName];
  if (!method) {
    debugPrint('No method "' + remoteName + '.' + methodName + '" found.');
    return;
  }
  method.apply(this, msg.slice(2));
};


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
 * Disable autocompletion in the console.
 * TODO(yurys): change WebKit implementation to allow asynchronous completion.
 * @override
 */
WebInspector.Console.prototype.completions = function(
    wordRange, bestMatchOnly) {
  return null;
};


/**
 * @override
 */
WebInspector.ElementsPanel.prototype.updateStyles = function(forceUpdate) {
  var stylesSidebarPane = this.sidebarPanes.styles;
  if (!stylesSidebarPane.expanded || !stylesSidebarPane.needsUpdate) {
    return;
  }
  this.invokeWithStyleSet_(function(node) {
    stylesSidebarPane.needsUpdate = !!node;
    stylesSidebarPane.update(node, null, forceUpdate);
  });
};


/**
 * @override
 */
WebInspector.ElementsPanel.prototype.updateMetrics = function() {
  var metricsSidebarPane = this.sidebarPanes.metrics;
  if (!metricsSidebarPane.expanded || !metricsSidebarPane.needsUpdate) {
    return;
  }
  this.invokeWithStyleSet_(function(node) {
    metricsSidebarPane.needsUpdate = !!node;
    metricsSidebarPane.update(node);
  });
};


/**
 * Temporarily sets style fetched from the inspectable tab to the currently
 * focused node, invokes updateUI callback and clears the styles.
 * @param {function(Node):undefined} updateUI Callback to call while styles are
 *     set.
 */
WebInspector.ElementsPanel.prototype.invokeWithStyleSet_ =
    function(updateUI) {
  var node = this.focusedDOMNode;
  if (node && node.nodeType === Node.TEXT_NODE && node.parentNode)
    node = node.parentNode;
  
  if (node && node.nodeType == Node.ELEMENT_NODE) {
    var callback = function(stylesStr) {
      var styles = JSON.parse(stylesStr);
      if (!styles.computedStyle) {
        return;
      }
      node.setStyles(styles.computedStyle, styles.inlineStyle,
          styles.styleAttributes, styles.matchedCSSRules);
      updateUI(node);
      node.clearStyles();
    };
    devtools.tools.getDomAgent().getNodeStylesAsync(
        node,
        !Preferences.showUserAgentStyles,
        callback);
  } else {
    updateUI(null);
  }
};


/**
 * @override
 */
WebInspector.MetricsSidebarPane.prototype.editingCommitted =
    function(element, userInput, previousContent, context) {
  if (userInput === previousContent) {
    // nothing changed, so cancel
    return this.editingCancelled(element, context);
  }

  if (context.box !== "position" && (!userInput || userInput === "\u2012")) {
    userInput = "0px";
  } else if (context.box === "position" &&
      (!userInput || userInput === "\u2012")) {
    userInput = "auto";
  }

  // Append a "px" unit if the user input was just a number.
  if (/^\d+$/.test(userInput)) {
    userInput += "px";
  }
  devtools.tools.getDomAgent().setStylePropertyAsync(
      this.node,
      context.styleProperty,
      userInput,
      WebInspector.updateStylesAndMetrics_);
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
        var prototypes = JSON.parse(json);
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
    var resource = WebInspector.resources[identifier];
    if (InspectorController.addSourceToFrame(resource.mimeType, source,
                                             element)) {
      delete self._frameNeedsSetup;
      if (resource.type === WebInspector.Resource.Type.Script) {
        self.sourceFrame.addEventListener('syntax highlighting complete',
            self._syntaxHighlightingComplete, self);
        self.sourceFrame.syntaxHighlightJavascript();
      } else {
        self._sourceFrameSetupFinished();
      }
    }
  });
  return true;
};


/**
 * This override is necessary for adding script source asynchronously.
 * @override
 */
WebInspector.ScriptView.prototype.setupSourceFrameIfNeeded = function() {
  if (!this._frameNeedsSetup) {
    return;
  }

  this.attach();
  
  if (this.script.source) {
    this.didResolveScriptSource_();
  } else {
    var self = this;
    devtools.tools.getDebuggerAgent().resolveScriptSource(
        this.script.sourceID,
        function(source) {
          self.script.source = source || '<source is not available>';
          self.didResolveScriptSource_();
        });
  }
};


/**
 * Performs source frame setup when script source is aready resolved.
 */
WebInspector.ScriptView.prototype.didResolveScriptSource_ = function() {
  if (!InspectorController.addSourceToFrame(
      "text/javascript", this.script.source, this.sourceFrame.element)) {
    return;
  }

  delete this._frameNeedsSetup;

  this.sourceFrame.addEventListener(
      "syntax highlighting complete", this._syntaxHighlightingComplete, this);
  this.sourceFrame.syntaxHighlightJavascript();
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
  var props = JSON.parse(json);
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
  var obj = this.parentObject[this.propertyName];
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
  object = object.resolvedValue;
  for (var name in object) {
    this.appendChild(new constructor(object, name));
  }
};


/**
 * @override
 */
WebInspector.StylePropertyTreeElement.prototype.toggleEnabled =
    function(event) {
  var enabled = event.target.checked;
  devtools.tools.getDomAgent().toggleNodeStyleAsync(
      this.style,
      enabled,
      this.name,
      WebInspector.updateStylesAndMetrics_);
};


/**
 * @override
 */
WebInspector.StylePropertyTreeElement.prototype.applyStyleText = function(
    styleText, updateInterface) {
  devtools.tools.getDomAgent().applyStyleTextAsync(this.style, this.name,
      styleText,
      function() {
        if (updateInterface) {
          WebInspector.updateStylesAndMetrics_();
        }
      });
};


/**
 * Forces update of styles and metrics sidebar panes.
 */
WebInspector.updateStylesAndMetrics_ = function() {
  WebInspector.panels.elements.sidebarPanes.metrics.needsUpdate = true;
  WebInspector.panels.elements.updateMetrics();
  WebInspector.panels.elements.sidebarPanes.styles.needsUpdate = true;
  WebInspector.panels.elements.updateStyles(true);
};


/**
 * This function overrides standard searchableViews getters to perform search
 * only in the current view (other views are loaded asynchronously, no way to
 * search them yet).
 */
WebInspector.searchableViews_ = function() {
  var views = [];
  const visibleView = this.visibleView;
  if (visibleView && visibleView.performSearch) {
    views.push(visibleView);
  }
  return views;
};


/**
 * @override
 */
WebInspector.ResourcesPanel.prototype.__defineGetter__(
    'searchableViews',
    WebInspector.searchableViews_);


/**
 * @override
 */
WebInspector.ScriptsPanel.prototype.__defineGetter__(
    'searchableViews',
    WebInspector.searchableViews_);


/**
 * @override
 */
WebInspector.Console.prototype._evalInInspectedWindow = function(expression) {
  if (WebInspector.panels.scripts.paused)
    return WebInspector.panels.scripts.evaluateInSelectedCallFrame(expression);

  var console = this;
  devtools.tools.evaluateJavaScript(expression, function(response) {
    // TODO(yurys): send exception information along with the response
    var exception = false;
    console.addMessage(new WebInspector.ConsoleCommandResult(
        response, exception, null /* commandMessage */));
  });
  // TODO(yurys): refactor WebInspector.Console so that the result is added into
  // the command log message.
  return 'evaluating...';
};


(function() {
  var oldShow = WebInspector.ScriptsPanel.prototype.show;
  WebInspector.ScriptsPanel.prototype.show =  function() {
    devtools.tools.getDebuggerAgent().initializeScriptsCache();
    oldShow.call(this);
  };
})();


/**
 * We don't use WebKit's BottomUpProfileDataGridTree, instead using
 * our own (because BottomUpProfileDataGridTree's functionality is
 * implemented in profile_view.js for V8's Tick Processor).
 *
 * @param {WebInspector.ProfileView} profileView Profile view.
 * @param {devtools.profiler.ProfileView} profile Profile.
 */
WebInspector.BottomUpProfileDataGridTree = function(profileView, profile) {
  return WebInspector.buildProfileDataGridTree_(
      profileView, profile.heavyProfile);
};


/**
 * We don't use WebKit's TopDownProfileDataGridTree, instead using
 * our own (because TopDownProfileDataGridTree's functionality is
 * implemented in profile_view.js for V8's Tick Processor).
 *
 * @param {WebInspector.ProfileView} profileView Profile view.
 * @param {devtools.profiler.ProfileView} profile Profile.
 */
WebInspector.TopDownProfileDataGridTree = function(profileView, profile) {
  return WebInspector.buildProfileDataGridTree_(
      profileView, profile.treeProfile);
};


/**
 * A helper function, checks whether a profile node has visible children.
 *
 * @param {devtools.profiler.ProfileView.Node} profileNode Profile node.
 * @return {boolean} Whether a profile node has visible children.
 */
WebInspector.nodeHasChildren_ = function(profileNode) {
  var children = profileNode.children;
  for (var i = 0, n = children.length; i < n; ++i) {
    if (children[i].visible) {
      return true;
    }
  }
  return false;
};


/**
 * Common code for populating a profiler grid node or a tree with
 * given profile nodes.
 *
 * @param {WebInspector.ProfileDataGridNode|
 *     WebInspector.ProfileDataGridTree} viewNode Grid node or a tree.
 * @param {WebInspector.ProfileView} profileView Profile view.
 * @param {Array<devtools.profiler.ProfileView.Node>} children Profile nodes.
 * @param {WebInspector.ProfileDataGridTree} owningTree Grid tree.
 */
WebInspector.populateNode_ = function(
    viewNode, profileView, children, owningTree) {
  for (var i = 0, n = children.length; i < n; ++i) {
    var child = children[i];
    if (child.visible) {
      viewNode.appendChild(
          new WebInspector.ProfileDataGridNode(
              profileView, child, owningTree,
              WebInspector.nodeHasChildren_(child)));
    }
  }
};


/**
 * A helper function for building a profile grid tree.
 *
 * @param {WebInspector.ProfileView} profileview Profile view.
 * @param {devtools.profiler.ProfileView} profile Profile.
 * @return {WebInspector.ProfileDataGridTree} Profile grid tree.
 */
WebInspector.buildProfileDataGridTree_ = function(profileView, profile) {
  var children = profile.head.children;
  var dataGridTree = new WebInspector.ProfileDataGridTree(
      profileView, profile.head);
  WebInspector.populateNode_(dataGridTree, profileView, children, dataGridTree);
  return dataGridTree;
};


/**
 * @override
 */
WebInspector.ProfileDataGridNode.prototype._populate = function(event) {
  var children = this.profileNode.children;
  WebInspector.populateNode_(this, this.profileView, children, this.tree);
  this.removeEventListener("populate", this._populate, this);
};


// As columns in data grid can't be changed after initialization,
// we need to intercept the constructor and modify columns upon creation.
(function InterceptDataGridForProfiler() {
   var originalDataGrid = WebInspector.DataGrid;
   WebInspector.DataGrid = function(columns) {
     if (('average' in columns) && ('calls' in columns)) {
       delete columns['average'];
       delete columns['calls'];
     }
     return new originalDataGrid(columns);
   };
})();


/**
 * @override
 * TODO(pfeldman): Add l10n.
 */
WebInspector.UIString = function(string)
{
  return String.vsprintf(string, Array.prototype.slice.call(arguments, 1));
}
