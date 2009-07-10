
// Helpers

function $(id) {
  return document.getElementById(id);
}

// TODO(arv): Remove these when classList is available in HTML5.
// https://bugs.webkit.org/show_bug.cgi?id=20709
function hasClass(el, name) {
  return el.nodeType == 1 && el.className.split(/\s+/).indexOf(name) != -1;
}

function addClass(el, name) {
  el.className += ' ' + name;
}

function removeClass(el, name) {
  var names = el.className.split(/\s+/);
  el.className = names.filter(function(n) {
    return name != n;
  }).join(' ');
}

function findAncestorByClass(el, className) {
  return findAncestor(el, function(el) {
    return hasClass(el, className);
  });
}

/**
 * Return the first ancestor for which the {@code predicate} returns true.
 * @param {Node} node The node to check.
 * @param {function(Node) : boolean} predicate The function that tests the
 *     nodes.
 * @return {Node} The found ancestor or null if not found.
 */
function findAncestor(node, predicate) {
  var last = false;
  while (node != null && !(last = predicate(node))) {
    node = node.parentNode;
  }
  return last ? node : null;
}

// WebKit does not have Node.prototype.swapNode
// https://bugs.webkit.org/show_bug.cgi?id=26525
function swapDomNodes(a, b) {
  var afterA = a.nextSibling;
  if (afterA == b) {
    swapDomNodes(b, a);
    return;
  }
  var aParent = a.parentNode;
  b.parentNode.replaceChild(a, b);
  aParent.insertBefore(b, afterA);
}

function bind(fn, selfObj, var_args) {
  var boundArgs = Array.prototype.slice.call(arguments, 2);
  return function() {
    var args = Array.prototype.slice.call(arguments);
    args.unshift.apply(args, boundArgs);
    return fn.apply(selfObj, args);
  }
}

var loading = true;
var mostVisitedData = [];
var gotMostVisited = false;
var gotShownSections = false;

function mostVisitedPages(data) {
  logEvent('received most visited pages');

  // We append the class name with the "filler" so that we can style fillers
  // differently.
  var maxItems = 8;
  data.length = Math.min(maxItems, data.length);
  var len = data.length;
  for (var i = len; i < maxItems; i++) {
    data[i] = {filler: true};
  }

  mostVisitedData = data;
  renderMostVisited(data);

  gotMostVisited = true;
  onDataLoaded();
}

function downloadsList(data) {
  logEvent('received downloads');
  data.length = Math.min(data.length, 5);
  processData('#download-items', data);
}

function recentlyClosedTabs(data) {
  logEvent('received recently closed tabs');
  data.length = Math.min(data.length, 5);
  processData('#tab-items', data);
}

function onShownSections(mask) {
  logEvent('received shown sections');
  if (mask != shownSections) {

    // Only invalidate most visited if needed.
    if ((mask & Section.THUMB) != (shownSections & Section.THUMB) ||
        (mask & Section.LIST) != (shownSections & Section.LIST)) {
      mostVisited.invalidate();
    }

    shownSections = mask;
    mostVisited.updateDisplayMode();
    layoutLowerSections();
    updateOptionMenu();
  }

  gotShownSections = true;
  onDataLoaded();
}

function saveShownSections() {
  chrome.send('setShownSections', [String(shownSections)]);
}

function tips(data) {
  logEvent('received tips data');
  data.length = Math.min(data.length, 5);
  processData('#tip-items', data);
}

// This global variable is used to skip parts of the DOM tree for the global
// jst processing done by the i18n.
var processing = false;

function processData(selector, data) {
  var output = document.querySelector(selector);

  // Wait until ready
  if (typeof JsEvalContext !== 'function' || !output) {
    logEvent('JsEvalContext is not yet available, ' + selector);
    document.addEventListener('DOMContentLoaded', function() {
      processData(selector, data);
    });
  } else {
    var d0 = Date.now();
    var input = new JsEvalContext(data);
    processing = true;
    jstProcess(input, output);
    processing = false;
    logEvent('processData: ' + selector + ', ' + (Date.now() - d0));
  }
}

function getThumbnailClassName(data) {
  return 'thumbnail-container' +
      (data.pinned ? ' pinned' : '') +
      (data.filler ? ' filler' : '');
}

function renderMostVisited(data) {
  var parent = $('most-visited');
  var children = parent.children;
  for (var i = 0; i < data.length; i++) {
    var d = data[i];
    var t = children[i];

    // If we have a filler continue
    var oldClassName = t.className;
    var newClassName = getThumbnailClassName(d);
    if (oldClassName != newClassName) {
      t.className = newClassName;
    }

    // No need to continue if this is a filler.
    if (newClassName == 'thumbnail-container filler') {
      continue;
    }

    t.href = d.url;
    t.querySelector('.pin').title = localStrings.getString(d.pinned ?
        'unpinthumbnailtooltip' : 'pinthumbnailtooltip');
    t.querySelector('.remove').title =
        localStrings.getString('removethumbnailtooltip');

    // There was some concern that a malformed malicious URL could cause an XSS
    // attack but setting style.backgroundImage = 'url(javascript:...)' does
    // not execute the JavaScript in WebKit.
    t.querySelector('.thumbnail-wrapper').style.backgroundImage =
        'url("chrome://thumb/' + d.url + '")';
    var titleDiv = t.querySelector('.title > div');
    titleDiv.xtitle = titleDiv.textContent = d.title;
    titleDiv.style.backgroundImage = 'url("chrome://favicon/' + d.url + '")';
    titleDiv.dir = d.direction;
  }
}

/**
 * Calls chrome.send with a callback and restores the original afterwards.
 */
function chromeSend(name, params, callbackName, callback) {
  var old = global[callbackName];
  global[callbackName] = function() {
    // restore
    global[callbackName] = old;

    var args = Array.prototype.slice.call(arguments);
    return callback.apply(global, args);
  };
  chrome.send(name, params);
}

function useSmallGrid() {
  return window.innerWidth <= 940;
}

var LayoutMode = {
  SMALL: 1,
  NORMAL: 2
};

var layoutMode = useSmallGrid() ? LayoutMode.SMALL : LayoutMode.NORMAL;

function handleWindowResize() {
  if (window.innerWidth < 10) {
    // We're probably a background tab, so don't do anything.
    return;
  }

  var oldLayoutMode = layoutMode;
  layoutMode = useSmallGrid() ? LayoutMode.SMALL : LayoutMode.NORMAL

  if (layoutMode != oldLayoutMode){
    mostVisited.invalidate();
    mostVisited.layout();
    layoutLowerSections();
  }
}

/**
 * Bitmask for the different UI sections.
 * This matches the Section enum in ../dom_ui/shown_sections_handler.h
 * @enum {number}
 */
var Section = {
  THUMB: 1,
  LIST: 2,
  RECENT: 4,
  TIPS: 8
};

var shownSections = Section.THUMB | Section.RECENT | Section.TIPS;

function showSection(section) {
  if (!(section & shownSections)) {
    shownSections |= section;

    // THUMBS and LIST are mutually exclusive.
    if (section == Section.THUMB) {
      // hide LIST
      shownSections &= ~Section.LIST;
      mostVisited.invalidate();
    } else if (section == Section.LIST) {
      // hide THUMB
      shownSections &= ~Section.THUMB;
      mostVisited.invalidate();
    } else {
      notifyLowerSectionForChange(section, false);
      layoutLowerSections();
    }

    updateOptionMenu();
    mostVisited.updateDisplayMode();
    mostVisited.layout();
  }
}

function hideSection(section) {
  if (section & shownSections) {
    shownSections &= ~section;

    if (section & Section.THUMB || section & Section.LIST) {
      mostVisited.invalidate();
    }

    if (section & Section.RECENT|| section & Section.TIPS) {
      notifyLowerSectionForChange(section, true);
      layoutLowerSections();
    }

    updateOptionMenu();
    mostVisited.updateDisplayMode();
    mostVisited.layout();
  }
}

function notifyLowerSectionForChange(section, large) {
  // Notify recent and tips if they need to display more data.
  if (section == Section.RECENT || section == Section.TIPS) {
    // we are hiding one of them so if the other one is visible it is now
    // {@code large}.
    if (shownSections & Section.RECENT) {
      recentChangedSize(large);
    } else if (shownSections & Section.TIPS) {
      tipsChangedSize(large);
    }
  }
}

var mostVisited = {
  getItem: function(el) {
    return findAncestorByClass(el, 'thumbnail-container');
  },

  getHref: function(el) {
    return el.href;
  },

  togglePinned: function(el) {
    var index = this.getThumbnailIndex(el);
    var data = mostVisitedData[index];
    if (data.pinned) {
      removeClass(el, 'pinned');
      chrome.send('removePinnedURL', [data.url]);
    } else {
      addClass(el, 'pinned');
      chrome.send('addPinnedURL', [data.url, data.title, String(index)]);
    }
    data.pinned = !data.pinned;
    // Update tooltip
    el.querySelector('.pin').title = localStrings.getString(data.pinned ?
        'unpinthumbnailtooltip' : 'pinthumbnailtooltip');
  },

  getThumbnailIndex: function(el) {
    var nodes = el.parentNode.querySelectorAll('.thumbnail-container');
    return Array.prototype.indexOf.call(nodes, el);
  },

  swapPosition: function(source, destination) {
    var nodes = source.parentNode.querySelectorAll('.thumbnail-container');
    var sourceIndex = this.getThumbnailIndex(source);
    var destinationIndex = this.getThumbnailIndex(destination);
    swapDomNodes(source, destination);

    var sourceData = mostVisitedData[sourceIndex];
    chrome.send('addPinnedURL', [sourceData.url, sourceData.title,
                                 String(destinationIndex)]);
    sourceData.pinned = true;
    addClass(source, 'pinned');
    var destinationData = mostVisitedData[destinationIndex];
    // Only update the destination if it was pinned before.
    if (destinationData.pinned) {
      chrome.send('addPinnedURL', [destinationData.url, destinationData.title,
                                   String(sourceIndex)]);
    }
    mostVisitedData[destinationIndex] = sourceData;
    mostVisitedData[sourceIndex] = destinationData;
  },

  blacklist: function(el) {
    var self = this;
    var url = this.getHref(el);
    chrome.send('blacklistURLFromMostVisited', [url]);

    addClass(el, 'hide');

    // Find the old item.
    var oldUrls = {};
    var oldIndex = -1;
    var oldItem;
    for (var i = 0; i < mostVisitedData.length; i++) {
      if (mostVisitedData[i].url == url) {
        oldItem = mostVisitedData[i];
        oldIndex = i;
      }
      oldUrls[mostVisitedData[i].url] = true;
    }

    // Send 'getMostVisitedPages' with a callback since we want to find the new
    // page and add that in the place of the removed page.
    chromeSend('getMostVisited', [], 'mostVisitedPages', function(data) {
      // Find new item.
      var newItem;
      for (var i = 0; i < data.length; i++) {
        if (!(data[i].url in oldUrls)) {
          newItem = data[i];
          break;
        }
      }

      if (!newItem) {
        newItem = {filler: true};
      }

      // Replace old item with new item in the mostVisitedData array.
      if (oldIndex != -1) {
        mostVisitedData.splice(oldIndex, 1, newItem);
        mostVisitedPages(mostVisitedData);
        addClass(el, 'fade-in');
      }

      var text = localStrings.formatString('thumbnailremovednotification',
                                           oldItem.title);
      var actionText = localStrings.getString('undothumbnailremove');

      // Show notification and add undo callback function.
      var wasPinned = oldItem.pinned;
      showNotification(text, actionText, function() {
        self.removeFromBlackList(url);
        if (wasPinned) {
          chromeSend('addPinnedURL', [url, oldItem.title, String(oldIndex)]);
        }
        chrome.send('getMostVisited');
      });
    });
  },

  removeFromBlackList: function(url) {
    chrome.send('removeURLsFromMostVisitedBlacklist', [url]);
  },

  clearAllBlacklisted: function() {
    chrome.send('clearMostVisitedURLsBlacklist', []);
  },

  updateDisplayMode: function() {
    if (!this.dirty_) {
      return;
    }

    var thumbCheckbox = $('thumb-checkbox');
    var listCheckbox = $('list-checkbox');
    var mostVisitedElement = $('most-visited');

    if (shownSections & Section.THUMB) {
      thumbCheckbox.checked = true;
      listCheckbox.checked = false;
      removeClass(mostVisitedElement, 'list');
    } else if (shownSections & Section.LIST) {
      thumbCheckbox.checked = false;
      listCheckbox.checked = true;
      addClass(mostVisitedElement, 'list');
    } else {
      thumbCheckbox.checked = false;
      listCheckbox.checked = false;
    }

    thumbCheckbox.title = localStrings.getString(
        shownSections & Section.THUMB ? 'hidethumbnails' : 'showthumbnails');
    listCheckbox.title = localStrings.getString(
        shownSections & Section.LIST ? 'hidelist' : 'showlist');
  },

  dirty_: false,

  invalidate: function() {
    this.dirty_ = true;
  },

  layout: function() {
    if (!this.dirty_) {
      return;
    }
    var d0 = Date.now();
    var mostVisitedElement = $('most-visited');
    var thumbnails = mostVisitedElement.children;

    var small = useSmallGrid();

    var cols = 4;
    var rows = 2;
    var marginWidth = 10;
    var marginHeight = 7;
    var borderWidth = 4;
    var thumbWidth = small ? 150 : 212;
    var thumbHeight = small ? 93 : 132;
    var w = thumbWidth + 2 * borderWidth + 2 * marginWidth;
    var h = thumbHeight + 40 + 2 * marginHeight;
    var sumWidth = cols * w  - 2 * marginWidth;
    var sumHeight = rows * h;
    var opacity = 1;

    if (shownSections & Section.LIST) {
      w = (sumWidth + 2 * marginWidth) / 2;
      h = 45;
      rows = 4;
      cols = 2;
      sumHeight = rows * h;
      addClass(mostVisitedElement, 'list');
    } else if (shownSections & Section.THUMB) {
      removeClass(mostVisitedElement, 'list');
      } else {
      sumHeight = 0;
      opacity = 0;
    }

    mostVisitedElement.style.height = sumHeight + 'px';
    mostVisitedElement.style.opacity = opacity;
    // We set overflow to hidden so that the most visited element does not
    // "leak" when we hide and show it.
    if (!opacity) {
      mostVisitedElement.style.overflow = 'hidden';
    }

    var rtl = document.documentElement.dir == 'rtl';

    if (shownSections & Section.THUMB || shownSections & Section.LIST) {
      for (var i = 0; i < thumbnails.length; i++) {
        var t = thumbnails[i];

        // Remove temporary ID that was used during startup layout.
        t.id = '';

        var row, col;
        if (shownSections & Section.THUMB) {
          row = Math.floor(i / cols);
          col = i % cols;
        } else {
          col = Math.floor(i / rows);
          row = i % rows;
        }

        if (shownSections & Section.THUMB) {
          t.style.left = (rtl ?
              sumWidth - col * w - thumbWidth - 2 * borderWidth :
              col * w) + 'px';
        } else {
          t.style.left = (rtl ?
              sumWidth - col * w - w + 2 * marginWidth :
              col * w) + 'px';
        }
        t.style.top = row * h + 'px';

        if (shownSections & Section.LIST) {
          t.style.width = w - 2 * marginWidth + 'px';
        } else {
          t.style.width = '';
        }
      }
    }

    afterTransition(function() {
      // Only set overflow to visible if the element is shown.
      if (opacity) {
        mostVisitedElement.style.overflow = '';
      }
    });

    this.dirty_ = false;

    logEvent('mostVisited.layout: ' + (Date.now() - d0));
  }
};

function recentChangedSize(large) {
  // TODO(arv): Implement
}

function tipsChangedSize(large) {
  // TODO(arv): Implement
}

// Recent activities

function layoutLowerSections() {
  // This lower sections are inline blocks so all we need to do is to set the
  // width and opacity.
  var lowerSectionsElement = $('lower-sections');
  var recentElement = $('recent-activities');
  var tipsElement = $('tips');
  var spacer = recentElement.nextElementSibling;

  var totalWidth = useSmallGrid() ? 692 : 940;
  var spacing = 20;
  var rtl = document.documentElement.dir == 'rtl';

  var recentShown = shownSections & Section.RECENT;
  var tipsShown = shownSections & Section.TIPS;

  if (recentShown || tipsShown) {
    lowerSectionsElement.style.height = '198px';
    lowerSectionsElement.style.opacity = '';
  } else {
    lowerSectionsElement.style.height = lowerSectionsElement.style.opacity = 0;
  }

  if (recentShown && tipsShown) {
    var w = (totalWidth - spacing) / 2;
    recentElement.style.width = tipsElement.style.width = w + 'px'
    recentElement.style.opacity = tipsElement.style.opacity = '';
    spacer.style.width = spacing + 'px';
  } else if (recentShown) {
    recentElement.style.width = totalWidth + 'px';
    recentElement.style.opacity = '';
    tipsElement.style.width =
        tipsElement.style.opacity = 0;
    spacer.style.width = 0;
  } else if (tipsShown) {
    tipsElement.style.width = totalWidth + 'px';
    tipsElement.style.opacity = '';
    recentElement.style.width = recentElement.style.opacity = 0;
    spacer.style.width = 0;
  }
}

/**
 * Returns the text used for a recently closed window.
 * @param {number} numTabs Number of tabs in the window.
 * @return {string} The text to use.
 */
function formatTabsText(numTabs) {
  if (numTabs == 1)
    return localStrings.getString('closedwindowsingle');
  return localStrings.formatString('closedwindowmultiple', numTabs);
}

/**
 * We need both most visited and the shown sections to be considered loaded.
 * @return {boolean}
 */
function onDataLoaded() {
  if (gotMostVisited && gotShownSections) {
    mostVisited.layout();
    loading = false;
    // Remove class name in a timeout so that changes done in this JS thread are
    // not animated.
    window.setTimeout(function() {
      removeClass(document.body, 'loading');
    }, 1);
  }
}

// Theme related

function themeChanged() {
  $('themecss').href = 'chrome://theme/css/newtab.css?' + Date.now();
  updateAttribution();
}

function updateAttribution() {
  // TODO(arv): Implement
  //$('attribution-img').src = 'chrome://theme/theme_ntp_attribution?' +
  //    Date.now();
}

function bookmarkBarAttached() {
  document.documentElement.setAttribute("bookmarkbarattached", "true");
}

function bookmarkBarDetached() {
  document.documentElement.setAttribute("bookmarkbarattached", "false");
}

function viewLog() {
  var lines = [];
  var start = log[0][1];

  for (var i = 0; i < log.length; i++) {
    lines.push((log[i][1] - start) + ': ' + log[i][0]);
  }

  console.log(lines.join('\n'));
}

// Updates the visibility of the menu items.
function updateOptionMenu() {
  var menuItems = $('option-menu').children;
  for (var i = 0; i < menuItems.length; i++) {
    var item = menuItems[i];
    var section = Section[item.getAttribute('section')];
    var show = item.getAttribute('show') == 'true';
    // Hide show items if already shown. Hide hide items if already hidden.
    var hideMenuItem = show == !!(shownSections & section);
    item.style.display = hideMenuItem ? 'none' : '';
  }
}

// We apply the size class here so that we don't trigger layout animations
// onload.

handleWindowResize();

var localStrings = new LocalStrings();

///////////////////////////////////////////////////////////////////////////////
// Things we know are not needed at startup go below here

function afterTransition(f) {
  if (loading) {
    // Make sure we do not use a timer during load since it slows down the UI.
    f();
  } else {
    // The duration of all transitions are 500ms
    window.setTimeout(f, 500);
  }
}

// Notification

function showNotification(text, actionText, f) {
  var notificationElement = $('notification');
  var actionLink = notificationElement.querySelector('.link');
  notificationElement.firstElementChild.textContent = text;

  actionLink.textContent = actionText;
  actionLink.onclick = function() {
    f();
    removeClass(notificationElement, 'show');
    // Since we have a :hover rule to not hide the notification banner when the
    // mouse is over we need force it to hide. We remove the hide class after
    // a short timeout to allow the banner to be shown again.
    addClass(notificationElement, 'hide');
    afterTransition(function() {
      removeClass(notificationElement, 'hide');
    })
  };
  addClass(notificationElement, 'show');
  window.setTimeout(function() {
    removeClass(notificationElement, 'show');
  }, 10000);
}

/**
 * This handles the option menu.
 * @param {Element} button The button element.
 * @param {Element} menu The menu element.
 * @constructor
 */
function OptionMenu(button, menu) {
  this.button = button;
  this.menu = menu;
  this.button.onmousedown = bind(this.handleMouseDown, this);
  this.button.onkeydown = bind(this.handleKeyDown, this);
  this.boundHideMenu_ = bind(this.hide, this);
  this.boundMaybeHide_ = bind(this.maybeHide_, this);
  this.menu.onmouseover = bind(this.handleMouseOver, this);
  this.menu.onmouseout = bind(this.handleMouseOut, this);
  this.menu.onmouseup = bind(this.handleMouseUp, this);
}

OptionMenu.prototype = {
  show: function() {
    windowMenu.hide();

    this.menu.style.display = 'block';
    this.button.focus();

    // Listen to document and window events so that we hide the menu when the
    // user clicks outside the menu or tabs away or the whole window is blurred.
    document.addEventListener('focus', this.boundMaybeHide_, true);
    document.addEventListener('mousedown', this.boundMaybeHide_, true);
  },

  hide: function() {
    this.menu.style.display = 'none';
    this.setSelectedIndex(-1);

    document.removeEventListener('focus', this.boundMaybeHide_, true);
    document.removeEventListener('mousedown', this.boundMaybeHide_, true);
  },

  isShown: function() {
    return this.menu.style.display == 'block';
  },

  /**
   * Callback for document mousedown and focus. It checks if the user tried to
   * navigate to a different element on the page and if so hides the menu.
   * @param {Event} e The mouse or focus event.
   * @private
   */
  maybeHide_: function(e) {
    if (!this.menu.contains(e.target) && !this.button.contains(e.target)) {
      this.hide();
    }
  },

  handleMouseDown: function(e) {
    if (this.isShown()) {
      this.hide();
    } else {
      this.show();
    }
  },

  handleMouseOver: function(e) {
    var el = e.target;
    var index = Array.prototype.indexOf.call(this.menu.children, el);
    this.setSelectedIndex(index);
  },

  handleMouseOut: function(e) {
    this.setSelectedIndex(-1);
  },

  handleMouseUp: function(e) {
    var item = this.getSelectedItem();
    if (item) {
      this.executeItem(item);
    }
  },

  handleKeyDown: function(e) {
    var item = this.getSelectedItem();

    var self = this;
    function selectNextVisible(m) {
      var children = self.menu.children;
      var len = children.length;
      var i = self.selectedIndex_;
      if (i == -1 && m == -1) {
        // Edge case when we need to go the last item fisrt.
        i = 0;
      }
      while (true) {
        i = (i + m + len) % len;
        item = children[i];
        if (item && item.style.display != 'none') {
          break;
        }
      }
      if (item) {
        self.setSelectedIndex(i);
      }
    }

    switch (e.keyIdentifier) {
      case 'Down':
        if (!this.isShown()) {
          this.show();
        }
        selectNextVisible(1);
        e.preventDefault();
        break;
      case 'Up':
        if (!this.isShown()) {
          this.show();
        }
        selectNextVisible(-1);
        e.preventDefault();
        break;
      case 'Esc':
      case 'U+001B': // Maybe this is remote desktop playing a prank?
        this.hide();
        break;
      case 'Enter':
      case 'U+0020': // Space
        if (this.isShown()) {
          if (item) {
            this.executeItem(item);
          } else {
            this.hide();
          }
        } else {
          this.show();
        }
        e.preventDefault();
        break;
    }
  },

  selectedIndex_: -1,
  setSelectedIndex: function(i) {
    if (i != this.selectedIndex_) {
      var items = this.menu.children;
      var oldItem = items[this.selectedIndex_];
      if (oldItem) {
        oldItem.removeAttribute('selected');
      }
      var newItem = items[i];
      if (newItem) {
        newItem.setAttribute('selected', 'selected');
      }
      this.selectedIndex_ = i;
    }
  },

  getSelectedItem: function() {
    return this.menu.children[this.selectedIndex_] || null;
  },

  executeItem: function(item) {
    var section = Section[item.getAttribute('section')];
    var show = item.getAttribute('show') == 'true';
    if (show) {
      showSection(section);
    } else {
      hideSection(section);
    }

    this.hide();
    saveShownSections();
  }
};

var optionMenu = new OptionMenu($('option-button'), $('option-menu'));

$('most-visited').addEventListener('click', function(e) {
  var target = e.target;
  if (hasClass(target, 'pin')) {
    mostVisited.togglePinned(mostVisited.getItem(target));
    e.preventDefault();
  } else if (hasClass(target, 'remove')) {
    mostVisited.blacklist(mostVisited.getItem(target));
    e.preventDefault();
  }
});

function handleIfEnterKey(f) {
  return function(e) {
    if (e.keyIdentifier == 'Enter') {
      f(e);
    }
  };
}

$('downloads').addEventListener('click', maybeOpenFile);
$('downloads').addEventListener('keydown', handleIfEnterKey(maybeOpenFile));

function maybeOpenFile(e) {
  var el = findAncestor(e.target, function(el) {
    return el.fileId !== undefined;
  });
  if (el) {
    chrome.send('openFile', [String(el.fileId)]);
    e.preventDefault();
  }
}

var recentTabs = $('recent-tabs');
recentTabs.addEventListener('click', maybeReopenTab);
recentTabs.addEventListener('keydown', handleIfEnterKey(maybeReopenTab));

function maybeReopenTab(e) {
  var el = findAncestor(e.target, function(el) {
    return el.sessionId !== undefined;
  });
  if (el) {
    chrome.send('reopenTab', [String(el.sessionId)]);
    e.preventDefault();
  }
}

recentTabs.addEventListener('mouseover', maybeShowWindowMenu);
recentTabs.addEventListener('focus', maybeShowWindowMenu, true);


function maybeShowWindowMenu(e) {
  var el = findAncestor(e.target, function(el) {
    return el.tabItems !== undefined;
  });
  if (el) {
    windowMenu.show(e, el, el.tabItems);
  }
}

/**
 * This object represents a window/tooltip representing a closed window. It is
 * shown when hovering over a closed window item or when the item is focused. It
 * gets hidden when blurred or when mousing out of the menu or the item.
 * @param {Element} menuEl The element to use as the menu.
 * @constructor
 */
function WindowMenu(menuEl) {
  this.menuEl = menuEl;
  var self = this;
  this.boundHide_ = bind(this.hide, this);
  menuEl.onmouseover = function() {
    clearTimeout(self.timer);
  };
  menuEl.onmouseout = this.boundHide_;
}

WindowMenu.prototype = {
  timer: 0,
  show: function(e, linkEl, tabs) {
    optionMenu.hide();

    clearTimeout(this.timer);
    processData('#window-menu', tabs);
    var rect = linkEl.getBoundingClientRect();
    var bodyRect = document.body.getBoundingClientRect()
    var rtl = document.documentElement.dir == 'rtl';

    this.menuEl.style.display = 'block';
    this.menuEl.style.left = (rtl ?
        rect.left + bodyRect.left + rect.width - this.menuEl.offsetWidth :
        rect.left + bodyRect.left) + 'px';
    this.menuEl.style.top = rect.top + bodyRect.top + rect.height + 'px';

    if (e.type == 'focus') {
      linkEl.onblur = this.boundHide_;
    } else { // mouseover
      linkEl.onmouseout = this.boundHide_;
    }
  },
  hide: function() {
    // Delay before hiding.
    var self = this;
    this.timer = setTimeout(function() {
      self.menuEl.style.display  = 'none';
    }, 100);
  }
};

var windowMenu = new WindowMenu($('window-menu'));

function getCheckboxHandler(section) {
  return function(e) {
    if (e.type == 'keydown') {
      if (e.keyIdentifier == 'Enter') {
        e.target.checked = !e.target.checked;
      } else {
        return;
      }
    }
    if (e.target.checked) {
      showSection(section);
    } else {
      hideSection(section);
    }
    saveShownSections();
  }
}

$('thumb-checkbox').addEventListener('change',
                                     getCheckboxHandler(Section.THUMB));
$('thumb-checkbox').addEventListener('keydown',
                                     getCheckboxHandler(Section.THUMB));
$('list-checkbox').addEventListener('change',
                                    getCheckboxHandler(Section.LIST));
$('list-checkbox').addEventListener('keydown',
                                    getCheckboxHandler(Section.LIST));

window.addEventListener('load', bind(logEvent, global, 'onload fired'));
window.addEventListener('load', onDataLoaded);
window.addEventListener('resize', handleWindowResize);
document.addEventListener('DOMContentLoaded', bind(logEvent, global,
                                                   'domcontentloaded fired'));

function hideAllMenus() {
  windowMenu.hide();
  optionMenu.hide();
}

window.addEventListener('blur', hideAllMenus);
window.addEventListener('keydown', function(e) {
  if (e.keyIdentifier == 'Alt' || e.keyIdentifier == 'Meta') {
    hideAllMenus();
  }
}, true);

// Tooltip for elements that have text that overflows.
document.addEventListener('mouseover', function(e) {
  // We don't want to do this while we are dragging because it makes things very
  // janky
  if (dnd.dragItem) {
    return;
  }

  var el = findAncestor(e.target, function(el) {
    return el.xtitle;
  });
  if (el && el.xtitle != el.title) {
    if (el.scrollWidth > el.clientWidth) {
      el.title = el.xtitle;
    } else {
      el.title = '';
    }
  }
});

// DnD

var dnd = {
  currentOverItem: null,
  dragItem: null,
  startX: 0,
  startY: 0,
  startScreenX: 0,
  startScreenY: 0,
  dragEndTimer: null,

  handleDragStart: function(e) {
    var thumbnail = mostVisited.getItem(e.target);
    if (thumbnail) {
      e.dataTransfer.setData('text/uri-list', mostVisited.getHref(thumbnail));
      this.dragItem = thumbnail;
      addClass(this.dragItem, 'dragging');
      this.dragItem.style.zIndex = 2;
    }
  },

  handleDragEnter: function(e) {
    this.currentOverItem = mostVisited.getItem(e.target);
    if (this.canDropOnElement(this.currentOverItem)) {
      e.preventDefault();
    }
  },

  handleDragOver: function(e) {
    var item = mostVisited.getItem(e.target);
    if (this.canDropOnElement(item)) {
      e.preventDefault();
    }
  },

  handleDragLeave: function(e) {
    var item = mostVisited.getItem(e.target);
    if (item) {
      e.preventDefault();
    }

    this.currentOverItem = null;
  },

  handleDrop: function(e) {
    var dropTarget = mostVisited.getItem(e.target);
    if (this.canDropOnElement(dropTarget)) {
      dropTarget.style.zIndex = 1;
      mostVisited.swapPosition(this.dragItem, dropTarget);
      mostVisited.invalidate();
      mostVisited.layout();
      e.preventDefault();
      if (this.dragEndTimer) {
        window.clearTimeout(this.dragEndTimer);
        this.dragEndTimer = null;
      }
      afterTransition(function() {
        dropTarget.style.zIndex = '';
      });
    }
  },

  handleDragEnd: function(e) {
    // WebKit fires dragend before drop.
    var dragItem = this.dragItem;
    if (dragItem) {
      dragItem.style.pointerEvents = '';
      removeClass(dragItem, 'dragging');

      afterTransition(function() {
        // Delay resetting zIndex to let the animation finish.
        dragItem.style.zIndex = '';
        // Same for overflow.
        dragItem.parentNode.style.overflow = '';
      });
      var self = this;
      this.dragEndTimer = window.setTimeout(function() {
        // These things needto happen after the drop event.
        mostVisited.invalidate();
        mostVisited.layout();
        self.dragItem = null;
      }, 10);

    }
  },

  handleDrag: function(e) {
    var item = mostVisited.getItem(e.target);
    var rect = document.querySelector('#most-visited').getBoundingClientRect();
    item.style.pointerEvents = 'none';

    item.style.left = this.startX + e.screenX - this.startScreenX + 'px';
    item.style.top = this.startY + e.screenY - this.startScreenY + 'px';
  },

  // We listen to mousedown to get the relative position of the cursor for dnd.
  handleMouseDown: function(e) {
    var item = mostVisited.getItem(e.target);
    if (item) {
      this.startX = item.offsetLeft;
      this.startY = item.offsetTop;
      this.startScreenX = e.screenX;
      this.startScreenY = e.screenY;
    }
  },

  canDropOnElement: function(el) {
    return this.dragItem && el && hasClass(el, 'thumbnail-container') &&
        !hasClass(el, 'filler');
  },

  init: function() {
    var el = $('most-visited');
    el.addEventListener('dragstart', bind(this.handleDragStart, this));
    el.addEventListener('dragenter', bind(this.handleDragEnter, this));
    el.addEventListener('dragover', bind(this.handleDragOver, this));
    el.addEventListener('dragleave', bind(this.handleDragLeave, this));
    el.addEventListener('drop', bind(this.handleDrop, this));
    el.addEventListener('dragend', bind(this.handleDragEnd, this));
    el.addEventListener('drag', bind(this.handleDrag, this));
    el.addEventListener('mousedown', bind(this.handleMouseDown, this));
  }
};

dnd.init();
