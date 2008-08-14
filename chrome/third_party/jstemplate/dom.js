// Copyright 2005 Google
//
// Functions that handle the DOM. Partly, these merely wrap methods in
// DOM interfaces in order to allow them to be obfuscated. Partly,
// they wrap cross browser differences, and partly they provide
// functionality beyond what is available directly in the DOM.


// These constants will be condensed away by jscompiler, other than
// the corresponding properties of Node. Based on
// <http://www.w3.org/TR/2000/ REC-DOM-Level-2-Core-20001113/
// core.html#ID-1950641247>.
var DOM_ELEMENT_NODE = 1;
var DOM_ATTRIBUTE_NODE = 2;
var DOM_TEXT_NODE = 3;
var DOM_CDATA_SECTION_NODE = 4;
var DOM_ENTITY_REFERENCE_NODE = 5;
var DOM_ENTITY_NODE = 6;
var DOM_PROCESSING_INSTRUCTION_NODE = 7;
var DOM_COMMENT_NODE = 8;
var DOM_DOCUMENT_NODE = 9;
var DOM_DOCUMENT_TYPE_NODE = 10;
var DOM_DOCUMENT_FRAGMENT_NODE = 11;
var DOM_NOTATION_NODE = 12;

/**
 * Traverses the element nodes in the DOM tree underneath the given
 * node and finds the first node with elemId, or null if there is no such
 * element.  Traversal is in depth-first order.
 *
 * NOTE: The reason this is not combined with the elem() function is
 * that the implementations are different.
 * elem() is a wrapper for the built-in document.getElementById() function,
 * whereas this function performs the traversal itself.
 * Modifying elem() to take an optional root node is a possibility,
 * but the in-built function would perform better than using our own traversal.
 *
 * @param {Element} node Root element of subtree to traverse.
 * @param {String} elemId The id of the element to search for.
 * @return {Element|Null} The corresponding element, or null if not found.
 */
function nodeGetElementById(node, elemId) {
  for (var c = node.firstChild; c; c = c.nextSibling) {
    if (c.id == elemId) {
      return c;
    }
    if (c.nodeType == DOM_ELEMENT_NODE) {
      var n = arguments.callee.call(this, c, elemId);
      if (n) {
        return n;
      }
    }
  }
  return null;
}

// These wrapper functions make the underlying Node methods condense
// better: the wrapper function can be condensed by the compiler,
// while the method cannot.

/**
 * Get an attribute from the DOM.  Simple redirect, exists to compress code.
 *
 * @param {Element} node  Element to interrogate.
 * @param {String} name  Name of parameter to extract.
 * @return {String}  Resulting attribute.
 */
function domGetAttribute(node, name) {
  return node.getAttribute(name);
  // NOTE: Neither in IE nor in Firefox, HTML DOM attributes
  // implement namespaces. All items in the attribute collection have
  // null localName and namespaceURI attribute values. In IE, we even
  // encounter DIV elements that don't implement the method
  // getAttributeNS().
}

/**
 * Set an attribute in the DOM.  Simple redirect to compress code.
 *
 * @param {Element} node  Element to interrogate.
 * @param {String} name  Name of parameter to set.
 * @param {String} value  Set attribute to this value.
 */
function domSetAttribute(node, name, value) {
  node.setAttribute(name, value);
}

/**
 * Remove an attribute from the DOM.  Simple redirect to compress code.
 *
 * @param {Element} node  Element to interrogate.
 * @param {String} name  Name of parameter to remove.
 */
function domRemoveAttribute(node, name) {
  node.removeAttribute(name);
}

/**
 * Clone a node in the DOM.
 *
 * @param {Node} node  Node to clone.
 * @return {Node}  Cloned node.
 */
function domCloneNode(node) {
  return node.cloneNode(true);
  // NOTE: we never so far wanted to use cloneNode(false),
  // hence the default.
}


/**
 * Return a safe string for the className of a node.
 * If className is not a string, returns "".
 *
 * @param {Element} node  DOM element to query.
 * @return {String}
 */
function domClassName(node) {
  return node.className ? "" + node.className : "";
}

/**
 * Adds a class name to the class attribute of the given node.
 *
 * @param {Element} node  DOM element to modify.
 * @param {String} className  Class name to add.
 */
function domAddClass(node, className) {
  var name = domClassName(node);
  if (name) {
    var cn = name.split(/\s+/);
    var found = false;
    for (var i = 0; i < jsLength(cn); ++i) {
      if (cn[i] == className) {
        found = true;
        break;
      }
    }

    if (!found) {
      cn.push(className);
    }

    node.className = cn.join(' ');
  } else {
    node.className = className;
  }
}

/**
 * Removes a class name from the class attribute of the given node.
 *
 * @param {Element} node  DOM element to modify.
 * @param {String} className  Class name to remove.
 */
function domRemoveClass(node, className) {
  // Don't touch the class name if we won't find anything to change
  // anyway.
  var c = domClassName(node);
  if (!c || c.indexOf(className) == -1) {
    return;
  }
  var cn = c.split(/\s+/);
  for (var i = 0; i < jsLength(cn); ++i) {
    if (cn[i] == className) {
      cn.splice(i--, 1);
    }
  }
  node.className = cn.join(' ');
}

/**
 * Checks if a node belongs to a style class.
 *
 * @param {Element} node  DOM element to test.
 * @param {String} className  Class name to check for.
 * @return {Boolean}  Node belongs to style class.
 */
function domTestClass(node, className) {
  var cn = domClassName(node).split(/\s+/);
  for (var i = 0; i < jsLength(cn); ++i) {
    if (cn[i] == className) {
      return true;
    }
  }
  return false;
}

/**
 * Inserts a new child before a given sibling.
 *
 * @param {Node} newChild  Node to insert.
 * @param {Node} oldChild  Sibling node.
 * @return {Node}  Reference to new child.
 */
function domInsertBefore(newChild, oldChild) {
  return oldChild.parentNode.insertBefore(newChild, oldChild);
}

/**
 * Appends a new child to the specified (parent) node.
 *
 * @param {Element} node  Parent element.
 * @param {Node} child  Child node to append.
 * @return {Node}  Newly appended node.
 */
function domAppendChild(node, child) {
  return node.appendChild(child);
}

/**
 * Remove a new child from the specified (parent) node.
 *
 * @param {Element} node  Parent element.
 * @param {Node} child  Child node to remove.
 * @return {Node}  Removed node.
 */
function domRemoveChild(node, child) {
  return node.removeChild(child);
}

/**
 * Replaces an old child node with a new child node.
 *
 * @param {Node} newChild  New child to append.
 * @param {Node} oldChild  Old child to remove.
 * @return {Node}  Replaced node.
 */
function domReplaceChild(newChild, oldChild) {
  return oldChild.parentNode.replaceChild(newChild, oldChild);
}

/**
 * Removes a node from the DOM.
 *
 * @param {Node} node  The node to remove.
 * @return {Node}  The removed node.
 */
function domRemoveNode(node) {
  return domRemoveChild(node.parentNode, node);
}

/**
 * Creates a new text node in the given document.
 *
 * @param {Document} doc  Target document.
 * @param {String} text  Text composing new text node.
 * @return {Text}  Newly constructed text node.
 */
function domCreateTextNode(doc, text) {
  return doc.createTextNode(text);
}

/**
 * Creates a new node in the given document
 *
 * @param {Document} doc  Target document.
 * @param {String} name  Name of new element (i.e. the tag name)..
 * @return {Element}  Newly constructed element.
 */
function domCreateElement(doc, name) {
  return doc.createElement(name);
}

/**
 * Creates a new attribute in the given document.
 *
 * @param {Document} doc  Target document.
 * @param {String} name  Name of new attribute.
 * @return {Attr}  Newly constructed attribute.
 */
function domCreateAttribute(doc, name) {
  return doc.createAttribute(name);
}

/**
 * Creates a new comment in the given document.
 *
 * @param {Document} doc  Target document.
 * @param {String} text  Comment text.
 * @return {Comment}  Newly constructed comment.
 */
function domCreateComment(doc, text) {
  return doc.createComment(text);
}

/**
 * Creates a document fragment.
 *
 * @param {Document} doc  Target document.
 * @return {DocumentFragment}  Resulting document fragment node.
 */
function domCreateDocumentFragment(doc) {
  return doc.createDocumentFragment();
}

/**
 * Redirect to document.getElementById
 *
 * @param {Document} doc  Target document.
 * @param {String} id  Id of requested node.
 * @return {Element|Null}  Resulting element.
 */
function domGetElementById(doc, id) {
  return doc.getElementById(id);
}

/**
 * Redirect to window.setInterval
 *
 * @param {Window} win  Target window.
 * @param {Function} fun  Callback function.
 * @param {Number} time  Time in milliseconds.
 * @return {Object}  Contract id.
 */
function windowSetInterval(win, fun, time) {
  return win.setInterval(fun, time);
}

/**
 * Redirect to window.clearInterval
 *
 * @param {Window} win  Target window.
 * @param {object} id  Contract id.
 * @return {any}  NOTE: Return type unknown?
 */
function windowClearInterval(win, id) {
  return win.clearInterval(id);
}

/**
 * Determines whether one node is recursively contained in another.
 * @param parent The parent node.
 * @param child The node to look for in parent.
 * @return parent recursively contains child
 */
function containsNode(parent, child) {
  while (parent != child && child.parentNode) {
    child = child.parentNode;
  }
  return parent == child;
};
