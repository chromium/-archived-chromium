
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
  layoutMostVisited();

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

function onShownSections(m) {
  logEvent('received shown sections');
  setShownSections(m);
  gotShownSections = true;
  onDataLoaded();
}

function saveShownSections() {
  chrome.send('setShownSections', [String(shownSections)]);
}

function layoutMostVisited() {
  var d0 = Date.now();
  var mostVisitedElement = $('most-visited');
  var thumbnails = mostVisitedElement.querySelectorAll('.thumbnail-container');

  if (thumbnails.length < 8) {
    return;
  }

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
  mostVisitedElement.style.overflow = 'hidden';

  var rtl = document.documentElement.dir == 'rtl';

  if (shownSections & Section.THUMB || shownSections & Section.LIST) {
    for (var i = 0; i < thumbnails.length; i++) {
      var t = thumbnails[i];

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
    mostVisitedElement.style.overflow = '';
  });

  logEvent('layoutMostVisited: ' + (Date.now() - d0));
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

var thumbnailTemplate;

function getThumbnailClassName(data) {
  return 'thumbnail-container' +
      (data.pinned ? ' pinned' : '') +
      (data.filler ? ' filler' : '');
}

function renderMostVisited(data) {
  var parent = $('most-visited');
  if (!thumbnailTemplate) {
    thumbnailTemplate = $('thumbnail-template');
    thumbnailTemplate.parentNode.removeChild(thumbnailTemplate);
  }

  var children = parent.children;
  for (var i = 0; i < data.length; i++) {
    var d = data[i];
    var reuse = !!children[i];
    var t = children[i] || thumbnailTemplate.cloneNode(true);
    t.style.display = '';
    t.className = getThumbnailClassName(d);
    t.title = d.title;
    t.href = d.url;
    t.querySelector('.edit-link').textContent =
        localStrings.getString('editthumbnail');

    // There was some concern that a malformed malicious URL could cause an XSS
    // attack but setting style.backgroundImage = 'url(javascript:...)' does
    // not execute the JavaScript in WebKit.
    t.querySelector('.thumbnail-wrapper').style.backgroundImage =
        'url(chrome://thumb/' + d.url + ')';
    var titleDiv = t.querySelector('.title > div');
    titleDiv.textContent = d.title;
    titleDiv.style.backgroundImage = 'url(chrome://favicon/' + d.url + ')';
    titleDiv.dir = d.direction;
    if (!reuse) {
      parent.appendChild(t);
    }
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
  return document.body.clientWidth <= 940;
}

function handleWindowResize(e, opt_noUpdate) {
  var body = document.body;
  if (!body || body.clientWidth < 10) {
    // We're probably a background tab, so don't do anything.
    return;
  }

  var hasSmallClass = hasClass(body, 'small');
  if (hasSmallClass && !useSmallGrid()) {
    removeClass(body, 'small');
    if (!opt_noUpdate) {
      layoutMostVisited();
      layoutLowerSections();
    }
  } else if (!hasSmallClass && useSmallGrid()) {
    addClass(body, 'small');
    if (!opt_noUpdate) {
      layoutMostVisited();
      layoutLowerSections();
    }
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
  RECOMMENDATIONS: 8
};

var shownSections = Section.RECENT | Section.RECOMMENDATIONS;

function showSection(section) {
  if (!(section & shownSections)) {
    // THUMBS and LIST are mutually exclusive.
    if (section == Section.THUMB) {
      hideSection(Section.LIST);
    } else if (section == Section.LIST) {
      hideSection(Section.THUMB);
    }

    shownSections |= section;
    notifyLowerSectionForChange(section, false);

    mostVisited.updateDisplayMode();
    layoutMostVisited();
    updateOptionMenu();
    layoutLowerSections();
  }
}

function hideSection(section) {
  if (section & shownSections) {
    shownSections &= ~section;
    notifyLowerSectionForChange(section, true);

    mostVisited.updateDisplayMode();
    layoutMostVisited();
    updateOptionMenu();
    layoutLowerSections();
  }
}

function notifyLowerSectionForChange(section, large) {
  // Notify recent and recommendations if they need to display more data.
  if (section == Section.RECENT || section == Section.RECOMMENDATIONS) {
    // we are hiding one of them so if the other one is visible it is now
    // {@code large}.
    if (shownSections & Section.RECENT) {
      recentChangedSize(large);
    } else if (shownSections & Section.RECOMMENDATIONS) {
      recommendationsChangedSize(large);
    }
  }
}

/**
 * This is called when we get the shown sections pref from the backend.
 */
function setShownSections(mask) {
  if (mask != shownSections) {
    shownSections = mask;
    mostVisited.updateDisplayMode();
    layoutMostVisited();
    layoutLowerSections();
    updateOptionMenu();
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
  }
};

function recentChangedSize(large) {
  // TODO(arv): Implement
}

function recommendationsChangedSize(large) {
  // TODO(arv): Implement
}

// Recent activities

function layoutLowerSections() {
  // This lower sections are inline blocks so all we need to do is to set the
  // width and opacity.
  var lowerSectionsElement = $('lower-sections');
  var recentElement = $('recent-activities');
  var recommendationsElement = $('recommendations');
  var spacer = recentElement.nextElementSibling;

  var totalWidth = useSmallGrid() ? 692 : 940;
  var spacing = 20;
  var rtl = document.documentElement.dir == 'rtl';

  var recentShown = shownSections & Section.RECENT;
  var recommendationsShown = shownSections & Section.RECOMMENDATIONS;

  if (recentShown || recommendationsShown) {
    lowerSectionsElement.style.height = '175px'
    lowerSectionsElement.style.opacity = '';
  } else {
    lowerSectionsElement.style.height = lowerSectionsElement.style.opacity = 0;
  }

  if (recentShown && recommendationsShown) {
    var w = (totalWidth - spacing) / 2;
    recentElement.style.width = recommendationsElement.style.width = w + 'px'
    recentElement.style.opacity = recommendationsElement.style.opacity = '';
    spacer.style.width = spacing + 'px';
  } else if (recentShown) {
    recentElement.style.width = totalWidth + 'px';
    recentElement.style.opacity = '';
    recommendationsElement.style.width =
        recommendationsElement.style.opacity = 0;
    spacer.style.width = 0;
  } else if (recommendationsShown) {
    recommendationsElement.style.width = totalWidth + 'px';
    recommendationsElement.style.opacity = '';
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
    // Remove class name in a timeout so that changes done in this JS thread are
    // not animated.
    window.setTimeout(function() {
      removeClass(document.body, 'loading');
    }, 10);
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

handleWindowResize(null, true);

var localStrings = new LocalStrings();

///////////////////////////////////////////////////////////////////////////////
// Things we know are not needed at startup go below here

// Notification

function afterTransition(f) {
  // The duration of all transitions are 500ms
  window.setTimeout(f, 500);
}

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

// Options menu
// TODO(arv): Keyboard navigation of the menu items.

function showMenu(button, menu) {
  function hide() {
    menu.style.display = '';
    menu.removeEventListener('blur', hide);
    window.removeEventListener('blur', hide);
  };
  menu.addEventListener('blur', hide);
  window.addEventListener('blur', hide);
  menu.style.display = 'block';
  menu.focus();
}

$('option-button').addEventListener('click', function(e) {
  showMenu(this, $('option-menu'));
});

$('option-menu').addEventListener('click', function(e) {
  var section = Section[e.target.getAttribute('section')];
  var show = e.target.getAttribute('show') == 'true';
  if (show) {
    showSection(section);
  } else {
    hideSection(section);
  }

  // Hide menu now.
  this.style.display = 'none';

  layoutLowerSections();
  mostVisited.updateDisplayMode();
  layoutMostVisited();

  saveShownSections();
});

$('most-visited').addEventListener('click', function(e) {
  var target = e.target;
  if (hasClass(target, 'pin')) {
    mostVisited.togglePinned(mostVisited.getItem(target));
    e.preventDefault();
  } else if (hasClass(target, 'remove')) {
    mostVisited.blacklist(mostVisited.getItem(target));
    e.preventDefault();
  } else if (hasClass(target, 'edit-link')) {
    alert('Not implemented yet')
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
recentTabs.addEventListener('mouseout', maybeHideWindowMenu);
recentTabs.addEventListener('blur', maybeHideWindowMenu, true);

function maybeShowWindowMenu(e) {
  var el = findAncestor(e.target, function(el) {
    return el.tabItems !== undefined;
  });
  if (el) {
    showWindowMenu(el, el.tabItems);
  }
}

function maybeHideWindowMenu(e) {
  var el = findAncestor(e.target, function(el) {
    return el.tabItems !== undefined;
  });
  if (el) {
    $('window-menu').style.display  = 'none';
  }
}

function showWindowMenu(el, tabs) {
  var menuEl = $('window-menu');
  processData('#window-menu', tabs);
  var rect = el.getBoundingClientRect();
  var bodyRect = document.body.getBoundingClientRect()
  var rtl = document.documentElement.dir == 'rtl';

  menuEl.style.display = 'block';
  menuEl.style.left = (rtl ?
      rect.left + bodyRect.left + rect.width - menuEl.offsetWidth :
      rect.left + bodyRect.left) + 'px';
  menuEl.style.top = rect.top + bodyRect.top + rect.height + 'px';

}

$('thumb-checkbox').addEventListener('change', function(e) {
  if (e.target.checked) {
    showSection(Section.THUMB);
  } else {
    hideSection(Section.THUMB);
  }
  mostVisited.updateDisplayMode();
  layoutMostVisited();
  saveShownSections();
});

$('list-checkbox').addEventListener('change', function(e) {
  var newValue = shownSections;
  if (e.target.checked) {
    showSection(Section.LIST);
  } else {
    hideSection(Section.LIST);
  }
  mostVisited.updateDisplayMode();
  layoutMostVisited();
  saveShownSections();
});

window.addEventListener('load', bind(logEvent, global, 'onload fired'));
window.addEventListener('load', onDataLoaded);
window.addEventListener('resize', handleWindowResize);
document.addEventListener('DOMContentLoaded', bind(logEvent, global,
                                                   'domcontentloaded fired'));

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
      layoutMostVisited();
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
        layoutMostVisited();
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
