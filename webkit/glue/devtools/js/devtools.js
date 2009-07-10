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


/**
 * Dispatches raw message from the host.
 * @param {string} remoteName
 * @prama {string} methodName
 * @param {Object} msg Message to dispatch.
 */
devtools.dispatch = function(remoteName, methodName, msg) {
  remoteName = 'Remote' + remoteName.substring(0, remoteName.length - 8);
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
  method.apply(this, msg);
};


devtools.ToolsAgent = function() {
  RemoteToolsAgent.DidEvaluateJavaScript = devtools.Callback.processCallback;
  RemoteToolsAgent.DidExecuteUtilityFunction =
      devtools.Callback.processCallback;
  RemoteToolsAgent.UpdateFocusedNode =
      goog.bind(this.updateFocusedNode_, this);
  RemoteToolsAgent.FrameNavigate =
      goog.bind(this.frameNavigate_, this);
  RemoteToolsAgent.AddMessageToConsole =
      goog.bind(this.addMessageToConsole_, this);
  RemoteToolsAgent.DispatchOnClient =
      goog.bind(this.dispatchOnClient_, this);
  RemoteToolsAgent.SetResourcesPanelEnabled =
      goog.bind(this.setResourcesPanelEnabled_, this);
  this.debuggerAgent_ = new devtools.DebuggerAgent();
  this.domAgent_ = new devtools.DomAgent();
};


/**
 * Resets tools agent to its initial state.
 */
devtools.ToolsAgent.prototype.reset = function() {
  DevToolsHost.reset();
  this.domAgent_.reset();
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
  var callbackId = devtools.Callback.wrap(function(result) {
    var pair = JSON.parse(result);
    if (callback) {
      callback(pair[0], pair[1]);
    }
  });
  RemoteToolsAgent.ExecuteUtilityFunction(callbackId,
      'evaluate', JSON.stringify([script]));
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
 * @see tools_agent.h
 * @private
 */
devtools.ToolsAgent.prototype.updateFocusedNode_ = function(nodeId) {
  var node = this.domAgent_.getNodeForId(nodeId);
  WebInspector.updateFocusedNode(node);
};


/**
 * @param {string} url Url frame navigated to.
 * @see tools_agent.h
 * @private
 */
devtools.ToolsAgent.prototype.frameNavigate_ = function(url) {
  this.reset();
  // Do not reset Profiles panel.
  var profiles = null;
  if ('profiles' in WebInspector.panels) {
    profiles = WebInspector.panels['profiles'];
    delete WebInspector.panels['profiles'];
  }
  WebInspector.reset();
  if (profiles != null) {
    WebInspector.panels['profiles'] = profiles;
  }
};


/**
 * @param {Object} message Message object to add.
 * @see tools_agent.h
 * @private
 */
devtools.ToolsAgent.prototype.addMessageToConsole_ = function(message) {
  var console = WebInspector.console;
  if (console) {
    console.addMessage(new WebInspector.ConsoleMessage(
        message.source, message.level, message.line, message.sourceId,
        undefined, 1, message.text));
  }
};


/**
 * @param {string} message Serialized call to be dispatched on WebInspector.
 * @private
 */
devtools.ToolsAgent.prototype.dispatchOnClient_ = function(message) {
  WebInspector.dispatch.apply(WebInspector, JSON.parse(message));
};


/**
 * Evaluates js expression.
 * @param {string} expr
 */
devtools.ToolsAgent.prototype.evaluate = function(expr) {
  RemoteToolsAgent.evaluate(expr);
};


/**
 * Enables / disables resource tracking.
 * @param {boolean} enabled Sets tracking status.
 * @param {boolean} always Determines whether tracking status should be sticky.
 */
devtools.ToolsAgent.prototype.setResourceTrackingEnabled = function(enabled,
    always) {
  RemoteToolsAgent.SetResourceTrackingEnabled(enabled, always);
};


/**
 * Enables / disables resources panel in the ui.
 * @param {boolean} enabled New panel status.
 */
devtools.ToolsAgent.prototype.setResourcesPanelEnabled_ = function(enabled) {
  InspectorController.resourceTrackingEnabled_ = enabled;
  // TODO(pfeldman): Extract this upstream.
  var panel = WebInspector.panels.resources;
  if (enabled) {
    panel.enableToggleButton.title =
        WebInspector.UIString("Resource tracking enabled. Click to disable.");
    panel.enableToggleButton.addStyleClass("toggled-on");
    panel.largerResourcesButton.removeStyleClass("hidden");
    panel.sortingSelectElement.removeStyleClass("hidden");
    panel.panelEnablerView.visible = false;
  } else {
    panel.enableToggleButton.title =
        WebInspector.UIString("Resource tracking disabled. Click to enable.");
    panel.enableToggleButton.removeStyleClass("toggled-on");
    panel.largerResourcesButton.addStyleClass("hidden");
    panel.sortingSelectElement.addStyleClass("hidden");
    panel.panelEnablerView.visible = true;
  }
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
 * Callback function used with the getNodeProperties.
 */
WebInspector.didGetNodePropertiesAsync_ = function(treeOutline, constructor,
    nodeId, path, json) {
  var props = JSON.parse(json);
  var properties = [];
  var obj = {};
  obj.devtools$$nodeId_ = nodeId;
  obj.devtools$$path_ = path;
  for (var i = 0; i < props.length; i += 4) {
    var type = props[i];
    var name = props[i + 1];
    var value = props[i + 2];
    var className = props[i + 3];
    properties.push(name);
    if (type == 'object' || type == 'function') {
      // fake object is going to be replaced on expand.
      obj[name] = new WebInspector.UnresolvedPropertyValue(type, className);
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
 * @param {string} type Type of the the property value('object' or 'function').
 * @param {string} className Class name of the property value.
 * @constructor
 */
WebInspector.UnresolvedPropertyValue = function(type, className) {
  this.type = type;
  this.className = className;
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

  var scopeChain = callFrame.scopeChain;
  var ScopeType = devtools.DebuggerAgent.ScopeType;
  for (var i = 0; i < scopeChain.length; ++i) {
    var scopeObject = scopeChain[i];
    var thisObject = null;
    var title;
    switch(scopeObject.type) {
      case ScopeType.Global:
        title = 'Global';
        break;
      case ScopeType.Local:
        title = 'Local';
        thisObject = callFrame.thisObject;
        break;
      case ScopeType.With:
        title = 'With';
        break;
      case ScopeType.Closure:
        title = 'Closure';
        break;
      default:
        title = '<Unknown scope type>';
    }

    var section = new WebInspector.ScopeChainPropertiesSection(
        scopeObject, title, thisObject);
    section.editInSelectedCallFrameWhenPaused = true;
    section.pane = this;

    // Only first scope is expanded by default(if it's not a global one).
    section.expanded = (i == 0) && (scopeObject.type != ScopeType.Global);

    this.sections.push(section);
    this.bodyElement.appendChild(section.element);
  }
};


/**
 * Our basic implementation of ObjectPropertiesSection for debugger object
 * represented in console.
 * @constructor
 */
WebInspector.ConsoleObjectPropertiesSection = function(object, title,
    extraProperties) {
  WebInspector.ObjectPropertiesSection.call(this, object, title,
      null /* subtitle */, null /* emptyPlaceholder */,
      true /* ignoreHasOwnProperty */, null /* extraProperties */,
      WebInspector.DebuggedObjectTreeElement);
};
goog.inherits(WebInspector.ConsoleObjectPropertiesSection,
    WebInspector.ObjectPropertiesSection);


/**
 * @override
 */
WebInspector.ConsoleObjectPropertiesSection.prototype.onpopulate = function() {
  devtools.tools.getDebuggerAgent().resolveChildren(
      this.object,
      goog.bind(this.didResolveChildren_, this));
};


/**
 * @param {Object} object
 */
WebInspector.ConsoleObjectPropertiesSection.prototype.didResolveChildren_ =
    function(object) {
  WebInspector.DebuggedObjectTreeElement.addResolvedChildren(
      object,
      this.propertiesTreeOutline,
      this.treeElementConstructor);
};


/**
 * Our implementation of ObjectPropertiesSection for scope variables.
 * @constructor
 */
WebInspector.ScopeChainPropertiesSection = function(object, title, thisObject) {
  WebInspector.ObjectPropertiesSection.call(this, object, title,
      null /* subtitle */, null /* emptyPlaceholder */,
      true /* ignoreHasOwnProperty */, null /* extraProperties */,
      WebInspector.DebuggedObjectTreeElement);
  this.thisObject_ = thisObject;
};
goog.inherits(WebInspector.ScopeChainPropertiesSection,
    WebInspector.ObjectPropertiesSection);


/**
 * @override
 */
WebInspector.ScopeChainPropertiesSection.prototype.onpopulate = function() {
  devtools.tools.getDebuggerAgent().resolveScope(
      this.object,
      goog.bind(this.didResolveChildren_, this));
};

/**
 * @param {Object} object
 */
WebInspector.ScopeChainPropertiesSection.prototype.didResolveChildren_ =
    function(object) {
  // Add this to the properties list if it's specified.
  if (this.thisObject_) {
    object.resolvedValue['this'] = this.thisObject_;
  }
  WebInspector.DebuggedObjectTreeElement.addResolvedChildren(
      object,
      this.propertiesTreeOutline,
      this.treeElementConstructor);
};


/**
 * Custom implementation of TreeElement that asynchronously resolves children
 * using the debugger agent.
 * @constructor
 */
WebInspector.DebuggedObjectTreeElement = function(parentObject,
    propertyName) {
  WebInspector.ScopeVariableTreeElement.call(this, parentObject, propertyName);
}
WebInspector.DebuggedObjectTreeElement.inherits(
    WebInspector.ScopeVariableTreeElement);


/**
 * @override
 */
WebInspector.DebuggedObjectTreeElement.prototype.onpopulate =
    function() {
  var obj = this.parentObject[this.propertyName];
  devtools.tools.getDebuggerAgent().resolveChildren(obj,
      goog.bind(this.didResolveChildren_, this), false /* no intrinsic */ );
};


/**
 * Callback function used with the resolveChildren.
 */
WebInspector.DebuggedObjectTreeElement.prototype.didResolveChildren_ =
    function(object) {
  this.removeChildren();
  WebInspector.DebuggedObjectTreeElement.addResolvedChildren(
      object,
      this,
      this.treeOutline.section.treeElementConstructor);
};


/**
 * Utility function used to populate children list of tree element representing
 * debugged object with values resolved through the debugger agent.
 * @param {Object} resolvedObject Object whose properties have been resolved.
 * @param {Element} treeElementContainer Container fot the HTML elements
 *     representing the resolved properties.
 * @param {function(object, string):Element} treeElementConstructor
 */
WebInspector.DebuggedObjectTreeElement.addResolvedChildren = function(
    resolvedObject, treeElementContainer, treeElementConstructor) {
  var object = resolvedObject.resolvedValue;
  var names = [];
  for (var name in object) {
    names.push(name);
  }
  names.sort();
  for (var i = 0; i < names.length; i++) {
    treeElementContainer.appendChild(
        new treeElementConstructor(object, names[i]));
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


WebInspector.Console.prototype.doEvalInWindow =
    function(expression, callback) {
  if (!expression ) {
    // Empty expression should evaluate to the global object for completions to
    // work.
    expression = "this";
  }
  devtools.tools.evaluateJavaScript(expression, callback);
};


WebInspector.ScriptsPanel.prototype.doEvalInCallFrame =
    function(callFrame, expression, callback) {
  if (!expression) {
    // Empty expression should eval to scope roots for completions to work.
    devtools.CallFrame.getVariablesInScopeAsync(callFrame, callback);
    return;
  }
  devtools.CallFrame.doEvalInCallFrame(callFrame, expression, callback);
};


(function() {
  var oldShow = WebInspector.ScriptsPanel.prototype.show;
  WebInspector.ScriptsPanel.prototype.show =  function() {
    devtools.tools.getDebuggerAgent().initUI();
    this.enableToggleButton.addStyleClass('hidden');
    oldShow.call(this);
  };
})();


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


// WebKit's profiler displays milliseconds with high resolution (shows
// three digits after the decimal point). We never have such resolution,
// as our minimal sampling rate is 1 ms. So we are disabling high resolution
// to avoid visual clutter caused by meaningless ".000" parts.
(function InterceptTimeDisplayInProfiler() {
   var originalDataGetter =
       WebInspector.ProfileDataGridNode.prototype.__lookupGetter__('data');
   WebInspector.ProfileDataGridNode.prototype.__defineGetter__('data',
     function() {
       var oldNumberSecondsToString = Number.secondsToString;
       Number.secondsToString = function(seconds, formatterFunction) {
         return oldNumberSecondsToString(seconds, formatterFunction, false);
       };
       var data = originalDataGetter.call(this);
       Number.secondsToString = oldNumberSecondsToString;
       return data;
     });
})();


(function InterceptProfilesPanelEvents() {
  var oldShow = WebInspector.ProfilesPanel.prototype.show;
  WebInspector.ProfilesPanel.prototype.show = function() {
    devtools.tools.getDebuggerAgent().initializeProfiling();
    this.enableToggleButton.addStyleClass('hidden');
    oldShow.call(this);
    // Show is called on every show event of a panel, so
    // we only need to intercept it once.
    WebInspector.ProfilesPanel.prototype.show = oldShow;
  };
})();


/**
 * @override
 * TODO(pfeldman): Add l10n.
 */
WebInspector.UIString = function(string) {
  return String.vsprintf(string, Array.prototype.slice.call(arguments, 1));
};


// There is no clear way of setting frame title yet. So sniffing main resource
// load.
(function OverrideUpdateResource() {
  var originalUpdateResource = WebInspector.updateResource;
  WebInspector.updateResource = function(identifier, payload) {
    originalUpdateResource.call(this, identifier, payload);
    var resource = this.resources[identifier];
    if (resource && resource.mainResource && resource.finished) {
      document.title = 'Developer Tools - ' + resource.url;
    }
  };
})();


// There is no clear way of rendering class name for scope variables yet.
(function OverrideObjectDescribe() {
  var oldDescribe = Object.describe;
  Object.describe = function(obj, abbreviated) {
    if (obj instanceof WebInspector.UnresolvedPropertyValue) {
      return obj.className;
    }

    var result = oldDescribe.call(Object, obj, abbreviated);
    if (result == 'Object' && obj.className) {
      return obj.className;
    }
    return result;
  };

  // This is needed to evaluate 'instanceof' against win.Node, etc.
  // Need a real window, not just InspectorController.inspectedWindow wrapper.
  var oldType = Object.type;
  Object.type = function(obj) {
    return oldType.call(this, obj, window);
  };
})();


Object.sortedProperties = function(obj) {
  var properties = [];
  for (var prop in obj) {
    if (prop != '___devtools_id') {
      properties.push(prop);
    }
  }
  properties.sort();
  return properties;
};


// Highlight extension content scripts in the scripts list.
(function () {
  var original = WebInspector.ScriptsPanel.prototype._addScriptToFilesMenu;
  WebInspector.ScriptsPanel.prototype._addScriptToFilesMenu = function(script) {
    var result = original.apply(this, arguments);
    var debuggerAgent = devtools.tools.getDebuggerAgent();
    var type = debuggerAgent.getScriptContextType(script.sourceID);
    var option = script.filesSelectOption;
    if (type == 'injected' && option) {
      option.addStyleClass('injected');
    }
    return result;
  };
})();


WebInspector.Console.prototype._formatobject = function(object, elem) {
  var section;
  if (object.handle && object.className) {
    object.ref = object.handle;
    var className = object.className;
    section = new WebInspector.ConsoleObjectPropertiesSection(object,
        className);
    section.pane = {
      callFrame: {
        _expandedProperties : { className : '' }
      }
    };
  } else {
    var wrapper = {};
    wrapper.id_ = object.___devtools_id;
    wrapper.protoDepth_ = -1;
    section = new WebInspector.SidebarObjectPropertiesSection(wrapper, null);
  }
  elem.appendChild(section.element);
};
