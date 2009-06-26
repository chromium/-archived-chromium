/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/**
 * @fileoverview The jsdoctoolkit loads this file and calls publish()
 */


var g_symbolSet;  // so we can look stuff up below.
var g_filePrefix;
var g_validJSDOCTypes = {
  'number': true,
  'Number': true,
  'object': true,
  'Object': true,
  '*': true,
  '...': true,
  'string': true,
  'String': true,
  'void': true,
  'undefined': true};
var g_unknownTypes = { };

/**
 * Called automatically by JsDoc Toolkit.
 * @param {SymbolSet} symbolSet Set of all symbols in all files.
 */
function publish(symbolSet) {
  publish.conf = {  // trailing slash expected for dirs
    ext: '.ezt',
    outDir: JSDOC.opt.d || SYS.pwd + '../out/jsdoc/',
    templatesDir: JSDOC.opt.t || SYS.pwd + '../templates/jsdoc/',
    symbolsDir: '',
    prefix: 'js_1_0_'};
  publish.conf.srcDir = publish.conf.outDir + 'src/'
  publish.conf.htmlDir = publish.conf.outDir + 'original_html/'

  // is source output is suppressed, just display the links to the source file
  if (JSDOC.opt.s && defined(Link) && Link.prototype._makeSrcLink) {
    Link.prototype._makeSrcLink = function(srcFilePath) {
      return '&lt;' + srcFilePath + '&gt;';
    }
  }

  // create the folders and subfolders to hold the output
  IO.mkPath((publish.conf.htmlDir).split('/'));

  // used to allow Link to check the details of things being linked to
  Link.symbolSet = symbolSet;
  Link.base = '../';

  // used to allow other parts of this module to access database of symbols
  // and the file prefix.
  g_symbolSet = symbolSet;
  g_filePrefix = publish.conf.prefix;

  // create the required templates
  try {
    var templatesDir = publish.conf.templatesDir;
    var classTemplate = new JSDOC.JsPlate(templatesDir + 'class.tmpl');
    var membersTemplate = new JSDOC.JsPlate(templatesDir + 'members.tmpl');
    var classTreeTemplate = new JSDOC.JsPlate(templatesDir + 'classtree.tmpl');
    var fileListTemplate = new JSDOC.JsPlate(templatesDir + 'filelist.tmpl');
    var annotatedTemplate = new JSDOC.JsPlate(templatesDir + 'annotated.tmpl');
    var namespacesTemplate = new JSDOC.JsPlate(templatesDir +
                                               'namespaces.tmpl');
  } catch(e) {
    print('Couldn\'t create the required templates: ' + e);
    quit();
  }

  // some ustility filters
  function hasNoParent($) {return ($.memberOf == '')}
  function isaFile($) {return ($.is('FILE'))}
  function isaClass($) {return ($.is('CONSTRUCTOR') || $.isNamespace)}

  // get an array version of the symbolset, useful for filtering
  var symbols = symbolSet.toArray();

  // create the hilited source code files
  if (false) {
    var files = JSDOC.opt.srcFiles;
    for (var i = 0, l = files.length; i < l; i++) {
      var file = files[i];
      makeSrcFile(file, publish.conf.srcDir);
    }
  }

  // get a list of all the classes in the symbolset
  var classes = symbols.filter(isaClass).sort(makeSortby('alias'));

  // create each of the class pages
  for (var i = 0, l = classes.length; i < l; i++) {
    var symbol = classes[i];
    g_unknownTypes = { };

    symbol.events = symbol.getEvents();   // 1 order matters
    symbol.methods = symbol.getMethods(); // 2

    print('Generating docs for: ' + symbol.alias);
    // Comment these lines in to see what data is available to the templates.
    //print('----------------------------------------------------------------');
    //dumpObject(symbol, 5);

    // <a href='symbol.source'>symbol.filename</a>
    symbol.source = symbol.srcFile;    // This is used as a link to the source
    symbol.filename = symbol.srcFile;  // This is display as the link.

    var output = '';
    output = classTemplate.process(symbol);

    IO.saveFile(publish.conf.outDir,
                (publish.conf.prefix + symbol.alias +
                 '_ref' + publish.conf.ext).toLowerCase(),
                output);
    IO.saveFile(publish.conf.htmlDir,
                (publish.conf.prefix + symbol.alias +
                 '_ref.html').toLowerCase(),
                output);

    var output = '';
    output = membersTemplate.process(symbol);
    IO.saveFile(publish.conf.outDir,
                (publish.conf.prefix + symbol.alias +
                 '_members' + publish.conf.ext).toLowerCase(),
                output);
    IO.saveFile(publish.conf.htmlDir,
                (publish.conf.prefix + symbol.alias +
                 '_members.html').toLowerCase(),
                output);
  }

  var classTree = classTreeTemplate.process(classes);
  IO.saveFile(publish.conf.outDir, 'classtree.html', classTree);

  var fileList = fileListTemplate.process(symbols);
  IO.saveFile(publish.conf.outDir, 'filelist.html', fileList);

  var annotated = annotatedTemplate.process(classes);
  IO.saveFile(publish.conf.outDir, 'annotated' + publish.conf.ext, annotated);
  IO.saveFile(publish.conf.htmlDir, 'annotated.html', annotated);

  var namespaces = namespacesTemplate.process(classes);
  IO.saveFile(publish.conf.outDir, 'namespaces' + publish.conf.ext, namespaces);
  IO.saveFile(publish.conf.htmlDir, 'namespaces.html', namespaces);
}


/**
 *  Gets just the first sentence (up to a full stop).
 *  Should not break on dotted variable names.
 *  @param {string} desc Description to extract summary from.
 *  @return {string} summary.
 */
function summarize(desc) {
  if (typeof desc != 'undefined')
    return desc.match(/([\w\W]+?\.)[^a-z0-9_$]/i) ? RegExp.$1 : desc;
}

/**
 * Makes a symbol sorter by some attribute.
 * @param {string} attribute to sort by.
 * @return {number} sorter result.
 */
function makeSortby(attribute) {
  return function(a, b) {
    if (a[attribute] != undefined && b[attribute] != undefined) {
      a = a[attribute].toLowerCase();
      b = b[attribute].toLowerCase();
      if (a < b) return -1;
      if (a > b) return 1;
      return 0;
    }
  }
}

/**
 * Pull in the contents of an external file at the given path.
 * @param {string} path Path of file relative to template directory.
 * @return {string} contents of file.
 */
function include(path) {
  var path = publish.conf.templatesDir + path;
  return IO.readFile(path);
}

/**
 * Turns a raw source file into a code-hilited page in the docs.
 * @param {string} path Path to source.
 * @param {string} srcDir path to place to store hilited page.
 * @param {string} opt_name to name output file.
 *
 */
function makeSrcFile(path, srcDir, opt_name) {
  if (JSDOC.opt.s) return;

  if (!opt_name) {
    opt_name = path.replace(/\.\.?[\\\/]/g, '').replace(/[\\\/]/g, '_');
    opt_name = opt_name.replace(/\:/g, '_');
  }

  var src = {path: path, name: opt_name, charset: IO.encoding, hilited: ''};

  if (defined(JSDOC.PluginManager)) {
    JSDOC.PluginManager.run('onPublishSrc', src);
  }

  if (src.hilited) {
    IO.saveFile(srcDir, opt_name + publish.conf.ext, src.hilited);
  }
}

/**
 * Builds output for displaying function parameters.
 * @param {Array} params Array of function params.
 * @return {string} string in format '(param1, param2)'.
 */
function makeSignature(params) {
  if (!params) return '()';
  var signature = '(' +
  params.filter(
    function($) {
      return $.name.indexOf('.') == -1; // don't show config params in signature
    }
  ).map(
    function($) {
      return $.name;
    }
  ).join(', ') + ')';
  return signature;
}

/**
 * Find symbol {@link ...} strings in text and turn into html links.
 * @param {string} str String to modify.
 * @return {string} modifed string.
 */
function resolveLinks(str) {
  str = str.replace(/\{@link ([^} ]+) ?\}/gi,
    function(match, symbolName) {
      return new Link().toSymbol(symbolName);
    }
  );

  return str;
}

/**
 * Make link from symbol.
 * @param {Object} symbol Symbol from class database.
 * @param {string} opt_extra extra suffix to add before '.html'.
 * @return {string} url to symbol.
 */
function getLinkToSymbol(symbol, opt_extra) {
  if (symbol.is('CONSTRUCTOR') || symbol.isNamespace) {
    return (g_filePrefix + symbol.alias + (opt_extra || '_ref') +
            '.html').toLowerCase();
  } else {
    var parentSymbol = getSymbol(symbol.memberOf);
    return (g_filePrefix + parentSymbol.alias + (opt_extra || '_ref') +
            '.html').toLowerCase() + '#' + symbol.name;
  }
}

/**
 * Given a class alias, returns a link to its reference page
 * @param {string} classAlias Fully qualified name of class.
 * @return {string} url to class.
 */
function getLinkToClassByAlias(classAlias) {
  var symbol = getSymbol(classAlias);
  if (!symbol) {
    throw Error('no documentation for "' + classAlias + '"');
  }
  return getLinkToSymbol(symbol);
}

/**
 * Given a class alias, returns a link to its member reference page
 * @param {string} classAlias Fully qualified name of class.
 * @return {string} url to class in members file.
 */
function getLinkToClassMembersByAlias(classAlias) {
  var symbol = getSymbol(classAlias);
  return getLinkToSymbol(symbol, '_members');
}

/**
 * Given a class alias like o3djs.namespace.function returns an HTML string
 * with a link to each part (o3djs, namespace, function)
 * @param {string} classAlias Fully qualified alias of class.
 * @param {string} opt_cssClassId css class Id to put in class="" instead links.
 * @return {string} html with links to each class and parent.
 */
function getHierarchicalLinksToClassByAlias(classAlias, opt_cssClassId) {
  var parts = classAlias.split('.');
  var name = '';
  var html = '';
  var delim = '';
  var classId = '';
  if (opt_cssClassId) {
    classId = ' class="' + opt_cssClassId + '"';
  }
  for (var pp = 0; pp < parts.length; ++pp) {
    var part = parts[pp];
    name = name + delim + part;
    link = getLinkToClassByAlias(name);
    html = html + delim + '<a' + classId +
           ' href="' + link + '">' + part + '</a>';
    delim = '.';
  }
  return html;
}

/**
 * Dumps a javascript object.
 *
 * @param {Object} obj Object to dump.
 * @param {number} depth Depth to dump (0 = forever).
 * @param {string} opt_prefix Prefix to put before each line.
 */
function dumpObject(obj, depth, opt_prefix) {
  opt_prefix = opt_prefix || '';
  --depth;
  for (var prop in obj) {
    if (typeof obj[prop] != 'function') {
      dumpWithPrefix(prop + ' : ' + obj[prop], opt_prefix);
      if (depth != 0) {
        dumpObject(obj[prop], depth, opt_prefix + '    ');
      }
    }
  }
}

/**
 * Dumps a string, putting a prefix before each line
 * @param {string} str String to dump.
 * @param {string} prefix Prefix to put before each line.
 */
function dumpWithPrefix(str, prefix) {
  var parts = str.split('\n');
  for (var pp = 0; pp < parts.length; ++pp) {
    print(prefix + parts[pp]);
  }
}

/**
 * gets the type of a property.
 * @param {!object} property Property object.
 * @return {string} type of property.
 */
function getPropertyType(property) {
  if (property.type.length > 0) {
    return property.type;
  } else {
    var tag = property.comment.getTag('type');
    if (tag.length > 0) {
      return tag[0].type;
    } else {
      return 'undefined';
    }
  }
}

/**
 * Converts [ to [[] for ezt files.
 * Also converts '\n\n' to <br/></br>
 * @param {string} str to sanitize.
 * @return {string} Sanitized string.
 */
function sanitizeForEZT(str) {
  return str.replace(/\[/g, '[[]').replace(/\n\n/g, '<br/><br/>');
}

/**
 * Check if string starts with another string.
 * @param {string} str String to check.
 * @param {string} prefix Prefix to check for.
 * @return {boolean} True if str starts with prefix.
 */
function startsWith(str, prefix) {
  return str.substring(0, prefix.length) === prefix;
}

/**
 * Check if string ends with another string.
 * @param {string} str String to check.
 * @param {string} suffix Suffix to check for.
 * @return {boolean} True if str ends with suffix.
 */
function endsWith(str, suffix) {
  return str.substring(str.length - suffix.length) === suffix;
}

/**
 * Converts a camelCase name to underscore as in TypeOfFruit becomes
 * type_of_fruit.
 * @param {string} str CamelCase string.
 * @return {string} underscorified str.
 */
function camelCaseToUnderscore(str) {
  function toUnderscore(match) {
    return '_' + match.toLowerCase();
  }
  return str[0].toLowerCase() +
      str.substring(1).replace(/[A-Z]/g, toUnderscore);
}

/**
 * Prints a warning about an unknown type only once.
 * @param {string} type Type specification.
 */
function reportUnknownType(type) {
  if (!g_unknownTypes[type]) {
    g_unknownTypes[type] = true;
    print ('WARNING: reference to unknown type: "' + type + '"');
  }
}

/**
 * Gets index of closing character.
 * @param {string} str string to search.
 * @param {number} startIndex index to start searching at. Must be an opening
 *      character.
 * @return {number} Index of closing character or (-1) if not found.
 */
function getIndexOfClosingCharacter(str, startIndex) {
  var openCloseMap = {
    '(': ')',
    '<': '>',
    '[': ']',
    '{': '}'};
  var closeMap = {
    ')': true,
    '>': true,
    ']': true,
    '}': true,
  };
  var stack = [];
  if (!openCloseMap[str[startIndex]]) {
    throw 'startIndex does not point to opening character.';
  }
  var endIndex = str.length;
  while (startIndex < endIndex) {
    var c = str[startIndex];
    var closing = openCloseMap[c];
    if (closing) {
      stack.unshift(closing);
    } else {
      closing = closeMap[c];
      if (closing) {
        var expect = stack.shift()
        if (c != expect) {
          return -1;
        }
        if (stack.length == 0) {
          return startIndex;
        }
      }
    }
    ++startIndex;
  }
  return -1;
}

/**
 * Converts a reference to a single JSDOC type specification to an html link.
 * @param {string} str to linkify.
 * @return {string} linkified string.
 */
function linkifySingleType(type) {
  var not = '';
  var equals = '';
  // Remove ! if it exists.
  if (type[0] == '!') {
    not = '!'
    type = type.substring(1);
  }
  if (endsWith(type, '=')) {
    equals = '=';
    type = type.substring(0, type.length - 1);
  }

  var link = type;

  // Check for array wrapper.
  if (startsWith(type, 'Array.<')) {
    var closingAngle = getIndexOfClosingCharacter(type, 6);
    if (closingAngle < 0) {
      print ('WARNING: Unmatched "<" in Array type : ' + type);
    } else {
      link = 'Array.&lt;' +
        linkifySingleType(type.substring(7, closingAngle)) + '>';
    }
  } else if (startsWith(type, 'function(')) {
    var closingParen = getIndexOfClosingCharacter(type, 8);
    if (closingParen < 0) {
      print ('WARNING: Unmatched "(" in function type : ' + type);
    } else {
      var end = type.substring(closingParen + 1);
      if (!startsWith(end, ': ')) {
        print ('WARNING: Malformed return specification on function. Must be' +
               ' "function(args): type" including the space after the colon.');
      } else {
        var args = type.substring(9, closingParen).split(/ *, */);
        var output = '';
        for (var ii = 0; ii < args.length; ++ii) {
          if (ii > 0) {
            output += ', ';
          }
          output += linkifyTypeSpec(args[ii]);
        }
        link = 'function(' + output + '): ' + linkifyTypeSpec(end.substring(2));
      }
    }
  } else if (type.indexOf(':') >= 0) { // check for records.
    var elements = type.split(/\s*,\s*/);
    var output = '{';
    for (var ii = 0; ii < elements.length; ++ii) {
      if (ii > 0) {
        output += ', ';
      }
      var element = elements[ii];
      var colon = element.indexOf(': ');
      if (colon < 0) {
        print ("WARNING: Malformed record specification. Format must be " +
               "{id1: type1, id2: type2, ...}.");
        output += element;
      } else {
        var name = element.substring(0, colon);
        var subType = element.substring(colon + 2);
        output += name + ':&nbsp;' + linkifyTypeSpec(subType)
      }
    }
    link = output + '}';
  } else {
    var symbol = getSymbol(type);
    if (symbol) {
      link = '<a class="el" href="' + getLinkToSymbol(symbol) + '">' +
          type + '</a>';
    } else if (startsWith(type, 'o3d.')) {
      // TODO: remove this hack, make nixysa generate JSDOC js
      //     files instead of C++ headers and pass those into
      //     jsdoctoolkit.
      reportUnknownType(type);
      link = '<a class="el" href="../classo3d_1_1_' +
          camelCaseToUnderscore(type.substring(4)) + '.html">' +
          type + '</a>';
    } else {
      // See if the symbol is a property or field.
      var period = type.lastIndexOf('.');
      if (period >= 0 && type != '...') {
        var subType = type.substring(0, period);
        symbol = getSymbol(subType);
        if (symbol) {
          var field = type.substring(period + 1);
          link = '<a class="el" href="' + getLinkToSymbol(symbol) + '#' +
              field + '">' +  type + '</a>';
        } else {
          if (subType[0] == '?') {
            subType = subType.substring(1);
          }
          if (!g_validJSDOCTypes[subType]) {
            reportUnknownType(type);
          }
        }
      }
    }
  }

  return not + link + equals;
}

/**
 * Fix function specs.
 * The jsdoctoolkit wrongly converts ',' to | as in 'function(a, b)' to
 * 'function(a|b)' and '{id1: type1, id2: type2}' to '{id1: type1|id2: type2}'
 * so we need to put those back here (or fix jsdoctoolkit, the proper solution).
 * @param {string} str JSDOC type specification string .
 * @return {string} str with '|' converted back to ', ' unless the specification
 *     starts with '(' and ends with ')'. That's not a very good check beacuse
 *     you could 'function((string|number)) and this would fail to do the right
 *     thing.
 *
 */
function fixSpecCommas(str) {
  // TODO: This is really a complete hack and we should fix the
  // jsdoctoolkit.
  if (startsWith(str, '(') && endsWith(str, ')')) {
    return str;
  } else {
    return str.replace(/\|/g, ', ');
  }
}

/**
 * Converts a JSDOC type specification into html links. For example
 * '(!o3djs.math.Vector3|!O3D.math.Vector4)' would change to
 * '(!<a href="??">o3djs.math.Vector3</a>
 * |!<a href="??">o3djs.math.Vector4</a>)'.
 * @param {string} str to linkify.
 * @return {string} linkified string.
 */
function linkifyTypeSpec(str) {
  var output = '';
  if (str) {
    var fixed = fixSpecCommas(str);
    // TODO: needs to split outside of parens and angle brackets.
    if (startsWith(fixed, '(') && endsWith(fixed, ')')) {
      var types = fixed.substring(1, fixed.length - 1).split('|');
      output += '(';
      for (var tt = 0; tt < types.length; ++tt) {
        if (tt > 0) {
          output += '|';
        }
        output += linkifySingleType(types[tt]);
      }
      output += ')';
    } else {
      output += linkifySingleType(fixed);
    }
  }
  return output;
}

/**
 * Gets a symbol for a type.
 * This is here mostly for debugging so you can insert a print before or after
 * each call to g_symbolSet.getSymbol.
 * @param {string} type fully qualified type.
 * @return {Symbol} The symbol object for the type or null if not found.
 */
function getSymbol(type) {
  return g_symbolSet.getSymbol(type);
}

/**
 * Gets the source path for a symbol starting from 'o3djs/'
 * @param {!Symbol} symbol The symbol to get the source name for.
 * @return {string} The name of the source file.
 */
function getSourcePath(symbol) {
  var path = symbol.srcFile.replace(/\\/g, '/');
  var index = path.indexOf('/o3djs/');
  return path.substring(index + 1);
}
