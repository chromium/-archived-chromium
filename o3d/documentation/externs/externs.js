// Copyright 2006 Google Inc.
// All Rights Reserved.

// Common external variables when compiling
// DEPRECATED: please use the files in //javascript/externs/...

// JavaScript objects
/**
 * @constructor
 * @param {*} var_args
 * @return {!Array}
 */
function Array(var_args) {}
/**
 * @constructor
 * @param {*} opt_value
 * @return {boolean}
 */
function Boolean(opt_value) {}
/**
 * @constructor
 * @return {string}
 */
function Date(opt_yr_num, opt_mo_num, opt_day_num, opt_hr_num, opt_min_num,
    opt_sec_num, opt_ms_num) {}
/**
 * @constructor
 * @param {*} opt_message
 * @param {*} opt_file
 * @param {*} opt_line
 * @return {!Error}
 */
function Error(opt_message, opt_file, opt_line) {}
/**
 * @constructor
 * @param {*} var_args
 */
function Function(var_args) {}
/**
 * @constructor
 * @param {*} opt_value
 * @return {number}
 */
function Number(opt_value) {}
/**
 * @constructor
 * @param {*} opt_value
 */
function Object(opt_value) {}
/**
 * @constructor
 * @param {*} opt_pattern
 * @param {*} opt_flags
 * @return {!RegExp}
 */
function RegExp(opt_pattern, opt_flags) {}
/**
 * @constructor
 * @param {*} opt_str
 * @return {string}
 */
function String(opt_str) {}

// For IE, returns an object representing key-value pairs for all the global
// variables prefixed with str, e.g. test*
function RuntimeObject(opt_str) {}

/** @constructor */
function Arguments() {}

// JavaScript functions
// escape() and unescape() should only be used in browser where
// encode/decodeURIComponent are not present, as the latter
// handle fancy Unicode characters.
function decodeURI(x) {}
function decodeURIComponent(x) {}
function encodeURI(x) {}
function encodeURIComponent(x) {}
function escape(x) {}    // Caution: Read comments above regarding
function unescape(x) {}  // use of escape and unescape.
function isFinite(x) {}
function isNaN(x) {}
function parseFloat(x) {}
// base is required.  If you really want to parse octal or hex based on the
// leader, then pass undefind as the base.
function parseInt(x, base) {}
function eval(x) {}

// JavaScript builtins
var arguments;
var Infinity;
var Math;
var NaN;
var undefined;

var builtins = {};
builtins.arguments; // deprecated
builtins.arity; // deprecated
builtins.caller;
builtins.constructor;
builtins.length;
builtins.name;
builtins.prototype;

// Window properties
// Only common properties are here, others such as open() and setTimeout()
// should be used with an explicit window object

/**
 * @constructor
 * @extends {EventTarget}
 */
function Window() {}

var window;
var top;
var navigator;
var document;
var screen;
var self;
function alert(x) {}
function confirm(x) {}
function prompt(x, opt_value) {}

// Magic functions for Firefox's LiveConnect.
// We'll probably never use these in practice. But redefining them
// will fire up the JVM, so we want to reserve the symbol names.
var JavaArray;
var JavaClass;
var JavaMember;
var JavaObject;
var JavaPackage;
var Packages;
var java;
var netscape;
var sun;

// Browser objects
/** @constructor */ function ActiveXObject(name, opt_location) {}
/** @constructor */ function CSSStyleDeclaration() {}
/** @constructor */ function StyleSheet() {}
/** @constructor */ function DOMParser() {}
// Image can't be used as a function
/**
 * @constructor
 * @extends {EventTarget}
 */
function Image(opt_width, opt_height) {}
/** @constructor */ function XMLHttpRequest() {}
/** @constructor */ function XMLSerializer() {}
/**
 * @noalias
 * @constructor
 */
function XPathResult() {}
/** @constructor */ function XSLTProcessor() {}
/** @constructor */ function Range() {}
/** @constructor */ function NodeFilter() {}
/** @constructor */ function Option(opt_text, opt_value, opt_defaultSelected,
                                    opt_selected) {}
var opera;

// Browser types for type checking
// see http://www.w3.org/TR/DOM-Level-2-HTML/ecma-script-binding.html

// TODO(nicksantos): Rewrite all the DOM interfaces as interfaces, instead
// of kluding them as an inheritance hierarchy.

/**
 * @constructor
 * @extends {Node}
 */
function Document() {}

/**
 * @constructor
 * @extends {Node}
 */
function DocumentFragment() {}

/**
 * @constructor
 * @extends {Node}
 */
function DocumentType() {}

/**
 * @constructor
 * @extends {EventTarget}
 */
function Node() {}

/** @constructor */ function NodeList() {}

/**
 * @constructor
 * @extends {Node}
 */
function Element() {}

/**
 * @constructor
 * @extends {Node}
 */
function CharacterData() {}

/**
 * @constructor
 * @extends {CharacterData}
 */
function Text() {}

/** @constructor */ function Event() {}
/** @constructor */ function EventTarget() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLAnchorElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLAppletElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLAreaElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLBaseElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLBaseFontElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLBlockquoteElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLBodyElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLBRElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLButtonElement() {}

/**
 * @constructor
 */
function HTMLCollection() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLDirectoryElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLDivElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLDListElement() {}

/**
 * @constructor
 * @extends {Document}
 */
function HTMLDocument() {}

/**
 * @constructor
 * @extends {Element}
 */
function HTMLElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLFieldSetElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLFontElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLFormElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLFrameElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLFrameSetElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLHeadElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLHeadingElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLHRElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLHtmlElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLIFrameElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLImageElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLInputElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLIsIndexElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLLabelElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLLayerElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLLegendElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLLIElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLLinkElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLMapElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLMenuElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLMetaElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLModElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLObjectElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLOListElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLOptGroupElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLOptionElement() {}

/**
 * @constructor
 */
function HTMLOptionsCollection() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLParagraphElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLParamElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLPreElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLQuoteElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLScriptElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLSelectElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLSpanElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLStyleElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLTableCaptionElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLTableCellElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLTableColElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLTableElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLTableRowElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLTableSectionElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLTextAreaElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLTitleElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function HTMLUListElement() {}

// Gecko Selection interface
/** @constructor */ function Selection() {}

// IE Range interface
/** @constructor */ function TextRange() {}

// IE Control Range interface
/** @constructor */ function controlRange() {}

// Safari and Firefox canvas rendering context
/** @constructor */ function CanvasRenderingContext2D() {}

// Browser object methods and properties

/**
 * The postMessage method (as defined by HTML5 spec and implemented in FF3).
 * @param {string} message
 * @param {string} targetOrigin
 */
Window.prototype.postMessage = function(message, targetOrigin) {};

/**
 * The postMessage method (as implemented in Opera).
 * @param {string} message
 */
Document.prototype.postMessage = function(message) {};

// Below is a generic list of methods and properties defined on various
// browser/DOM objects. Ideally these would be defined on the appropriate
// prototypes (as is done above). As long as the list below is around,
// please keep it alphabetically sorted.
var methods = {};
methods.$1;
methods.$2;
methods.$3;
methods.$4;
methods.$5;
methods.$6;
methods.$7;
methods.$8;
methods.$9;
methods.ANY_TYPE;
methods.ANY_UNORDERED_NODE_TYPE;
methods.ATTRIBUTE_NODE;
methods.AT_TARGET;
methods.BOOLEAN_TYPE;
methods.BUBBLING_PHASE;
methods.CAPTURING_PHASE;
methods.CDATA_SECTION_NODE;
methods.CHARSET_RULE;
methods.COMMENT_NODE;
methods.CSS_ATTR;
methods.CSS_CM;
methods.CSS_COUNTER;
methods.CSS_CUSTOM;
methods.CSS_DEG;
methods.CSS_DIMENSION;
methods.CSS_EMS;
methods.CSS_EXS;
methods.CSS_GRAD;
methods.CSS_HZ;
methods.CSS_IDENT;
methods.CSS_IN;
methods.CSS_INHERIT;
methods.CSS_KHZ;
methods.CSS_MM;
methods.CSS_MS;
methods.CSS_NUMBER;
methods.CSS_PC;
methods.CSS_PERCENTAGE;
methods.CSS_PRIMITIVE_VALUE;
methods.CSS_PT;
methods.CSS_PX;
methods.CSS_RAD;
methods.CSS_RECT;
methods.CSS_RGBCOLOR;
methods.CSS_S;
methods.CSS_STRING;
methods.CSS_UNKNOWN;
methods.CSS_URI;
methods.CSS_VALUE_LIST;
methods.DOCUMENT_FRAGMENT_NODE;
methods.DOCUMENT_NODE;
methods.DOCUMENT_TYPE_NODE;
methods.DOMSTRING_SIZE_ERR;
methods.E;
methods.ELEMENT_NODE;
methods.END_TO_END;
methods.END_TO_START;
methods.ENTITY_NODE;
methods.ENTITY_REFERENCE_NODE;
methods.FILTER_ACCEPT;
methods.FILTER_REJECT;
methods.FILTER_SKIP;
methods.FIRST_ORDERED_NODE_TYPE;
methods.FONT_FACE_RULE;
methods.HIERARCHY_REQUEST_ERR;
methods.HORIZONTAL_AXIS;
methods.IMPORT_RULE;
methods.INDEX_SIZE_ERR;
methods.INFINITY;
methods.INUSE_ATTRIBUTE_ERR;
methods.INVALID_ACCESS_ERR;
methods.INVALID_CHARACTER_ERR;
methods.INVALID_MODIFICATION_ERR;
methods.INVALID_STATE_ERR;
methods.LOG10E;
methods.LOG2E;
methods.MAX_VALUE;
methods.MEDIA_RULE;
methods.MIN_VALUE;
methods.MODIFICATION;
methods.NAMESPACE_ERR;
methods.NEGATIVE_INFINITY;
methods.NOTATION_NODE;
methods.NOT_FOUND_ERR;
methods.NOT_SUPPORTED_ERR;
methods.NO_DATA_ALLOWED_ERR;
methods.NO_MODIFICATION_ALLOWED_ERR;
methods.NUMBER_TYPE;
methods.NaN;
methods.ORDERED_NODE_ITERATOR_TYPE;
methods.ORDERED_NODE_SNAPSHOT_TYPE;
methods.PAGE_RULE;
methods.PI;
methods.POSITIVE_INFINITY;
methods.PROCESSING_INSTRUCTION_NODE;
methods.PercentLoaded; // flash?
methods.REMOVAL;
methods.SHOW_ALL;
methods.SHOW_ATTRIBUTE;
methods.SHOW_CDATA_SECTION;
methods.SHOW_COMMENT;
methods.SHOW_DOCUMENT;
methods.SHOW_DOCUMENT_FRAGMENT;
methods.SHOW_DOCUMENT_TYPE;
methods.SHOW_ELEMENT;
methods.SHOW_ENTITY;
methods.SHOW_ENTITY_REFERENCE;
methods.SHOW_NOTATION;
methods.SHOW_PROCESSING_INSTRUCTION;
methods.SHOW_TEXT;
methods.START_TO_END;
methods.START_TO_START;
methods.STRING_TYPE;
methods.STYLE_RULE;
methods.SYNTAX_ERR;
methods.TEXT_NODE;
methods.TGotoLabel;
methods.TPlay;
methods.UNKNOWN_RULE;
methods.UNORDERED_NODE_ITERATOR_TYPE;
methods.UNORDERED_NODE_SNAPSHOT_TYPE;
methods.UNSPECIFIED_EVENT_TYPE_ERR;
methods.UTC;
methods.VERTICAL_AXIS;
methods.WRONG_DOCUMENT_ERR;
methods.aLink;
methods.abbr;
methods.abort;
methods.abs;
methods.accept;
methods.acceptCharset;
methods.acceptNode;
methods.accessKey;
methods.acos;
methods.action;
methods.activeElement;
methods.actualEncoding;
methods.add;
methods.addColorStop;
methods.addElement;
methods.addEventListener;
methods.addRange;
methods.addRule;
methods.adoptNode;
methods.alert;
methods.align;
methods.alinkColor;
methods.all;
methods.alt;
methods.altGraphKey;
methods.altKey;
methods.anchorNode;
methods.anchorOffset;
methods.anchors;
methods.angle;
methods.appCodeName;
methods.appName;
methods.appVersion;
methods.appendChild = function(newChild) {};
methods.appendData;
methods.appendMedium;
methods.applets;
methods.apply;
methods.arc;
methods.arcTo;
methods.archive;
methods.areas;
methods.arguments;
methods.asin;
methods.assign;
methods.async;
methods.atan2;
methods.atan;
methods.atob;
methods.attachEvent;
methods.attrChange;
methods.attrName;
methods.attributes;
methods.availHeight;
methods.availWidth;
methods.axis;
methods.back;
methods.baseNode;
methods.baseOffset;
methods.beginPath;
methods.bezierCurveTo;
methods.bgColor;
methods.blendTrans;
methods.blue;
methods.blur;
methods.body;
methods.boundingWidth;
methods.btoa;
methods.bubbles;
methods.button;
methods.call;
methods.callee;
methods.caller;
methods.cancelBubble;
methods.cancelable;
methods.canHaveChildren;
methods.caption;
methods.captureEvents;
methods.ceil;
methods.cellIndex;
methods.cellPadding;
methods.cellSpacing;
methods.cells;
methods.ch;
methods.chOff;
methods.charAt = function(index) {};
methods.charCode;
methods.charCodeAt;
methods.characterSet;
methods.charset;
methods.checked;
methods.childNodes;
methods.children;
methods.childNodes;
methods.cite;
methods.className;
methods.clearData;
methods.clearInterval = function(intervalRev) {};
methods.clearParameters;
methods.clearRect;
methods.clearShadow;
methods.clearTimeout = function(timeoutRef) {};
methods.click;
methods.clientHeight;
methods.clientLeft;
methods.clientTop;
methods.clientWidth;
methods.clientX;
methods.clientY;
methods.clipboardData;
methods.cloneContents;
methods.cloneNode = function(deepClone) {};
methods.cloneRange;
methods.close;
methods.closed;
methods.closePath;
methods.closed;
methods.code;
methods.codeBase;
methods.codeType;
methods.colSpan;
methods.collapse;
methods.collapseToEnd;
methods.collapseToStart;
methods.collapsed;
methods.color;
methods.color2;
methods.colorDepth;
methods.cols;
methods.commonAncestorContainer;
methods.compact;
methods.compareBoundaryPoints;
methods.compareEndPoints;
methods.compareDocumentPosition = function(el) {};
methods.compatMode;
methods.comparePoint;
methods.complete;
methods.componentFromPoint;
methods.concat;
methods.console;
methods.constructor;
methods.contains;
methods.containsNode;
methods.contentDocument;
methods.contentEditable;
methods.contentWindow;
methods.cookie;
methods.cookieEnabled;
methods.coordorigin;
methods.coords;
methods.coordsize;
methods.cos;
methods.createAttribute;
methods.createAttributeNS;
methods.createCDATASection;
methods.createCSSStyleSheet;
methods.createCaption;
methods.createComment;
methods.createControlRange;
methods.createContextualFragment;
methods.createDocument;
methods.createDocumentFragment;
methods.createDocumentType;
methods.createElement;
methods.createElementNS;
methods.createEntityReference;
methods.createEvent;
methods.createEventObject;
methods.createHTMLDocument;
methods.createLinearGradient;
methods.createNode;
methods.createNodeIterator;
methods.createNSResolver;
methods.createPattern;
methods.createPopup;
methods.createProcessingInstruction;
methods.createProcessor = function() {}; // XSLT
methods.createRadialGradient;
methods.createRange;
methods.createStyleSheet;
methods.createTFoot;
methods.createTHead;
methods.createTextNode;
methods.createTextRange = function() {};
methods.createTreeWalker;
methods.cssRules;
methods.cssText;
methods.cssValueType;
methods.ctrlKey;
methods.currentNode;
methods.currentStyle;
methods.currentTarget;
methods.data;
methods.dataTransfer;
methods.dateTime;
methods.declare;
methods.defaultCharset;
methods.defaultChecked;
methods.defaultSelected;
methods.defaultValue;
methods.defaultView;
methods.defer;
methods.deleteCaption;
methods.deleteCell;
methods.deleteContents;
methods.deleteData;
methods.deleteMedium;
methods.deleteRow;
methods.deleteRule;
methods.deleteTFoot;
methods.deleteTHead;
methods.description;
methods.designMode;
methods.detach;
methods.detachEvent;
methods.detail;
methods.dir;
methods.direction;
methods.disabled;
methods.dispatchEvent;
methods.doctype;
methods.document;
methods.documentElement;
methods.documentMode;
methods.domain;
methods.drawImage;
methods.drawImageFromRect;
methods.dropEffect;
methods.dump;
methods.duplicate = function() {};
methods.effectAllowed;
methods.elementFromPoint;
methods.elements;
methods.embeds;
methods.empty;
methods.enabledPlugin;
methods.encoding;
methods.enctype;
methods.endContainer;
methods.endOffset;
methods.entities;
methods.evaluate = function(
    xpathExpression, contextNode, namespaceResolver, resultType, result) {};
methods.event;
methods.eventPhase;
methods.exec;
methods.execCommand;
methods.execScript;
methods.expand;
methods.expandEntityReferences;
methods.explicitOriginalTarget;
methods.exp = function(x) {};
methods.expn;
methods.extend;
methods.extentNode;
methods.extentOffset;
methods.extractContents;
methods.eval;
methods.every;
methods.face;
methods.fgColor;
methods.fileName;
methods.filename;
methods.fill = function() {};
methods.filled;
methods.fillcolor;
methods.fillRect;
methods.fillStyle;
methods.filter;
methods.filters;
methods.findText; // IE text ranges
methods.fireEvent;
methods.firstChild;
methods.floor;
methods.focus;
methods.focusNode;
methods.focusOffset;
methods.forEach;
methods.form;
methods.forms;
methods.forward;
methods.frame;
methods.frames;
methods.frameBorder;
methods.frameElement;
methods.fromCharCode = function(var_args) {};
methods.fromElement;
methods.getAllResponseHeaders;
methods.getAttribute;
methods.getAttributeNS;
methods.getAttributeNode;
methods.getAttributeNodeNS;
methods.getBookmark;
methods.getBoundingClientRect = function() {};
methods.getBoxObjectFor = function(node) {};
methods.getComputedStyle;
methods.getContext;
methods.getCounterValue;
methods.getData;
methods.getDate;
methods.getDay;
methods.getElementById;
methods.getElementsByName;
methods.getElementsByClassName;
methods.getElementsByTagName;
methods.getElementsByTagNameNS;
methods.getFloatValue;
methods.getFullYear;
methods.getHours;
methods.getImageData = function(sx, sy, sw, sh) {};
methods.getMatchedCSSRules;
methods.getMilliseconds;
methods.getMinutes;
methods.getMonth;
methods.getNamedItem;
methods.getNamedItemNS;
methods.getOverrideStyle;
methods.getParameter;
methods.getPropertyCSSValue;
methods.getPropertyPriority;
methods.getPropertyShorthand;
methods.getPropertyValue;
methods.getRGBColorValue;
methods.getRangeAt;
methods.getRectValue;
methods.getResponseHeader;
methods.getSeconds;
methods.getSelection;
methods.getStringValue;
methods.getTime;
methods.getTimezoneOffset;
methods.getUTCDate;
methods.getUTCDay;
methods.getUTCFullYear;
methods.getUTCHours;
methods.getUTCMilliseconds;
methods.getUTCMinutes;
methods.getUTCMonth;
methods.getUTCSeconds;
methods.GetVariable; // ActiveXObject
methods.global;
methods.globalAlpha;
methods.globalCompositeOperation;
methods.go;
methods.green;
methods.handleEvent;
methods.hasAttribute;
methods.hasAttributeNS;
methods.hasAttributes = function() {};
methods.hasChildNodes = function() {};
methods.hasFeature;
methods.hasFocus;
methods.hasOwnProperty;
methods.hash;
methods.head;
methods.headers;
methods.history;
methods.host;
methods.hostname;
methods.href;
methods.hreflang;
methods.hspace;
methods.htmlFor;
methods.htmlText;
methods.httpEquiv;
methods.id;
methods.identifier;
methods.ignoreCase;
methods.images;
methods.implementation;
methods.imports;
methods.importNode;
methods.importStylesheet;
methods.indeterminate;
methods.index;
methods.indexOf;
methods.initEvent;
methods.initKeyboardEvent;
methods.initMouseEvent;
methods.initMutationEvent;
methods.initUIEvent;
methods.innerHTML;
methods.innerHeight;
methods.innerText;
methods.innerWidth;
methods.inputEncoding;
methods.input; // XSLT
methods.inRange;
methods.insertAdjacentText = function(where, text) {};
methods.insertBefore = function(newNode, childNode) {};
methods.insertCell;
methods.insertData;
methods.insertNode;
methods.insertRow;
methods.insertRule;
methods.internalSubset;
methods.intersectsNode;
methods.invalidIteratorState;
methods.isCollapsed;
methods.isContentEditable;
methods.isEqual = function(textRange) {};
methods.isPointInPath = function(x, y) {};
methods.isMap;
methods.isOpen;
methods.isPointInPath = function(x, y) {};
methods.isPropertyImplicit;
methods.isPrototypeOf;
methods.isSupported = function(feature, version) {};
methods.item;
methods.iterateNext = function() {};
methods.javaEnabled;
methods.join;
methods.keyCode;
methods.keyIdentifier;
methods.keyLocation;
methods.KhtmlOpacity;
methods.label;
methods.lang;
methods.language;
methods.lastChild;
methods.lastIndex;
methods.lastIndexOf;
methods.lastMatch;
methods.lastModified;
methods.lastParen;
methods.layerX;
methods.layerY;
methods.leftContext;
methods.length;
methods.line;
methods.lineCap;
methods.lineJoin;
methods.lineNumber;
methods.lineTo;
methods.lineWidth;
methods.linkColor;
methods.links;
methods.localName;
methods.load; // XSLT
methods.loadXML;
methods.localeCompare;
methods.location;
methods.log;
methods.longDesc;
methods.map;
methods.marginHeight;
methods.marginWidth;
methods.match;
methods.max;
methods.maxLength;
methods.media;
methods.mediaText;
methods.mergeAttributes;
methods.message;
methods.metaKey;
methods.method;
methods.mimeTypes;
methods.min;
methods.miterLimit;
methods.move = function(unit, opt_count) {};
methods.moveBy;
methods.moveEnd;
methods.moveStart;
methods.moveTo;
methods.moveToElementText = function(node) {};
methods.moveToPoint;
methods.multiline;
methods.multiple;
methods.name;
methods.namedItem;
methods.namespaces;
methods.namespaceURI;
methods.naturalHeight;
methods.naturalWidth;
methods.navigationMode;
methods.newValue;
methods.nextNode;
methods.nextSibling;
methods.nodeType;
methods.noHref;
methods.noResize;
methods.noShade;
methods.now; // Date.now()
methods.noWrap;
methods.nodeName;
methods.nodeType;
methods.nodeValue;
methods.normalize = function() {};
methods.notationName;
methods.notations;
methods.object;
methods.offsetHeight;
methods.offsetLeft;
methods.offsetParent;
methods.offsetTop;
methods.offsetWidth;
methods.offsetX;
methods.offsetY;
methods.onabort;
methods.onbeforeunload;
methods.onblur;
methods.onchange;
methods.onclick;
methods.oncontextmenu;
methods.ondblclick;
methods.ondrag;
methods.ondragend;
methods.ondragenter; // IE
methods.ondragleave; // IE
methods.ondragover; // IE
methods.ondragstart; // IE
methods.ondrop; // IE
methods.onerror;
methods.onfocus;
methods.onkeydown;
methods.onkeypress;
methods.onkeyup;
methods.onLine;
methods.onload;
methods.onmousedown;
methods.onmousemove;
methods.onmouseout;
methods.onmouseover;
methods.onmouseup;
methods.onmousewheel;
methods.onpropertychange;
methods.onreadystatechange;
methods.onresize;
methods.onscroll;
methods.onselect;
methods.onselectend;
methods.onselectstart;
methods.onsubmit;
methods.onunload;
methods.opacity;
methods.open;
methods.openDialog;
methods.opener;
methods.opera;
methods.options;
methods.originalTarget;
methods.outerHTML;
methods.outerHeight;
methods.outerText;
methods.outerWidth;
methods.output; // XSLT
methods.overflowX;
methods.overflowY;
methods.overrideMimeType;
methods.ownerDocument;
methods.ownerElement;
methods.ownerNode;
methods.ownerRule;
methods.owningElement;
methods.pageX;
methods.pageXOffset;
methods.pageY;
methods.pageYOffset;
methods.parentElement = function() {};
methods.parent;
methods.parentNode;
methods.parentRule;
methods.parentStyleSheet;
methods.parentWindow;
methods.parse;
methods.parseError;
methods.parseFromString;
methods.parseJSON;
methods.pasteHTML;
methods.pathname;
methods.peek;
methods.pixelBottom;
methods.pixelDepth;
methods.pixelHeight;
methods.pixelLeft;
methods.pixelRight;
methods.pixelTop;
methods.pixelWidth;
methods.platform;
methods.play;
methods.plugins;
methods.pointerBeforeReferenceNode;
methods.pop;
methods.port;
methods.pow;
methods.preferredStylesheetSet;
methods.prefix;
methods.prevValue;
methods.preventDefault;
methods.previousNode;
methods.previousSibling;
methods.primitiveType;
methods.print;
methods.product;
methods.productSub;
methods.profile;
methods.prompt;
methods.propertyIsEnumerable;
methods.propertyName;
methods.protocol;
methods.publicId;
methods.push;
methods.putImageData = function(image, dx, dy) {};
methods.quadraticCurveTo;
methods.queryCommandEnabled;
methods.queryCommandIndeterm;
methods.queryCommandState;
methods.queryCommandSupported;
methods.queryCommandValue;
methods.querySelector = function(selector) {};
methods.querySelectorAll = function(selector) {};
methods.quote = function() {};
methods.rangeCount;
methods.random;
methods.rangeCount;
methods.readOnly;
methods.readyState;
methods.rect;
methods.red;
methods.reduce = function(callback, opt_initialValue) {};
methods.reduceRight = function(callback, opt_initialValue) {};
methods.referenceNode;
methods.referrer;
methods.refresh;
methods.rel;
methods.relatedNode;
methods.relatedTarget;
methods.releaseCapture;
methods.releaseEvents;
methods.reload;
methods.remove;
methods.removeAllRanges;
methods.removeAttribute;
methods.removeAttributeNS;
methods.removeAttributeNode;
methods.removeChild = function(oldChild) {};
methods.removeEventListener;
methods.removeExpression = function(propertyName) {};
methods.removeNamedItem;
methods.removeNamedItemNS;
methods.removeNode;
methods.removeParameter;
methods.removeProperty;
methods.repeat;
methods.replace;
methods.replaceChild = function(newNode, childNode) {};
methods.replaceData;
methods.replaceNode = function(newNode) {};
methods.reset;
methods.resizeBy = function(x, y) {};
methods.resizeTo = function(w, h) {};
methods.resolveExternals; // XSLT
methods.responseText;
methods.responseXML;
methods.restore;
methods.result;
methods.returnValue;
methods.rev;
methods.reverse;
methods.rightContext;
methods.rotate;
methods.rotation;
methods.round;
methods.rowIndex;
methods.rowSpan;
methods.rows;
methods.rules;
methods.runtimeStyle;
methods.save;
methods.scale;
methods.scheme;
methods.scope;
methods.screen;
methods.screenLeft;
methods.screenTop;
methods.screenX;
methods.screenY;
methods.scripts;
methods.scroll = function(x, y) {};
methods.scrollBy;
methods.scrollByLines;
methods.scrollByPages;
methods.scrollHeight;
methods.scrollIntoView;
methods.scrollLeft;
methods.scrollTo = function(x, y) {};
methods.scrollTop;
methods.scrollWidth;
methods.scrollX;
methods.scrollY;
methods.scrolling;
methods.search;
methods.sectionRowIndex;
methods.select;
methods.selectNode;
methods.selectNodeContents;
methods.selectNodes = function(patternString) {};
methods.selectSingleNode = function(patternString) {};
methods.selected;
methods.selectedIndex;
methods.selectedStylesheetSet;
methods.selection;
methods.selectionEnd;
methods.selectionStart;
methods.selectorText;
methods.send;
methods.separator;
methods.serializeToString;
methods.setActive = function() {};
methods.setAlpha;
methods.setAttribute;
methods.setAttributeNS;
methods.setAttributeNode;
methods.setAttributeNodeNS;
methods.setBaseAndExtent;
methods.setCapture;
methods.setCompositeOperation;
methods.setData;
methods.setDate;
methods.setDragImage;
methods.setEnd;
methods.setEndAfter;
methods.setEndBefore;
methods.setEndPoint = function(type, range) {};
methods.setExpression = function(propertyName, expression) {};
methods.setFillColor;
methods.setFloatValue;
methods.setFullYear;
methods.setHours;
methods.setInterval = function(fn, interval) {};
methods.setLineCap;
methods.setLineJoin;
methods.setLineWidth;
methods.setMilliseconds;
methods.setMinutes;
methods.setMiterLimit;
methods.setMonth;
methods.setNamedItem;
methods.setNamedItemNS;
methods.setParameter;
methods.setPosition;
methods.setProperty;
methods.setRequestHeader;
methods.setSUTCSeconds;
methods.setSeconds;
methods.setSelectionRange;
methods.setShadow;
methods.setStart;
methods.setStartAfter;
methods.setStartBefore;
methods.setStringValue;
methods.setStrokeColor;
methods.setTime;
methods.setTimeout = function(fn, time) {};
methods.setTransform = function(m11, m12, m21, m22, dx, dy) {};
methods.setUTCDate;
methods.setUTCFullYear;
methods.setUTCHours;
methods.setUTCMilliseconds;
methods.setUTCMinutes;
methods.setUTCMonth;
methods.setUTCSeconds;
methods.shadowBlur;
methods.shadowColor;
methods.shadowOffsetX;
methods.shadowOffsetY;
methods.shape;
methods.sheet;
methods.shift;
methods.shiftKey;
methods.showModalDialog = function(url, opt_arguments, opt_features) {};
methods.sin;
methods.singleNodeValue;
methods.slice;
methods.snapshotItem = function(index) {};
methods.snapshotLength;
methods.some;
methods.sort;
methods.source;
methods.sourceIndex;
methods.sourceURL;
methods.span;
methods.specified;
methods.splice;
methods.split;
methods.splitText;
methods.sqrt;
methods.src;
methods.srcElement;
methods.stack;
methods.standby;
methods.start;
methods.startContainer;
methods.startOffset;
methods.status;
methods.statusText;
methods.stop;
methods.stopPropagation;
methods.stroke;
methods.strokecolor;
methods.stroked;
methods.strokeRect;
methods.strokeStyle;
methods.strokeweight;
methods.style;
methods.styleSheet;
methods.stylesheet; // XSLT
methods.styleSheets;
methods.submit;
methods.substr = function(start, opt_length) {};
methods.substring = function(start, opt_end) {};
methods.substringData;
methods.suffixes;
methods.summary;
methods.surroundContents;
methods.systemId;
methods.tBodies;
methods.tFoot;
methods.tHead;
methods.tabIndex;
methods.tagName;
methods.tags;
methods.tan;
methods.target;
methods.test;
methods.text;
methods.textContent;
methods.timeStamp;
methods.title;
methods.toDateString;
methods.toElement;
methods.toExponential;
methods.toFixed;
methods.toJSONString;
methods.toLocaleDateString;
methods.toLocaleLowerCase;
methods.toLocaleString;
methods.toLocaleTimeString;
methods.toLocaleUpperCase;
methods.toLowerCase;
methods.toPrecision;
methods.toSource;
methods.toString;
methods.toSource;
methods.toTimeString;
methods.toUTCString;
methods.toUpperCase;
methods.transform = function(m11, m12, m21, m22, dx, dy) {};
methods.setTransform = function(m11, m12, m21, m22, dx, dy) {};
methods.transformToDocument;
methods.transformToFragment;
methods.translate;
methods.type;
methods.types;
methods.uneval;
methods.unshift;
methods.unselectable;
methods.useMap;
methods.userAgent;
methods.vAlign;
methods.value;
methods.valueOf;
methods.valueType;
methods.vendor;
methods.version;
methods.view;
methods.visible;
methods.vlinkColor;
methods.vspace;
methods.watch;
methods.whatToShow;
methods.wheelDelta;
methods.wheelDeltaX;
methods.wheelDeltaY;
methods.which;
methods.write;
methods.writeln;
methods.x;
methods.xml;
methods.y;

// CSS style properties
methods.azimuth;
methods.background;
methods.backgroundAttachment;
methods.backgroundColor;
methods.backgroundImage;
methods.backgroundPosition;
methods.backgroundPositionX;
methods.backgroundPositionY;
methods.backgroundRepeat;
methods.border;
methods.borderCollapse;
methods.borderColor;
methods.borderSpacing;
methods.borderStyle;
methods.borderTop;
methods.borderRight;
methods.borderBottom;
methods.borderLeft;
methods.borderTopColor;
methods.borderRightColor;
methods.borderBottomColor;
methods.borderLeftColor;
methods.borderTopStyle;
methods.borderRightStyle;
methods.borderBottomStyle;
methods.borderLeftStyle;
methods.borderTopWidth;
methods.borderRightWidth;
methods.borderBottomWidth;
methods.borderLeftWidth;
methods.borderWidth;
methods.bottom;
methods.captionSide;
methods.clear;
methods.clip;
methods.color;
methods.content;
methods.counterIncrement;
methods.counterReset;
methods.cssFloat;
methods.cue;
methods.cueAfter;
methods.cueBefore;
methods.cursor;
methods.direction;
methods.display;
methods.elevation;
methods.emptyCells;
methods.font;
methods.fontFamily;
methods.fontSize;
methods.fontSizeAdjust;
methods.fontStretch;
methods.fontStyle;
methods.fontVariant;
methods.fontWeight;
methods.height;
methods.hideFocus;
methods.left;
methods.letterSpacing;
methods.lineHeight;
methods.listStyle;
methods.listStyleImage;
methods.listStylePosition;
methods.listStyleType;
methods.margin;
methods.marginBottom;
methods.marginLeft;
methods.marginRight;
methods.marginTop;
methods.markerOffset;
methods.marks;
methods.maxHeight;
methods.maxWidth;
methods.minHeight;
methods.minWidth;
methods.orphans;
methods.outline;
methods.outlineColor;
methods.outlineStyle;
methods.outlineWidth;
methods.overflow;
methods.padding;
methods.paddingTop;
methods.paddingRight;
methods.paddingBottom;
methods.paddingLeft;
methods.page;
methods.pageBreakAfter;
methods.pageBreakBefore;
methods.pageBreakInside;
methods.pause;
methods.pauseAfter;
methods.pauseBefore;
methods.pitch;
methods.pitchRange;
methods.position;
methods.quotes;
methods.richness;
methods.right;
methods.size;
methods.speak;
methods.speakHeader;
methods.speakNumeral;
methods.speakPunctuation;
methods.speechRate;
methods.stress;
methods.styleFloat;
methods.tableLayout;
methods.textAlign;
methods.textDecoration;
methods.textIndent;
methods.textShadow;
methods.textTransform;
methods.top;
methods.unicodeBidi;
methods.verticalAlign;
methods.visibility;
methods.voiceFamily;
methods.volume;
methods.whiteSpace;
methods.widows;
methods.width;
methods.wordSpacing;
methods.wordWrap;
methods.zIndex;
methods.zoom;

// Gecko-only CSS style names. Copied from:
// http://lxr.mozilla.org/mozilla1.8.0/source/dom/public/idl/css/nsIDOMCSS2Properties.idl
methods.MozAppearance;
methods.MozBackgroundClip;
methods.MozBackgroundInlinePolicy;
methods.MozBackgroundOrigin;
methods.MozBinding;
methods.MozBorderBottomColors;
methods.MozBorderLeftColors;
methods.MozBorderRightColors;
methods.MozBorderTopColors;
methods.MozBorderRadius;
methods.MozBorderRadiusTopleft;
methods.MozBorderRadiusTopright;
methods.MozBorderRadiusBottomleft;
methods.MozBorderRadiusBottomright;
methods.MozBoxAlign;
methods.MozBoxDirection;
methods.MozBoxFlex;
methods.MozBoxOrient;
methods.MozBoxOrdinalGroup;
methods.MozBoxPack;
methods.MozBoxSizing;
methods.MozColumnCount;
methods.MozColumnWidth;
methods.MozColumnGap;
methods.MozFloatEdge;
methods.MozForceBrokenImageIcon;
methods.MozImageRegion;
methods.MozMarginEnd;
methods.MozMarginStart;
methods.MozOpacity;
methods.MozOutline;
methods.MozOutlineColor;
methods.MozOutlineRadius;
methods.MozOutlineRadiusTopleft;
methods.MozOutlineRadiusTopright;
methods.MozOutlineRadiusBottomleft;
methods.MozOutlineRadiusBottomright;
methods.MozOutlineStyle;
methods.MozOutlineWidth;
methods.MozOutlineOffset;
methods.MozPaddingEnd;
methods.MozPaddingStart;
methods.MozUserFocus;
methods.MozUserInput;
methods.MozUserModify;
methods.MozUserSelect;

// Webkit-only CSS style names. Copied from:
// https://svn.corp.google.com/v/chrome/trunk/third_party/WebKit/WebCore/css/CSSPropertyNames.in?view=markup
methods.WebkitAppearance;
methods.WebkitBackgroundClip;
methods.WebkitBackgroundComposite;
methods.WebkitBackgroundOrigin;
methods.WebkitBackgroundSize;
methods.WebkitBinding;
methods.WebkitBorderBottomLeftRadius;
methods.WebkitBorderBottomRightRadius;
methods.WebkitBorderFit;
methods.WebkitBorderHorizontalSpacing;
methods.WebkitBorderImage;
methods.WebkitBorderRadius;
methods.WebkitBorderTopLeftRadius;
methods.WebkitBorderTopRightRadius;
methods.WebkitBorderVerticalSpacing;
methods.WebkitBoxAlign;
methods.WebkitBoxDirection;
methods.WebkitBoxFlex;
methods.WebkitBoxFlexGroup;
methods.WebkitBoxLines;
methods.WebkitBoxOrdinalGroup;
methods.WebkitBoxOrient;
methods.WebkitBoxPack;
methods.WebkitBoxShadow;
methods.WebkitBoxSizing;
methods.WebkitColumnBreakAfter;
methods.WebkitColumnBreakBefore;
methods.WebkitColumnBreakInside;
methods.WebkitColumnCount;
methods.WebkitColumnGap;
methods.WebkitColumnRule;
methods.WebkitColumnRuleColor;
methods.WebkitColumnRuleStyle;
methods.WebkitColumnRuleWidth;
methods.WebkitColumnWidth;
methods.WebkitColumns;
methods.WebkitDashboardRegion;
methods.WebkitFontSizeDelta;
methods.WebkitHighlight;
methods.WebkitLineBreak;
methods.WebkitLineClamp;
methods.WebkitMarginBottomCollapse;
methods.WebkitMarginCollapse;
methods.WebkitMarginStart;
methods.WebkitMarginTopCollapse;
methods.WebkitMarquee;
methods.WebkitMarqueeDirection;
methods.WebkitMarqueeIncrement;
methods.WebkitMarqueeRepetition;
methods.WebkitMarqueeSpeed;
methods.WebkitMarqueeStyle;
methods.WebkitMatchNearestMailBlockquoteColor;
methods.WebkitNbspMode;
methods.WebkitPaddingStart;
methods.WebkitRtlOrdering;
methods.WebkitTextDecorationsInEffect;
methods.WebkitTextFillColor;
methods.WebkitTextSecurity;
methods.WebkitTextSizeAdjust;
methods.WebkitTextStroke;
methods.WebkitTextStrokeColor;
methods.WebkitTextStrokeWidth;
methods.WebkitTransform;
methods.WebkitTransformOrigin;
methods.WebkitTransformOriginX;
methods.WebkitTransformOriginY;
methods.WebkitTransition;
methods.WebkitTransitionDuration;
methods.WebkitTransitionProperty;
methods.WebkitTransitionRepeatCount;
methods.WebkitTransitionTimingFunction;
methods.WebkitUserDrag;
methods.WebkitUserModify;
methods.WebkitUserSelect;
