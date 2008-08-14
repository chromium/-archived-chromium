// Copyright 2005-2006 Google Inc. All Rights Reserved.
/**
 * @fileoverview This file contains miscellaneous basic functionality.
 *
 */

/**
 * Creates a DOM element with the given tag name in the document of the
 * owner element.
 *
 * @param {String} tagName  The name of the tag to create.
 * @param {Element} owner The intended owner (i.e., parent element) of
 * the created element.
 * @param {Point} opt_position  The top-left corner of the created element.
 * @param {Size} opt_size  The size of the created element.
 * @param {Boolean} opt_noAppend Do not append the new element to the owner.
 * @return {Element}  The newly created element node.
 */
function createElement(tagName, owner, opt_position, opt_size, opt_noAppend) {
  var element = ownerDocument(owner).createElement(tagName);
  // NOTE: Firefox/1.5 makes elements visible as soon as they
  // are inserted in the DOM. Hence, position must be set before
  // appending the element to its parent in order to avoid it showing
  // up transiently at the origin before placed at the right
  // position. NOTE: I don't think that this triggers the
  // reverse DOM insertion memory leak in IE.
  if (opt_position) {
    setPosition(element, opt_position);
  }
  if (opt_size) {
    setSize(element, opt_size);
  }
  if (owner && !opt_noAppend) {
    appendChild(owner, element);
  }

  return element;
}

/**
 * Creates a text node with the given value.
 *
 * @param {String} value  The text to place in the new node.
 * @param {Element} owner The owner (i.e., parent element) of the new
 * text node.
 * @return {Text}  The newly created text node.
 */
function createTextNode(value, owner) {
  var element = ownerDocument(owner).createTextNode(value);
  if (owner) {
    appendChild(owner, element);
  }
  return element;
}

/**
 * Returns the document owner of the given element. In particular,
 * returns window.document if node is null or the browser does not
 * support ownerDocument.
 *
 * @param {Node} node  The node whose ownerDocument is required.
 * @returns {Document|Null}  The owner document or null if unsupported.
 */
function ownerDocument(node) {
  return (node ? node.ownerDocument : null) || document;
}

/**
 * Wrapper function to create CSS units (pixels) string
 *
 * @param {Number} numPixels  Number of pixels, may be floating point.
 * @returns {String}  Corresponding CSS units string.
 */
function px(numPixels) {
  return round(numPixels) + "px";
}

/**
 * Sets the left and top of the given element to the given point.
 *
 * @param {Element} element  The dom element to manipulate.
 * @param {Point} point  The desired position.
 */
function setPosition(element, point) {
  var style = element.style;
  style.position = "absolute";
  style.left = px(point.x);
  style.top = px(point.y);
}

/**
 * Sets the width and height style attributes to the given size.
 *
 * @param {Element} element  The dom element to manipulate.
 * @param {Size} size  The desired size.
 */
function setSize(element, size) {
  var style = element.style;
  style.width = px(size.width);
  style.height = px(size.height);
}

/**
 * Sets display to none. Doing this as a function saves a few bytes for
 * the 'style.display' property and the 'none' literal.
 *
 * @param {Element} node  The dom element to manipulate.
 */
function displayNone(node) {
  node.style.display = 'none';
}

/**
 * Sets display to default.
 *
 * @param {Element} node  The dom element to manipulate.
 */
function displayDefault(node) {
  node.style.display = '';
}

/**
 * Appends the given child to the given parent in the DOM
 *
 * @param {Element} parent  The parent dom element.
 * @param {Node} child  The new child dom node.
 */
function appendChild(parent, child) {
  parent.appendChild(child);
}


/**
 * Wrapper for the eval() builtin function to evaluate expressions and
 * obtain their value. It wraps the expression in parentheses such
 * that object literals are really evaluated to objects. Without the
 * wrapping, they are evaluated as block, and create syntax
 * errors. Also protects against other syntax errors in the eval()ed
 * code and returns null if the eval throws an exception.
 *
 * @param {String} expr
 * @return {Object|Null}
 */
function jsEval(expr) {
  try {
    // NOTE: An alternative idiom would be:
    //
    //   eval('(' + expr + ')');
    //
    // Note that using the square brackets as below, "" evals to undefined.
    // The alternative of using parentheses does not work when evaluating
    // function literals in IE.
    // e.g. eval("(function() {})") returns undefined, and not a function
    // object, in IE.
    return eval('[' + expr + '][0]');
  } catch (e) {
    return null;
  }
}


/**
 * Wrapper for the eval() builtin function to execute statements. This
 * guards against exceptions thrown, but doesn't return a
 * value. Still, mostly for testability, it returns a boolean to
 * indicate whether execution was successful. NOTE:
 * javascript's eval semantics is murky in that it confounds
 * expression evaluation and statement execution into a single
 * construct. Cf. jsEval().
 *
 * @param {String} stmt
 * @return {Boolean}
 */
function jsExec(stmt) {
  try {
    eval(stmt);
    return true;
  } catch (e) {
    return false;
  }
}


/**
 * Wrapper for eval with a context. NOTE: The style guide
 * deprecates eval, so this is the exception that proves the
 * rule. Notice also that since the value of the expression is
 * returned rather than assigned to a local variable, one major
 * objection aganist the use of the with() statement, namely that
 * properties of the with() target override local variables of the
 * same name, is void here.
 *
 * @param {String} expr
 * @param {Object} context
 * @return {Object|Null}
 */
function jsEvalWith(expr, context) {
  try {
    with (context) {
      // See comment in jsEval.
      return eval('[' + expr + '][0]');
    }
  } catch (e) {
    return null;
  }
}
