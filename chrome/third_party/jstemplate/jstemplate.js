// Copyright 2006 Google Inc. All rights reserved.
/**
 * @fileoverview A simple formatter to project JavaScript data into
 * HTML templates. The template is edited in place. I.e. in order to
 * instantiate a template, clone it from the DOM first, and then
 * process the cloned template. This allows for updating of templates:
 * If the templates is processed again, changed values are merely
 * updated.
 *
 * NOTE: IE DOM doesn't have importNode().
 *
 * NOTE: The property name "length" must not be used in input
 * data, see comment in jstSelect_().
 */


/**
 * Names of jstemplate attributes. These attributes are attached to
 * normal HTML elements and bind expression context data to the HTML
 * fragment that is used as template.
 */
var ATT_select = 'jsselect';
var ATT_instance = 'jsinstance';
var ATT_display = 'jsdisplay';
var ATT_values = 'jsvalues';
var ATT_eval = 'jseval';
var ATT_transclude = 'transclude';
var ATT_content = 'jscontent';


/**
 * Names of special variables defined by the jstemplate evaluation
 * context. These can be used in js expression in jstemplate
 * attributes.
 */
var VAR_index = '$index';
var VAR_this = '$this';


/**
 * Context for processing a jstemplate. The context contains a context
 * object, whose properties can be referred to in jstemplate
 * expressions, and it holds the locally defined variables.
 *
 * @param {Object} opt_data The context object. Null if no context.
 *
 * @param {Object} opt_parent The parent context, from which local
 * variables are inherited. Normally the context object of the parent
 * context is the object whose property the parent object is. Null for the
 * context of the root object.
 *
 * @constructor
 */
function JsExprContext(opt_data, opt_parent) {
  var me = this;

  /**
   * The local context of the input data in which the jstemplate
   * expressions are evaluated. Notice that this is usually an Object,
   * but it can also be a scalar value (and then still the expression
   * $this can be used to refer to it). Notice this can be a scalar
   * value, including undefined.
   *
   * @type {Object}
   */
  me.data_ = opt_data;

  /**
   * The context for variable definitions in which the jstemplate
   * expressions are evaluated. Other than for the local context,
   * which replaces the parent context, variable definitions of the
   * parent are inherited. The special variable $this points to data_.
   *
   * @type {Object}
   */
  me.vars_ = {};
  if (opt_parent) {
    // If there is a parent node, inherit local variables from the
    // parent.
    copyProperties(me.vars_, opt_parent.vars_);
  }
  // Add the current context object as a special variable $this
  // so it is possible to use it in expressions.
  this.vars_[VAR_this] = me.data_;
}


/**
 * Evaluates the given expression in the context of the current
 * context object and the current local variables.
 *
 * @param {String} expr A javascript expression.
 *
 * @param {Element} template DOM node of the template.
 *
 * @return The value of that expression.
 */
JsExprContext.prototype.jseval = function(expr, template) {
  with (this.vars_) {
    with (this.data_) {
      try {
        // NOTE: Since eval is a built-in function, calling
        // eval.call(template, ...) does not set 'this' to template.
        return (function() {
          return eval('[' + expr + '][0]');
        }).call(template);
      } catch (e) {
        return null;
      }
    }
  }
}


/**
 * Clones the current context for a new context object. The cloned
 * context has the data object as its context object and the current
 * context as its parent context. It also sets the $index variable to
 * the given value. This value usually is the position of the data
 * object in a list for which a template is instantiated multiply.
 *
 * @param {Object} data The new context object.
 *
 * @param {Number} index Position of the new context when multiply
 * instantiated. (See implementation of jstSelect().)
 *
 * @return {JsExprContext}
 */
JsExprContext.prototype.clone = function(data, index) {
  var ret = new JsExprContext(data, this);
  ret.setVariable(VAR_index, index);
  if (this.resolver_) {
    ret.setSubTemplateResolver(this.resolver_);
  }
  return ret;
}


/**
 * Binds a local variable to the given value. If set from jstemplate
 * jsvalue expressions, variable names must start with $, but in the
 * API they only have to be valid javascript identifier.
 *
 * @param {String} name
 *
 * @param {Object} value
 */
JsExprContext.prototype.setVariable = function(name, value) {
  this.vars_[name] = value;
}


/**
 * Sets the function used to resolve the values of the transclude
 * attribute into DOM nodes. By default, this is jstGetTemplate(). The
 * value set here is inherited by clones of this context.
 *
 * @param {Function} resolver The function used to resolve transclude
 * ids into a DOM node of a subtemplate. The DOM node returned by this
 * function will be inserted into the template instance being
 * processed. Thus, the resolver function must instantiate the
 * subtemplate as necessary.
 */
JsExprContext.prototype.setSubTemplateResolver = function(resolver) {
  this.resolver_ = resolver;
}


/**
 * Resolves a sub template from an id. Used to process the transclude
 * attribute. If a resolver function was set using
 * setSubTemplateResolver(), it will be used, otherwise
 * jstGetTemplate().
 *
 * @param {String} id The id of the sub template.
 *
 * @return {Node} The root DOM node of the sub template, for direct
 * insertion into the currently processed template instance.
 */
JsExprContext.prototype.getSubTemplate = function(id) {
  return (this.resolver_ || jstGetTemplate).call(this, id);
}


/**
 * HTML template processor. Data values are bound to HTML templates
 * using the attributes transclude, jsselect, jsdisplay, jscontent,
 * jsvalues. The template is modifed in place. The values of those
 * attributes are JavaScript expressions that are evaluated in the
 * context of the data object fragment.
 *
 * @param {JsExprContext} context Context created from the input data
 * object.
 *
 * @param {Element} template DOM node of the template. This will be
 * processed in place. After processing, it will still be a valid
 * template that, if processed again with the same data, will remain
 * unchanged.
 */
function jstProcess(context, template) {
  var processor = new JstProcessor();
  processor.run_([ processor, processor.jstProcess_, context, template ]);
}


/**
 * Internal class used by jstemplates to maintain context.
 * NOTE: This is necessary to process deep templates in Safari
 * which has a relatively shallow stack.
 * @class
 */
function JstProcessor() {
}


/**
 * Runs the state machine, beginning with function "start".
 *
 * @param {Array} start The first function to run, in the form
 * [object, method, args ...]
 */
JstProcessor.prototype.run_ = function(start) {
  var me = this;

  me.queue_ = [ start ];
  while (jsLength(me.queue_)) {
    var f = me.queue_.shift();
    f[1].apply(f[0], f.slice(2));
  }
}


/**
 * Appends a function to be called later.
 * Analogous to calling that function on a subsequent line, or a subsequent
 * iteration of a loop.
 *
 * @param {Array} f  A function in the form [object, method, args ...]
 */
JstProcessor.prototype.enqueue_ = function(f) {
  this.queue_.push(f);
}


/**
 * Implements internals of jstProcess.
 *
 * @param {JsExprContext} context
 *
 * @param {Element} template
 */
JstProcessor.prototype.jstProcess_ = function(context, template) {
  var me = this;

  var transclude = domGetAttribute(template, ATT_transclude);
  if (transclude) {
    var tr = context.getSubTemplate(transclude);
    if (tr) {
      domReplaceChild(tr, template);
      me.enqueue_([ me, me.jstProcess_, context, tr ]);
    } else {
      domRemoveNode(template);
    }
    return;
  }

  var select = domGetAttribute(template, ATT_select);
  if (select) {
    me.jstSelect_(context, template, select);
    return;
  }

  var display = domGetAttribute(template, ATT_display);
  if (display) {
    if (!context.jseval(display, template)) {
      displayNone(template);
      return;
    }

    displayDefault(template);
    // domRemoveAttribute(template, ATT_display);
  }

  // NOTE: content is a separate attribute, instead of just a
  // special value mentioned in values, for two reasons: (1) it is
  // fairly common to have only mapped content, and writing
  // content="expr" is shorter than writing values="content:expr", and
  // (2) the presence of content actually terminates traversal, and we
  // need to check for that. Display is a separate attribute for a
  // reason similar to the second, in that its presence *may*
  // terminate traversal.

  var values = domGetAttribute(template, ATT_values);
  if (values) {
    // domRemoveAttribute(template, ATT_values);
    me.jstValues_(context, template, values);
  }

  // Evaluate expressions immediately. Useful for hooking callbacks
  // into jstemplates.
  //
  // NOTE: Evaluation order is sometimes significant, e.g.  in
  // map_html_addressbook_template.tpl, the expression evaluated in
  // jseval relies on the values set in jsvalues, so it needs to be
  // evaluated *after* jsvalues. TODO: This is quite arbitrary,
  // it would be better if this would have more necessity to it.
  var expressions = domGetAttribute(template, ATT_eval);
  if (expressions) {
    // See NOTE at top of jstValues.
    foreach(expressions.split(/\s*;\s*/), function(expression) {
      expression = stringTrim(expression);
      if (jsLength(expression)) {
        context.jseval(expression, template);
      }
    });
  }

  var content = domGetAttribute(template, ATT_content);
  if (content) {
    // domRemoveAttribute(template, ATT_content);
    me.jstContent_(context, template, content);

  } else {
    var childnodes = [];
    // NOTE: We enqueue the processing of each child via
    // enqueue_(), before processing any one of them.  This means
    // that any newly generated children will be ignored (as desired).
    for (var i = 0; i < jsLength(template.childNodes); ++i) {
      if (template.childNodes[i].nodeType == DOM_ELEMENT_NODE) {
      me.enqueue_(
          [ me, me.jstProcess_, context, template.childNodes[i] ]);
      }
    }
  }
}


/**
 * Implements the jsselect attribute: evalutes the value of the
 * jsselect attribute in the current context, with the current
 * variable bindings (see JsExprContext.jseval()). If the value is an
 * array, the current template node is multiplied once for every
 * element in the array, with the array element being the context
 * object. If the array is empty, or the value is undefined, then the
 * current template node is dropped. If the value is not an array,
 * then it is just made the context object.
 *
 * @param {JsExprContext} context The current evaluation context.
 *
 * @param {Element} template The currently processed node of the template.
 *
 * @param {String} select The javascript expression to evaluate.
 *
 * @param {Function} process The function to continue processing with.
 */
JstProcessor.prototype.jstSelect_ = function(context, template, select) {
  var me = this;

  var value = context.jseval(select, template);
  domRemoveAttribute(template, ATT_select);

  // Enable reprocessing: if this template is reprocessed, then only
  // fill the section instance here. Otherwise do the cardinal
  // processing of a new template.
  var instance = domGetAttribute(template, ATT_instance);
  var instance_last = false;
  if (instance) {
    if (instance.charAt(0) == '*') {
      instance = parseInt10(instance.substr(1));
      instance_last = true;
    } else {
      instance = parseInt10(instance);
    }
  }

  // NOTE: value instanceof Array is occasionally false for
  // arrays, seen in Firefox. Thus we recognize an array as an object
  // which is not null that has a length property. Notice that this
  // also matches input data with a length property, so this property
  // name should be avoided in input data.
  var multiple = (value !== null &&
                  typeof value == 'object' &&
                  typeof value.length == 'number');
  var multiple_empty = (multiple && value.length == 0);

  if (multiple) {
    if (multiple_empty) {
      // For an empty array, keep the first template instance and mark
      // it last. Remove all other template instances.
      if (!instance) {
        domSetAttribute(template, ATT_select, select);
        domSetAttribute(template, ATT_instance, '*0');
        displayNone(template);
      } else {
        domRemoveNode(template);
      }

    } else {
      displayDefault(template);
      // For a non empty array, create as many template instances as
      // are needed. If the template is first processed, as many
      // template instances are needed as there are values in the
      // array. If the template is reprocessed, new template instances
      // are only needed if there are more array values than template
      // instances. Those additional instances are created by
      // replicating the last template instance. NOTE: If the
      // template is first processed, there is no jsinstance
      // attribute. This is indicated by instance === null, except in
      // opera it is instance === "". Notice also that the === is
      // essential, because 0 == "", presumably via type coercion to
      // boolean. For completeness, in case any browser returns
      // undefined and not null for getAttribute of nonexistent
      // attributes, we also test for === undefined.
      if (instance === null || instance === "" || instance === undefined ||
          (instance_last && instance < jsLength(value) - 1)) {
        var templatenodes = [];
        var instances_start = instance || 0;
        for (var i = instances_start + 1; i < jsLength(value); ++i) {
          var node = domCloneNode(template);
          templatenodes.push(node);
          domInsertBefore(node, template);
        }
        // Push the originally present template instance last to keep
        // the order aligned with the DOM order, because the newly
        // created template instances are inserted *before* the
        // original instance.
        templatenodes.push(template);

        for (var i = 0; i < jsLength(templatenodes); ++i) {
          var ii = i + instances_start;
          var v = value[ii];
          var t = templatenodes[i];

          me.enqueue_([ me, me.jstProcess_, context.clone(v, ii), t ]);
          var instanceStr = (ii == jsLength(value) - 1 ? '*' : '') + ii;
          me.enqueue_(
              [ null, postProcessMultiple_, t, select, instanceStr ]);
        }

      } else if (instance < jsLength(value)) {
        var v = value[instance];

        me.enqueue_(
            [me, me.jstProcess_, context.clone(v, instance), template]);
        var instanceStr = (instance == jsLength(value) - 1 ? '*' : '')
                          + instance;
        me.enqueue_(
            [ null, postProcessMultiple_, template, select, instanceStr ]);
      } else {
        domRemoveNode(template);
      }
    }
  } else {
    if (value == null) {
      domSetAttribute(template, ATT_select, select);
      displayNone(template);
    } else {
      me.enqueue_(
          [ me, me.jstProcess_, context.clone(value, 0), template ]);
      me.enqueue_(
          [ null, postProcessSingle_, template, select ]);
    }
  }
}


/**
 * Sets ATT_select and ATT_instance following recursion to jstProcess.
 *
 * @param {Element} template  The template
 *
 * @param {String} select  The jsselect string
 *
 * @param {String} instanceStr  The new value for the jsinstance attribute
 */
function postProcessMultiple_(template, select, instanceStr) {
  domSetAttribute(template, ATT_select, select);
  domSetAttribute(template, ATT_instance, instanceStr);
}


/**
 * Sets ATT_select and makes the element visible following recursion to
 * jstProcess.
 *
 * @param {Element} template  The template
 *
 * @param {String} select  The jsselect string
 */
function postProcessSingle_(template, select) {
  domSetAttribute(template, ATT_select, select);
  displayDefault(template);
}


/**
 * Implements the jsvalues attribute: evaluates each of the values and
 * assigns them to variables in the current context (if the name
 * starts with '$', javascript properties of the current template node
 * (if the name starts with '.'), or DOM attributes of the current
 * template node (otherwise). Since DOM attribute values are always
 * strings, the value is coerced to string in the latter case,
 * otherwise it's the uncoerced javascript value.
 *
 * @param {JsExprContext} context Current evaluation context.
 *
 * @param {Element} template Currently processed template node.
 *
 * @param {String} valuesStr Value of the jsvalues attribute to be
 * processed.
 */
JstProcessor.prototype.jstValues_ = function(context, template, valuesStr) {
  // TODO: It is insufficient to split the values by simply
  // finding semi-colons, as the semi-colon may be part of a string constant
  // or escaped. This needs to be fixed.
  var values = valuesStr.split(/\s*;\s*/);
  for (var i = 0; i < jsLength(values); ++i) {
    var colon = values[i].indexOf(':');
    if (colon < 0) {
      continue;
    }
    var label = stringTrim(values[i].substr(0, colon));
    var value = context.jseval(values[i].substr(colon + 1), template);

    if (label.charAt(0) == '$') {
      // A jsvalues entry whose name starts with $ sets a local
      // variable.
      context.setVariable(label, value);

    } else if (label.charAt(0) == '.') {
      // A jsvalues entry whose name starts with . sets a property of
      // the current template node. The name may have further dot
      // separated components, which are translated into namespace
      // objects. This specifically allows to set properties on .style
      // using jsvalues. NOTE(mesch): Setting the style attribute has
      // no effect in IE and hence should not be done anyway.
      var nameSpaceLabel = label.substr(1).split('.');
      var nameSpaceObject = template;
      var nameSpaceDepth = jsLength(nameSpaceLabel);
      for (var j = 0, J = nameSpaceDepth - 1; j < J; ++j) {
        var jLabel = nameSpaceLabel[j];
        if (!nameSpaceObject[jLabel]) {
          nameSpaceObject[jLabel] = {};
        }
        nameSpaceObject = nameSpaceObject[jLabel];
      }
      nameSpaceObject[nameSpaceLabel[nameSpaceDepth - 1]] = value;
    } else if (label) {
      // Any other jsvalues entry sets an attribute of the current
      // template node.
      if (typeof value == 'boolean') {
        // Handle boolean values that are set as attributes specially,
        // according to the XML/HTML convention.
        if (value) {
          domSetAttribute(template, label, label);
        } else {
          domRemoveAttribute(template, label);
        }
      } else {
        domSetAttribute(template, label, '' + value);
      }
    }
  }
}


/**
 * Implements the jscontent attribute. Evalutes the expression in
 * jscontent in the current context and with the current variables,
 * and assigns its string value to the content of the current template
 * node.
 *
 * @param {JsExprContext} context Current evaluation context.
 *
 * @param {Element} template Currently processed template node.
 *
 * @param {String} content Value of the jscontent attribute to be
 * processed.
 */
JstProcessor.prototype.jstContent_ = function(context, template, content) {
  var value = '' + context.jseval(content, template);
  // Prevent flicker when refreshing a template and the value doesn't
  // change.
  if (template.innerHTML == value) {
    return;
  }
  while (template.firstChild) {
    domRemoveNode(template.firstChild);
  }
  var t = domCreateTextNode(ownerDocument(template), value);
  domAppendChild(template, t);
}


/**
 * Helps to implement the transclude attribute, and is the initial
 * call to get hold of a template from its ID.
 *
 * @param {String} name The ID of the HTML element used as template.
 *
 * @returns {Element} The DOM node of the template. (Only element
 * nodes can be found by ID, hence it's a Element.)
 */
function jstGetTemplate(name) {
  var section = domGetElementById(document, name);
  if (section) {
    var ret = domCloneNode(section);
    domRemoveAttribute(ret, 'id');
    return ret;
  } else {
    return null;
  }
}

window['jstGetTemplate'] = jstGetTemplate;
window['jstProcess'] = jstProcess;
window['JsExprContext'] = JsExprContext;
