// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Dom and DomNode are used to represent remote DOM in the
 * web inspector.
 */
goog.provide('devtools.DomAgent');
goog.provide('devtools.DomDocument');
goog.provide('devtools.DomNode');

goog.require('devtools.Callback');


/**
 * Defines indexes for the node payload properties.
 */
devtools.PayloadIndex = {
  ID : 0,
  TYPE : 1,
  NAME : 2,
  VALUE : 3,
  ATTRS : 4,
  HAS_CHILDREN : 5,
  CHILD_NODES : 6
};


/**
 * Creates document node in a given document based on a given payload data.
 * @param {devtools.Doc} doc Document to create node in.
 * @param {Array.<Object>} payload Data to build node based upon.
 * @constructor
 */
devtools.DomNode = function(doc, payload) {
  this.ownerDocument = doc;

  this.id_ = payload[devtools.PayloadIndex.ID];
  this.nodeType = payload[devtools.PayloadIndex.TYPE];
  this.nodeName = payload[devtools.PayloadIndex.NAME];
  this.nodeValue_ = payload[devtools.PayloadIndex.VALUE];
  this.textContent = this.nodeValue;

  this.attributes = [];
  this.attributesMap_ = {};
  if (payload.length > devtools.PayloadIndex.ATTRS) {
    this.setAttributesPayload_(payload[devtools.PayloadIndex.ATTRS]);
  }

  this.childNodesCount_ = payload[devtools.PayloadIndex.HAS_CHILDREN];
  this.children = null;

  this.nextSibling = null;
  this.prevSibling = null;
  this.firstChild = null;
  this.parentNode = null;

  this.disabledStyleProperties_ = {};

  if (payload.length > devtools.PayloadIndex.CHILD_NODES) {
    // Has children payloads
    this.setChildrenPayload_(
        payload[devtools.PayloadIndex.CHILD_NODES]);
  }

  this.computedStyle_ = null;
  this.style = null;
  this.matchedCSSRules_ = [];
};


/**
 * Overrides for getters and setters.
 */
devtools.DomNode.prototype = {
  get nodeValue() {
    return this.nodeValue_;
  },

  set nodeValue(value) {
    if (this.nodeType != Node.TEXT_NODE) {
      return;
    }
    var self = this;
    this.ownerDocument.domAgent_.setTextNodeValueAsync(this, value,
        function() {
          self.nodeValue_ = value;
          self.textContent = value;
        });
  }
};


/**
 * Sets attributes for a given node based on a given attrs payload.
 * @param {Array.<string>} attrs Attribute key-value pairs to set.
 * @private
 */
devtools.DomNode.prototype.setAttributesPayload_ = function(attrs) {
  for (var i = 0; i < attrs.length; i += 2) {
    this.addAttribute_(attrs[i], attrs[i + 1]);
  }
};


/**
 * @return True iff node has attributes.
 */
devtools.DomNode.prototype.hasAttributes = function()  {
  return this.attributes.length > 0;
};


/**
 * @return True iff node has child nodes.
 */
devtools.DomNode.prototype.hasChildNodes = function()  {
  return this.childNodesCount_ > 0;
};


/**
 * Inserts child node into this node after a given anchor.
 * @param {devtools.DomNode} prev Node to insert child after.
 * @param {Array.<Object>} payload Child node data.
 * @private
 */
devtools.DomNode.prototype.insertChild_ = function(prev, payload) {
  var node = new devtools.DomNode(this.ownerDocument, payload);
  if (!prev) {
    // First node
    this.children = [ node ];
  } else {
    this.children.splice(this.children.indexOf(prev) + 1, 0, node);
  }
  this.renumber_();
  return node;
};


/**
 * Removes child node from this node.
 * @param {devtools.DomNode} node Node to remove.
 * @private
 */
devtools.DomNode.prototype.removeChild_ = function(node) {
  this.children.splice(this.children.indexOf(node), 1);
  node.parentNode = null;
  this.renumber_();
};


/**
 * Sets children for this node based on the given payload.
 * @param {Array.<Object>} payload Data for children.
 * @private
 */
devtools.DomNode.prototype.setChildrenPayload_ = function(payloads) {
  this.children = [];
  for (var i = 0; i < payloads.length; ++i) {
    var payload = payloads[i];
    var node = new devtools.DomNode(this.ownerDocument, payload);
    this.children.push(node);
  }
  this.renumber_();
};


/**
 * Normalizes prev/next/parent/firstChild links for this node's children.
 * @private
 */
devtools.DomNode.prototype.renumber_ = function() {
  this.childNodesCount_ = this.children.length;
  if (this.childNodesCount_ == 0) {
    this.firstChild = null;
    return;
  }
  this.firstChild = this.children[0];
  for (var i = 0; i < this.childNodesCount_; ++i) {
    var child = this.children[i]; 
    child.nextSibling = i + 1 < this.childNodesCount_ ? 
        this.children[i + 1] : null;
    child.prevSibling = i - 1 >= 0 ? this.children[i - 1] : null;
    child.parentNode = this;
  }
};


/**
 * Returns attribute value.
 * @param {string} name Attribute name to get value for.
 * @return {string} Attribute value.
 */
devtools.DomNode.prototype.getAttribute = function(name) {
  var attr = this.attributesMap_[name];
  return attr ? attr.value : undefined;
};


/**
 * Sends 'set attribute' command to the remote agent.
 * @param {string} name Attribute name to set value for.
 * @param {string} value Attribute value to set.
 */
devtools.DomNode.prototype.setAttribute = function(name, value) {
  var self = this;
  this.ownerDocument.domAgent_.setAttributeAsync(this, name, value,
      function() {
        var attr = self.attributesMap_[name];
        if (attr) {
          attr.value = value;
        } else {
          attr = self.addAttribute_(name, value);
        }
      });
};


/**
 * Creates an attribute-like object and adds it to the object.
 * @param {string} name Attribute name to set value for.
 * @param {string} value Attribute value to set.
 */
devtools.DomNode.prototype.addAttribute_ = function(name, value) {  
  var attr = {
    'name': name,
    'value': value,
    node_: this
  };
    
  this.attributesMap_[name] = attr;
  this.attributes.push(attr);
};


/**
 * Sends 'remove attribute' command to the remote agent.
 * @param {string} name Attribute name to set value for.
 */
devtools.DomNode.prototype.removeAttribute = function(name) {
  var self = this;
  this.ownerDocument.domAgent_.removeAttributeAsync(this, name, function() {
    delete self.attributesMap_[name];
    for (var i = 0;  i < self.attributes.length; ++i) {
      if (self.attributes[i].name == name) {
        self.attributes.splice(i, 1);
        break;
      }
    }
  });
};


/**
 * Makes available the following methods and properties:
 * - node.style property
 * - node.document.defaultView.getComputedStyles(node)
 * - node.document.defaultView.getMatchedCSSRules(node, ...)
 * - style attribute of node's attributes
 * @param {string} computedStyle is a cssText of result of getComputedStyle().
 * @param {string} inlineStyle is a style.cssText (defined in the STYLE
 *     attribute).
 * @param {Object} styleAttributes represents 'style' property
 *     of attributes.
 * @param {Array.<object>} matchedCSSRules represents result of the
 *     getMatchedCSSRules(node, '', authorOnly). Each elemet consists of:
 *     selector, rule.style.cssText[, rule.parentStyleSheet.href
 *     [, rule.parentStyleSheet.ownerNode.nodeName]].
 */
devtools.DomNode.prototype.setStyles = function(computedStyle, inlineStyle,
     styleAttributes, matchedCSSRules) {
  this.computedStyle_ = this.makeStyle_(computedStyle);
  this.style = this.makeStyle_(inlineStyle);

  for (var name in styleAttributes) {
    if (this.attributesMap_[name]) {
      this.attributesMap_[name].style =
          this.makeStyle_(styleAttributes[name]);
    }
  }

  this.matchedCSSRules_ = [];
  for (var i = 0; i < matchedCSSRules.length; i++) {
    var descr = matchedCSSRules[i];

    var rule = {};
    rule.selectorText = descr['selector'];
    rule.style = this.makeStyle_(descr['style']);

    if (descr['parentStyleSheet']) {
      var parentStyleMock = {};
      parentStyleMock.href = descr['parentStyleSheet']['href'];
      var nodeName = descr['parentStyleSheet']['ownerNodeName'];
      if (nodeName) {
        parentStyleMock.ownerNode = {
          'nodeName': nodeName
        };
      }
      rule.parentStyleSheet = parentStyleMock;
    }
    this.matchedCSSRules_.push(rule);
  }
};


/**
 * Creates a style declaration.
 * @param {payload} payload
 * @return {devtools.CSSStyleDeclaration:undefined}
 * @see devtools.CSSStyleDeclaration
 */
devtools.DomNode.prototype.makeStyle_ = function(payload) {
  var style = new devtools.CSSStyleDeclaration(payload);
  style.nodeId_ = this.id_;
  return style;
};


/**
 * Remove references to the style information to release
 * resources when styles are not going to be used.
 * @see setStyles.
 */
devtools.DomNode.prototype.clearStyles = function() {
  this.computedStyle = null;
  this.style = null;
  for (var name in this.attributesMap_) {
    this.attributesMap_[name].style = null;
  }
  this.matchedCSSRules_ = null;
};


/**
 * Remote Dom document abstraction.
 * @param {devtools.DomAgent} domAgent owner agent.
 * @param {devtools.DomWindow} defaultView owner window.
 * @constructor.
 */
devtools.DomDocument = function(domAgent, defaultView) {
  devtools.DomNode.call(this, null,
    [
      0,   // id
      9,   // type = Node.DOCUMENT_NODE,
      '',  // nodeName
      '',  // nodeValue 
      [],  // attributes
      0,   // childNodeCount
    ]);
  this.listeners_ = {};
  this.domAgent_ = domAgent;
  this.defaultView = defaultView;
};
goog.inherits(devtools.DomDocument, devtools.DomNode);


/**
 * Adds event listener to the Dom.
 * @param {string} name Event name.
 * @param {function(Event):undefined} callback Listener callback.
 * @param {bool} useCapture Listener's useCapture settings.
 */
devtools.DomDocument.prototype.addEventListener =
    function(name, callback, useCapture) {
  var listeners = this.listeners_[name];
  if (!listeners) {
    listeners = [];
    this.listeners_[name] = listeners;
  }
  listeners.push(callback);
};


/**
 * Removes event listener from the Dom.
 * @param {string} name Event name.
 * @param {function(Event):undefined} callback Listener callback.
 * @param {bool} useCapture Listener's useCapture settings.
 */
devtools.DomDocument.prototype.removeEventListener =
    function(name, callback, useCapture) {
  var listeners = this.listeners_[name];
  if (!listeners) {
    return;
  }
  var index = listeners.indexOf(callback);
  if (index != -1) {
    listeners.splice(index, 1);
  }
};


/**
 * Fires Dom event to the listeners for given event type.
 * @param {string} name Event type.
 * @param {Event} event Event to fire.
 * @private
 */
devtools.DomDocument.prototype.fireDomEvent_ = function(name, event) {
  var listeners = this.listeners_[name];
  if (!listeners) {
    return;
  }
  for (var i = 0; i < listeners.length; ++i) {
    listeners[i](event);
  }
};



/**
 * Simulation of inspected DOMWindow.
 * @param {devtools.DomAgent} domAgent owner agent.
 * @constructor
 */
devtools.DomWindow = function(domAgent) {
  this.document = new devtools.DomDocument(domAgent, this);
};

/**
 * Represents DOM Node class.
 */
devtools.DomWindow.prototype.__defineGetter__('Node', function() {
  return devtools.DomNode;
});

/**
 * Represents DOM Element class.
 * @constructor
 */
devtools.DomWindow.prototype.__defineGetter__('Element', function() {
  return devtools.DomNode;
});


/**
 * See usages in ScopeChainSidebarPane.js where it's called as
 * constructor.
 */
devtools.DomWindow.prototype.Object = function() {
};


/**
 * Simulates the DOM interface for styles.
 * @param {devtools.DomNode} node
 * @return {CSSStyleDescription}
 */
devtools.DomWindow.prototype.getComputedStyle = function(node) {
  return node.computedStyle_;
};


/**
 * Simulates the DOM interface for styles.
 * @param {devtools.DomNode} nodeStyles
 * @param {string} pseudoElement assumed to be empty string.
 * @param {boolean} authorOnly assumed to be equal to authorOnly argument of
 *     getNodeStylesAsync.
 * @return {CSSStyleDescription}
 */
devtools.DomWindow.prototype.getMatchedCSSRules = function(node,
    pseudoElement, authorOnly) {
  return node.matchedCSSRules_;
};


/**
 * Creates DomAgent Js representation.
 * @constructor
 */
devtools.DomAgent = function() {
  RemoteDomAgent.DidGetChildNodes =
      devtools.Callback.processCallback;
  RemoteDomAgent.DidPerformSearch =
      devtools.Callback.processCallback;
  RemoteDomAgent.DidApplyDomChange =
      devtools.Callback.processCallback;
  RemoteDomAgent.DidRemoveAttribute =
      devtools.Callback.processCallback;
  RemoteDomAgent.DidSetTextNodeValue =
      devtools.Callback.processCallback;
  RemoteDomAgent.AttributesUpdated =
      goog.bind(this.attributesUpdated, this);
  RemoteDomAgent.SetDocumentElement =
      goog.bind(this.setDocumentElement, this);
  RemoteDomAgent.SetChildNodes =
      goog.bind(this.setChildNodes, this);
  RemoteDomAgent.HasChildrenUpdated =
      goog.bind(this.hasChildrenUpdated, this);
  RemoteDomAgent.ChildNodeInserted =
      goog.bind(this.childNodeInserted, this);
  RemoteDomAgent.ChildNodeRemoved =
      goog.bind(this.childNodeRemoved, this);

  /**
   * Top-level (and the only) document.
   * @type {devtools.DomWindow}
   * @private
   */
  this.window_ = null;

  /**
   * Id to node mapping.
   * @type {Object}
   * @private
   */
  this.idToDomNode_ = null;

  /**
   * @type {Array.<number>} Node ids for search results.
   * @private
   */
  this.searchResults_ = null;
};


/**
 * Resets dom agent to its initial state.
 */
devtools.DomAgent.prototype.reset = function() {
  this.window_ = new devtools.DomWindow(this);
  this.idToDomNode_ = { 0 : this.getDocument() };
  this.searchResults_ = [];
};


/**
 * @return {devtools.DomWindow} Window for the top level (and the only) document.
 */
devtools.DomAgent.prototype.getWindow = function() {
  return this.window_;
};


/**
 * @return {devtools.DomDocument} A document of the top level window.
 */
devtools.DomAgent.prototype.getDocument = function() {
  return this.window_.document;
};


/**
 * Requests that the document element is sent from the agent.
 */
devtools.DomAgent.prototype.getDocumentElementAsync = function() {
  if (this.getDocument().documentElement) {
    return;
  }
  RemoteDomAgent.GetDocumentElement();
};


/**
 * Asynchronously fetches children from the element with given id.
 * @param {devtools.DomNode} parent Element to get children for.
 * @param {function(devtools.DomNode):undefined} opt_callback Callback with
 *     the result.
 */
devtools.DomAgent.prototype.getChildNodesAsync = function(parent,
    opt_callback) {
  var children = parent.children;
  if (children && opt_callback) {
    opt_callback(children);
    return;
  }
  var mycallback = function() {
    if (opt_callback) {
      opt_callback(parent.children);
    }
  };
  var callId = devtools.Callback.wrap(mycallback);
  RemoteDomAgent.GetChildNodes(callId, parent.id_);
};


/**
 * Sends 'set attribute' command to the remote agent.
 * @param {devtools.DomNode} node Node to change.
 * @param {string} name Attribute name to set value for.
 * @param {string} value Attribute value to set.
 * @param {function():undefined} opt_callback Callback on success.
 */
devtools.DomAgent.prototype.setAttributeAsync = function(node, name, value,
    callback) {
  var mycallback = goog.bind(this.didApplyDomChange_, this, node, callback);
  RemoteDomAgent.SetAttribute(devtools.Callback.wrap(mycallback),
      node.id_, name, value);
};


/**
 * Sends 'remove attribute' command to the remote agent.
 * @param {devtools.DomNode} node Node to change.
 * @param {string} name Attribute name to set value for.
 * @param {function():undefined} opt_callback Callback on success.
 */
devtools.DomAgent.prototype.removeAttributeAsync = function(node, name,
    callback) {
  var mycallback = goog.bind(this.didApplyDomChange_, this, node, callback);
  RemoteDomAgent.RemoveAttribute(devtools.Callback.wrap(mycallback),
      node.id_, name);
};


/**
 * Sends 'set text node value' command to the remote agent.
 * @param {devtools.DomNode} node Node to change.
 * @param {string} text Text value to set.
 * @param {function():undefined} opt_callback Callback on success.
 */
devtools.DomAgent.prototype.setTextNodeValueAsync = function(node, text,
    callback) {
  var mycallback = goog.bind(this.didApplyDomChange_, this, node, callback);
  RemoteDomAgent.SetTextNodeValue(devtools.Callback.wrap(mycallback),
      node.id_, text);
};


/**
 * Universal callback wrapper for edit dom operations.
 * @param {devtools.DomNode} node Node to apply local changes on.
 * @param {Function} callback Post-operation call.
 * @param {boolean} success True iff operation has completed successfully.
 */
devtools.DomAgent.prototype.didApplyDomChange_ = function(node, 
    callback, success) {
  if (!success) {
   return;
  }
  callback();
  var elem = WebInspector.panels.elements.treeOutline.findTreeElement(node);
  if (elem) {
    elem._updateTitle();
  }
};


/**
 * @see DomAgentDelegate.
 * {@inheritDoc}.
 */
devtools.DomAgent.prototype.attributesUpdated = function(nodeId, attrsArray) {
  var node = this.idToDomNode_[nodeId];
  node.setAttributesPayload_(attrsArray);
};


/**
 * Returns node for id.
 * @param {number} nodeId Id to get node for.
 * @return {devtools.DomNode} Node with given id.
 */
devtools.DomAgent.prototype.getNodeForId = function(nodeId) {
  return this.idToDomNode_[nodeId];
};


/**
 * @see DomAgentDelegate.
 * {@inheritDoc}.
 */
devtools.DomAgent.prototype.setDocumentElement = function(payload) {
  var doc = this.getDocument();
  if (doc.documentElement) {
    this.reset();
    doc = this.getDocument();
  }
  this.setChildNodes(0, [payload]);
  doc.documentElement = doc.firstChild;
  doc.documentElement.ownerDocument = doc;
  WebInspector.panels.elements.reset();
};


/**
 * @see DomAgentDelegate.
 * {@inheritDoc}.
 */
devtools.DomAgent.prototype.setChildNodes = function(parentId, payloads) {
  var parent = this.idToDomNode_[parentId];
  if (parent.children) {
    return;
  }
  parent.setChildrenPayload_(payloads);
  this.bindNodes_(parent.children);
};


/**
 * Binds nodes to ids recursively.
 * @param {Array.<devtools.DomNode>} children Nodes to bind.
 */
devtools.DomAgent.prototype.bindNodes_ = function(children) {
  for (var i = 0; i < children.length; ++i) {
    var child = children[i];
    this.idToDomNode_[child.id_] = child;
    if (child.children) {
      this.bindNodes_(child.children);
    }
  }
};


/**
 * @see DomAgentDelegate.
 * {@inheritDoc}.
 */
devtools.DomAgent.prototype.hasChildrenUpdated = function(nodeId, newValue) {
  var node = this.idToDomNode_[nodeId];
  var outline = WebInspector.panels.elements.treeOutline;
  var treeElement = outline.findTreeElement(node);
  if (treeElement) {
    treeElement.hasChildren = newValue;
    treeElement.whitespaceIgnored = Preferences.ignoreWhitespace;
  }
};


/**
 * @see DomAgentDelegate.
 * {@inheritDoc}.
 */
devtools.DomAgent.prototype.childNodeInserted = function(
    parentId, prevId, payload) {
  var parent = this.idToDomNode_[parentId];
  var prev = this.idToDomNode_[prevId];
  var node = parent.insertChild_(prev, payload);
  this.idToDomNode_[node.id_] = node;
  var event = { target : node, relatedNode : parent };
  this.getDocument().fireDomEvent_('DOMNodeInserted', event);
};


/**
 * @see DomAgentDelegate.
 * {@inheritDoc}.
 */
devtools.DomAgent.prototype.childNodeRemoved = function(
    parentId, nodeId) {
  var parent = this.idToDomNode_[parentId];
  var node = this.idToDomNode_[nodeId];
  parent.removeChild_(node);
  var event = { target : node, relatedNode : parent };
  this.getDocument().fireDomEvent_('DOMNodeRemoved', event);
  delete this.idToDomNode_[nodeId];
};


/**
 * @see DomAgentDelegate.
 * {@inheritDoc}.
 */
devtools.DomAgent.prototype.performSearch = function(query, callback) {
  this.searchResults_ = [];
  RemoteDomAgent.PerformSearch(
      devtools.Callback.wrap(
          goog.bind(this.performSearchCallback_, this, callback,
                    this.searchResults_)),
      query);
};


/**
 * Invokes callback for nodes that needs to clear highlighting.
 * @param {function(Array.<devtools.DomNode>)} callback to accept the result.
 */
devtools.DomAgent.prototype.searchCanceled = function(callback) {
  if (!this.searchResults_)
    return;

  var nodes = [];
  for (var i = 0; i < this.searchResults_.length; ++i) {
    var nodeId = this.searchResults_[i];
    var node = this.idToDomNode_[nodeId];
    nodes.push(node);
  }

  callback(nodes);
  this.searchResults_ = null;
};


/**
 * Invokes callback for each node that needs to gain highlighting.
 * @param {function(Array.<devtools.DomNode>)} callback to accept the result.
 * @param {Array.<number>} searchResults to be populated.
 * @param {Array.<number>} nodeIds Ids to highlight.
 */
devtools.DomAgent.prototype.performSearchCallback_ = function(callback,
    searchResults, nodeIds) {

  if (this.searchResults_ !== searchResults)
    return; // another search has requested and this results are obsolete
  
  var nodes = [];

  for (var i = 0; i < nodeIds.length; ++i) {
    var node = this.idToDomNode_[nodeIds[i]];
    searchResults.push(nodeIds[i]);
    nodes.push(node);
  }

  callback(nodes);
};


/**
 * Returns a node by index from the actual search results
 * (last performSearch).
 * @param {number} index in the results.
 * @return {devtools.DomNode}
 */
devtools.DomAgent.prototype.getSearchResultNode = function(index) {
  return this.idToDomNode_[this.searchResults_[index]];
};


/**
 * Returns all properties of the given node.
 * @param {number} nodeId Node to get properties for.
 * @param {Array.<string>} path Path to the object.
 * @param {number} protoDepth Depth to the exact proto level.
 * @param {function(string):undefined} callback Function to call with the
 *     result.
 */
devtools.DomAgent.prototype.getNodePropertiesAsync = function(nodeId,
    path, protoDepth, callback) {
  var callbackId = this.utilityFunctionCallbackWrapper_(callback);
  RemoteToolsAgent.ExecuteUtilityFunction(callbackId,
      'getProperties', nodeId,
      JSON.stringify([path, protoDepth]));
};


/**
 * Returns prototype chain for a given node.
 * @param {number} nodeId Node to get prototypes for.
 * @param {Function} callback.
 */
devtools.DomAgent.prototype.getNodePrototypesAsync = function(nodeId,
    callback) {
  var callbackId = this.utilityFunctionCallbackWrapper_(callback);
  RemoteToolsAgent.ExecuteUtilityFunction(callbackId,
      'getPrototypes', nodeId, '[]');
};


/**
 * Returns styles for given node.
 * @param {devtools.DomNode} node Node to get prototypes for.
 * @param {boolean} authorOnly Returns only author styles if true.
 * @param {Function} callback.
 */
devtools.DomAgent.prototype.getNodeStylesAsync = function(node,
    authorOnly, callback) {
  var callbackId = this.utilityFunctionCallbackWrapper_(callback);
  RemoteToolsAgent.ExecuteUtilityFunction(callbackId,
      'getStyles',
      node.id_,
      JSON.stringify([authorOnly]));
};


/**
 * Toggles style with given id on/off.
 * @param {devtools.CSSStyleDeclaration} style Style to toggle.
 * @param {boolean} enabled True if style should be enabled.
 * @param {string} name Style name.
 * @param {Function} callback.
 */
devtools.DomAgent.prototype.toggleNodeStyleAsync = function(
    style, enabled, name, callback) {
  var callbackId = this.utilityFunctionCallbackWrapper_(callback);
  RemoteToolsAgent.ExecuteUtilityFunction(callbackId,
      'toggleNodeStyle',
      style.nodeId_,
      JSON.stringify([style.id_, enabled, name]));
};


/**
 * Applies new text to a style.
 * @param {devtools.CSSStyleDeclaration} style Style to edit.
 * @param {string} name Property name to edit.
 * @param {string} styleText Text to set the style from.
 * @param {Function} callback.
 */
devtools.DomAgent.prototype.applyStyleTextAsync = function(
    style, name, styleText, callback) {
  var callbackId = this.utilityFunctionCallbackWrapper_(callback);
  RemoteToolsAgent.ExecuteUtilityFunction(
      callbackId,
      'applyStyleText',
      style.nodeId_,
      JSON.stringify([style.id_, name, styleText]));
};


/**
 * Sets style property with given name to a value.
 * @param {devtools.DomNode} node Node to edit style for.
 * @param {string} name Property name to edit.
 * @param {string} value New value.
 * @param {Function} callback.
 */
devtools.DomAgent.prototype.setStylePropertyAsync = function(
    node, name, value, callback) {
  var callbackId = this.utilityFunctionCallbackWrapper_(callback);
  RemoteToolsAgent.ExecuteUtilityFunction(
      callbackId,
      'setStyleProperty',
      node.id_,
      JSON.stringify([name, value]));
};


/**
 * Dumps exception if something went wrong in ExecuteUtilityFunction.
 * @param {Function} callback Callback to wrap.
 * @return {number} Callback id.
 */
devtools.DomAgent.prototype.utilityFunctionCallbackWrapper_ =
    function(callback) {
  var mycallback = function(result, exception) {
    if (exception && exception.length) {
      debugPrint('Exception in ExecuteUtilityFunction styles:' + exception);
      return;
    }
    callback(result);
  };
  return devtools.Callback.wrap(mycallback);
};


/**
 * Represents remote CSSStyleDeclaration for using in StyleSidebarPane.
 * @param {id, Array<Object>} payload built by inject's getStyle from the
 *     injected js.
 * @constructor
 */
devtools.CSSStyleDeclaration = function(payload) {
  this.id_ = payload[0];
  this.width = payload[1];
  this.height = payload[2];
  this.__disabledProperties = payload[3];
  this.__disabledPropertyValues = payload[4];
  this.__disabledPropertyPriorities = payload[5];

  this.length = payload.length - 6;
  this.priority_ = {};
  this.implicit_ = {};
  this.shorthand_ = {};
  this.value_ = {};

  for (var i = 6; i < payload.length; ++i) {
    var p = payload[i];
    var name = p[0];

    this.priority_[name] = p[1];
    this.implicit_[name] = p[2];
    this.shorthand_[name] = p[3];
    this.value_[name] = p[4];

    this[i - 6] = name;
  }
};


/**
 * @param {string} name of a CSS property.
 * @return {string}
 */
devtools.CSSStyleDeclaration.prototype.getPropertyValue = function(name) {
  return this.value_[name] || '';
};


/**
 * @param {string} name of a CSS property.
 * @return {string} 'important' | ''.
 */
devtools.CSSStyleDeclaration.prototype.getPropertyPriority = function(name) {
  return this.priority_[name] || '';
};


/**
 * @param {string} name of a CSS property.
 * @return {string} shorthand name  or ''
 */
devtools.CSSStyleDeclaration.prototype.getPropertyShorthand = function(name) {
  return this.shorthand_[name] || '';
};


/**
 * @param {string} name of a CSS property.
 * @return {boolean}
 */
devtools.CSSStyleDeclaration.prototype.isPropertyImplicit = function(name) {
  return !!this.implicit_[name];
};


function firstChildSkippingWhitespace() {
  return this.firstChild;
}


function onlyTextChild() {
  if (!this.children) {
    return null;
  } else if (this.children.length == 1 &&
        this.children[0].nodeType == Node.TEXT_NODE) {
    return this.children[0];
  } else {
    return null;
  }
}
