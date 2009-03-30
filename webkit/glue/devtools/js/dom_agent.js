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

  if (payload.length > devtools.PayloadIndex.CHILD_NODES) {
    // Has children payloads
    this.setChildrenPayload_(
        payload[devtools.PayloadIndex.CHILD_NODES]);
  }
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
    var attr = {"name" : attrs[i], "value" : attrs[i+1]};
    this.attributes.push(attr);
    this.attributesMap_[attrs[i]] = attr;
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
          attr = {"name" : name, "value" : value};
          self.attributesMap_[name] = attr;
          self.attributes_.push(attr);
        }
      });
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
 * Remote Dom document abstraction.
 * @param {devtools.DomAgent} domAgent owner agent.
 * @constructor.
 */
devtools.DomDocument = function(domAgent) {
  devtools.DomNode.call(this, null,
    [
      0,   // id
      9,   // type = Node.DOCUMENT_NODE,
      "",  // nodeName
      "",  // nodeValue 
      [],  // attributes
      0,   // childNodeCount
    ]);
  this.listeners_ = {};
  this.defaultView = {
    getComputedStyle : function() {},
    getMatchedCSSRules : function() {}
  };
  this.domAgent_ = domAgent;
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
   * @type {devtools.DomDocument}
   * @private
   */
  this.document_ = null;

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

  this.reset();
};


/**
 * Rests dom agent to its initial state.
 */
devtools.DomAgent.prototype.reset = function() {
  this.document_ = new devtools.DomDocument(this);
  this.idToDomNode_ = { 0 : this.document_ };
  this.searchResults_ = [];
};


/**
 * @return {devtools.DomDocument} Top level (and the only) document.
 */
devtools.DomAgent.prototype.getDocument = function() {
  return this.document_;
};


/**
 * Requests that the document element is sent from the agent.
 */
devtools.DomAgent.prototype.getDocumentElementAsync = function() {
  if (this.document_.documentElement) {
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
  if (this.document_.documentElement) {
    return;
  }
  this.setChildNodes(0, [payload]);
  this.document_.documentElement = this.document_.firstChild;
  this.document_.documentElement.ownerDocument = this.document_;
  this.document_.fireDomEvent_("DOMContentLoaded");
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
  this.document_.fireDomEvent_("DOMNodeInserted", event);
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
  this.document_.fireDomEvent_("DOMNodeRemoved", event);
  delete this.idToDomNode_[nodeId];
};


/**
 * @see DomAgentDelegate.
 * {@inheritDoc}.
 */
devtools.DomAgent.prototype.performSearch = function(query, forEach) {
  RemoteDomAgent.PerformSearch(
      devtools.Callback.wrap(
          goog.bind(this.performSearchCallback_, this, forEach)),
      query);
};


/**
 * Invokes callback for each node that needs to clear highlighting.
 * @param {function(devtools.DomNode):undefined} forEach callback to call.
 */
devtools.DomAgent.prototype.searchCanceled = function(forEach) {
  for (var i = 0; i < this.searchResults_.length; ++i) {
    var nodeId = this.searchResults_[i];
    var node = this.idToDomNode_[nodeId];
    forEach(node);
  }
};


/**
 * Invokes callback for each node that needs to gain highlighting.
 * @param {function(devtools.DomNode):undefined} forEach callback to call.
 * @param {Array.<number>} nodeIds Ids to highlight.
 */
devtools.DomAgent.prototype.performSearchCallback_ = function(forEach,
    nodeIds) {
  this.searchResults_ = [];
  for (var i = 0; i < nodeIds.length; ++i) {
    var node = this.idToDomNode_[nodeIds[i]];
    this.searchResults_.push(nodeIds[i]);
    forEach(node);
  }
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
