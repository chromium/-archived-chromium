// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Dom and DomNode are used to represent remote DOM in the
 * web inspector.
 */
goog.provide('devtools.DomDocument');
goog.provide('devtools.DomNode');


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

  this.id = payload[devtools.PayloadIndex.ID];
  this.nodeType = payload[devtools.PayloadIndex.TYPE];
  this.nodeName = payload[devtools.PayloadIndex.NAME];
  this.nodeValue = payload[devtools.PayloadIndex.VALUE];
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
 * Sets attributes for a given node based on a given attrs payload.
 * @param {Array.<string>} attrs Attribute key-value pairs to set.
 * @private
 */
devtools.DomNode.prototype.setAttributesPayload_ = function(attrs) {
  for (var i = 0; i < attrs.length; i += 2) {
    this.attributes.push({"name" : attrs[i], "value" : attrs[i+1]});
    this.attributesMap_[attrs[i]] = attrs[i+1];
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
 @ @return {string} Attribute value.
 */
devtools.DomNode.prototype.getAttribute = function(name) {
  return this.attributesMap_[name];
};


/**
 * Remote Dom document abstraction.
 * @constructor.
 */
devtools.DomDocument = function() {
  devtools.DomNode.call(this, null,
    [
      0,   // id
      9,   // type = Node.DOCUMENT_NODE,
      "",  // nodeName
      "",  // nodeValue 
      [],  // attributes
      0,   // childNodeCount
    ]);
  this.id_to_dom_node_ = { 0 : this };
  this.listeners_ = {};
  this.defaultView = {
    getComputedStyle : function() {},
    getMatchedCSSRules : function() {}
  };
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
 * Sets root document element.
 * @param {Array.<Object>} payload Document element data.
 */
devtools.DomDocument.prototype.setDocumentElement = function(payload) {
  this.setChildren(0, [payload]);
  this.documentElement = this.firstChild;
  this.fireDomEvent_("DOMContentLoaded");
};


/**
 * Sets element's children.
 * @param {string} id Parent id.
 * @param {Array.<Object>} payloads Array with elements' data.
 */
devtools.DomDocument.prototype.setChildren = function(id, payloads) {
  var parent = this.id_to_dom_node_[id];
  parent.setChildrenPayload_(payloads);
  var children = parent.children;
  for (var i = 0; i < children.length; ++i) {
    this.id_to_dom_node_[children[i].id] = children[i];
  }
};


/**
 * Inserts node into the given parent after the given anchor.
 * @param {string} parentId Parent id.
 * @param {string} prevId Node to insert given node after.
 * @param {Array.<Object>} payload Node data.
 */
devtools.DomDocument.prototype.nodeInserted = function(
    parentId, prevId, payload) {
  var parent = this.id_to_dom_node_[parentId];
  var prev = this.id_to_dom_node_[prevId];
  var node = parent.insertChild_(prev, payload);
  this.id_to_dom_node_[node.id] = node;
  var event = { target : node, relatedNode : parent };
  this.fireDomEvent_("DOMNodeInserted", event);
};


/**
 * Removes node from the given parent.
 * @param {string} parentId Parent id.
 * @param {string} nodeId Id of the node to remove.
 */
devtools.DomDocument.prototype.nodeRemoved = function(
    parentId, nodeId) {
  var parent = this.id_to_dom_node_[parentId];
  var node = this.id_to_dom_node_[nodeId];
  parent.removeChild_(node);
  var event = { target : node, relatedNode : parent };
  this.fireDomEvent_("DOMNodeRemoved", event);
  delete this.id_to_dom_node_[nodeId];
};


/**
 * Sets attributes to an element with given id.
 * @param {string} nodeId Id of the element to set attributes for.
 * @param {Array.<Object>} attrsArray Flat attributes data.
 */
devtools.DomDocument.prototype.setAttributes = function(
    nodeId, attrsArray) {
  var node = this.id_to_dom_node_[nodeId];
  node.setAttributesPayload_(attrsArray);
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
