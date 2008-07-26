// Copyright 2006 Google
/**
 * @pageoverview Some logic for the jstemplates test pages.
 *
 * @author Steffen Meschkat <mesch@google.com>
 */

/**
 * Returns the element with the given ID
 *
 * @param {String} id  The id of the element to return.
 * @param {Document} opt_context  The context in which to search (optional).
 * @return {Element}  The corresponding element.
 */
function elem(id, opt_context) {
  if (opt_context && ownerDocument(opt_context)) {
    return ownerDocument(opt_context).getElementById(id);
  } else {
    return document.getElementById(id);
  }
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
    // NOTE(mesch): An alternative idiom would be:
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
 * Initializer for interactive test: copies the value from the actual
 * template HTML into a text area to make the HTML source visible.
 */
function jsinit() {
  elem('template').value = elem('tc').innerHTML;
}


/**
 * Interactive test.
 *
 * @param {Boolean} reprocess If true, reprocesses the output of the
 * previous invocation.
 */
function jstest(reprocess) {
  var jstext = elem('js').value;
  var input = jsEval(jstext);
  var t;
  if (reprocess) {
    t = elem('out');
  } else {
    elem('tc').innerHTML = elem('template').value;
    t = jstGetTemplate('t');
    elem('out').innerHTML = '';
    elem('out').appendChild(t);
  }
  jstProcess(new JsExprContext(input), t);
  elem('html').value = elem('out').innerHTML;
}
