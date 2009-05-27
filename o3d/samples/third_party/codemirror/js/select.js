/* Functionality for finding, storing, and restoring selections
 *
 * This does not provide a generic API, just the minimal functionality
 * required by the CodeMirror system.
 */

// Namespace object.
var select = {};

(function() {
  var ie_selection = document.selection && document.selection.createRangeCollection;

  // Find the 'top-level' (defined as 'a direct child of the node
  // passed as the top argument') node that the given node is
  // contained in. Return null if the given node is not inside the top
  // node.
  function topLevelNodeAt(node, top) {
    while (node && node.parentNode != top)
      node = node.parentNode;
    return node;
  }

  // Find the top-level node that contains the node before this one.
  function topLevelNodeBefore(node, top) {
    while (!node.previousSibling && node.parentNode != top)
      node = node.parentNode;
    return topLevelNodeAt(node.previousSibling, top);
  }

  // Used to prevent restoring a selection when we do not need to.
  var documentChanged = false;

  var fourSpaces = "\u00a0\u00a0\u00a0\u00a0";

  // Most functions are defined in two ways, one for the IE selection
  // model, one for the W3C one.
  if (ie_selection) {
    // Store the current selection in such a way that it can be
    // restored after we manipulated the DOM tree. For IE, we store
    // pixel coordinates.
    select.markSelection = function (win) {
      var selection = win.document.selection;
      var start = selection.createRange(), end = start.duplicate();
      var bookmark = start.getBookmark();
      start.collapse(true);
      end.collapse(false);

      documentChanged = false;
      var body = win.document.body;
      // And we better hope no fool gave this window a padding or a
      // margin, or all these computations will be in vain.
      return {start: {x: start.boundingLeft + body.scrollLeft - 1,
                      y: start.boundingTop + body.scrollTop},
              end: {x: end.boundingLeft + body.scrollLeft - 1,
                    y: end.boundingTop + body.scrollTop},
              window: win,
              bookmark: bookmark};
    };

    // Restore a stored selection.
    select.selectMarked = function(sel) {
      if (!sel || !documentChanged)
        return;

      documentChanged = false;
      var range1 = sel.window.document.body.createTextRange(), range2 = range1.duplicate();
      var done = false;
      if (sel.start.y >= 0 && sel.end.y < sel.window.document.body.clientHeight) {
        // This can fail for various hard-to-handle reasons, so we
        // fall back to moveToBookmark when it throws.
        try {
          range1.moveToPoint(sel.start.x, sel.start.y);
          range2.moveToPoint(sel.end.x, sel.end.y);
          range1.setEndPoint("EndToStart", range2);
          done = true;
        } catch(e) {}
      }
      if (!done) done = range1.moveToBookmark(sel.bookmark);
      if (done) range1.select();
    };


    // See W3C model for the actual role of this function. Here it
    // just sets a flag indicating the selection should be restored.
    select.replaceSelection = function(){
      documentChanged = true;
    };

    // Get the top-level node that one end of the cursor is inside or
    // after. Note that this returns false for 'no cursor', and null
    // for 'start of document'.
    select.selectionTopNode = function(container, start) {
      var selection = container.ownerDocument.selection;
      if (!selection) return false;

      var range = selection.createRange();
      range.collapse(start);
      var around = range.parentElement();
      if (around && isAncestor(container, around)) {
        // Only use this node if the selection is not at its start.
        var range2 = range.duplicate();
        range2.moveToElementText(around);
        if (range.compareEndPoints("StartToStart", range2) == -1)
          return topLevelNodeAt(around, container);
      }
      // Fall-back hack
      range.pasteHTML("<span id='xxx-temp-xxx'></span>");
      var temp = container.ownerDocument.getElementById("xxx-temp-xxx");
      if (temp) {
        var result = topLevelNodeBefore(temp, container);
        removeElement(temp);
        return result;
      }
      return false;
    };

    // Place the cursor after this.start. This is only useful when
    // manually moving the cursor instead of restoring it to its old
    // position.
    select.focusAfterNode = function(node, container) {
      var range = container.ownerDocument.body.createTextRange();
      range.moveToElementText(node || container);
      range.collapse(!node);
      range.select();
    };

    function insertAtCursor(window, html) {
      var selection = window.document.selection;
      if (selection) {
        var range = selection.createRange();
        range.pasteHTML(html);
        range.collapse(false);
        range.select();
      }
    }

    // Used to normalize the effect of the enter key, since browsers
    // do widely different things when pressing enter in designMode.
    select.insertNewlineAtCursor = function(window) {
      insertAtCursor(window, "<br/>");
    };

    select.insertTabAtCursor = function(window) {
      insertAtCursor(window, fourSpaces);
    };

    // Get the BR node at the start of the line on which the cursor
    // currently is, and the offset into the line. Returns null as
    // node if cursor is on first line.
    select.cursorPos = function(container, start) {
      var selection = container.ownerDocument.selection;
      if (!selection) return null;

      var topNode = select.selectionTopNode(container, start);
      while (topNode && topNode.nodeName != "BR")
        topNode = topNode.previousSibling;

      var range = selection.createRange(), range2 = range.duplicate();
      range.collapse(start);
      if (topNode) {
        range2.moveToElementText(topNode);
        range2.collapse(false);
      }
      else {
        // When nothing is selected, we can get all kinds of funky errors here.
        try { range2.moveToElementText(container); }
        catch (e) { return null; }
        range2.collapse(true);
      }
      range.setEndPoint("StartToStart", range2);

      return {node: topNode, offset: range.text.length};
    };

    select.setCursorPos = function(container, from, to) {
      function rangeAt(pos) {
        var range = container.ownerDocument.body.createTextRange();
        if (!pos.node) {
          range.moveToElementText(container);
          range.collapse(true);
        }
        else {
          range.moveToElementText(pos.node);
          range.collapse(false);
        }
        range.move("character", pos.offset);
        return range;
      }

      var range = rangeAt(from);
      if (to && to != from)
        range.setEndPoint("EndToEnd", rangeAt(to));
      range.select();
    }

    // Make sure the cursor is visible.
    select.scrollToCursor = function(container) {
      var selection = container.ownerDocument.selection;
      if (!selection) return null;
      selection.createRange().scrollIntoView();
    };
  }
  // W3C model
  else {
    // This is used to fix an issue with getting the scroll position
    // in Opera.
    var opera_scroll = !window.scrollX && !window.scrollY;

    // Store start and end nodes, and offsets within these, and refer
    // back to the selection object from those nodes, so that this
    // object can be updated when the nodes are replaced before the
    // selection is restored.
    select.markSelection = function (win) {
      documentChanged = false;
      var selection = win.getSelection();
      if (!selection || selection.rangeCount == 0)
        return null;
      var range = selection.getRangeAt(0);

      var result = {start: {node: range.startContainer, offset: range.startOffset},
                    end: {node: range.endContainer, offset: range.endOffset},
                    window: win,
                    scrollX: opera_scroll && win.document.body.scrollLeft,
                    scrollY: opera_scroll && win.document.body.scrollTop};

      // We want the nodes right at the cursor, not one of their
      // ancestors with a suitable offset. This goes down the DOM tree
      // until a 'leaf' is reached (or is it *up* the DOM tree?).
      function normalize(point){
        while (point.node.nodeType != 3 && point.node.nodeName != "BR") {
          var newNode = point.node.childNodes[point.offset] || point.node.nextSibling;
          point.offset = 0;
          while (!newNode && point.node.parentNode) {
            point.node = point.node.parentNode;
            newNode = point.node.nextSibling;
          }
          point.node = newNode;
          if (!newNode)
            break;
        }
      }

      normalize(result.start);
      normalize(result.end);
      // Make the links back to the selection object (see
      // replaceSelection).
      if (result.start.node)
        result.start.node.selectStart = result.start;
      if (result.end.node)
        result.end.node.selectEnd = result.end;

      return result;
    };

    select.selectMarked = function (sel) {
      if (!sel || !documentChanged)
        return;
      var win = sel.window;
      var range = win.document.createRange();

      function setPoint(point, which) {
        if (point.node) {
          // Remove the link back to the selection.
          delete point.node["select" + which];
          // Some magic to generalize the setting of the start and end
          // of a range.
          if (point.offset == 0)
            range["set" + which + "Before"](point.node);
          else
            range["set" + which](point.node, point.offset);
        }
        else {
          range.setStartAfter(win.document.body.lastChild || win.document.body);
        }
      }

      // Have to restore the scroll position of the frame in Opera.
      if (opera_scroll){
        sel.window.document.body.scrollLeft = sel.scrollX;
        sel.window.document.body.scrollTop = sel.scrollY;
      }
      setPoint(sel.end, "End");
      setPoint(sel.start, "Start");
      selectRange(range, win);
    };

    // This is called by the code in codemirror.js whenever it is
    // replacing a part of the DOM tree. The function sees whether the
    // given oldNode is part of the current selection, and updates
    // this selection if it is. Because nodes are often only partially
    // replaced, the length of the part that gets replaced has to be
    // taken into account -- the selection might stay in the oldNode
    // if the newNode is smaller than the selection's offset. The
    // offset argument is needed in case the selection does move to
    // the new object, and the given length is not the whole length of
    // the new node (part of it might have been used to replace
    // another node).
    select.replaceSelection = function(oldNode, newNode, length, offset) {
      documentChanged = true;
      function replace(which) {
        var selObj = oldNode["select" + which];
        if (selObj) {
          if (selObj.offset > length) {
            selObj.offset -= length;
          }
          else {
            newNode["select" + which] = selObj;
            delete oldNode["select" + which];
            selObj.node = newNode;
            selObj.offset += (offset || 0);
          }
        }
      }
      replace("Start");
      replace("End");
    };

    // Helper for selecting a range object.
    function selectRange(range, window) {
      var selection = window.getSelection();
      selection.removeAllRanges();
      selection.addRange(range);
    };
    function selectionRange(window) {
      var selection = window.getSelection();
      if (!selection || selection.rangeCount == 0)
        return false;
      else
        return selection.getRangeAt(0);
    }

    // Finding the top-level node at the cursor in the W3C is, as you
    // can see, quite an involved process.
    select.selectionTopNode = function(container, start) {
      var range = selectionRange(container.ownerDocument.defaultView);
      if (!range) return false;

      var node = start ? range.startContainer : range.endContainer;
      var offset = start ? range.startOffset : range.endOffset;
      // Work around (yet another) bug in Opera's selection model.
      if (window.opera && !start && range.endContainer == container && range.endOffset == range.startOffset + 1 &&
          container.childNodes[range.startOffset] && container.childNodes[range.startOffset].nodeName == "BR")
        offset--;

      // For text nodes, we look at the node itself if the cursor is
      // inside, or at the node before it if the cursor is at the
      // start.
      if (node.nodeType == 3){
        if (offset > 0)
          return topLevelNodeAt(node, container);
        else
          return topLevelNodeBefore(node, container);
      }
      // Occasionally, browsers will return the HTML node as
      // selection. If the offset is 0, we take the start of the frame
      // ('after null'), otherwise, we take the last node.
      else if (node.nodeName == "HTML") {
        return (offset == 1 ? null : container.lastChild);
      }
      // If the given node is our 'container', we just look up the
      // correct node by using the offset.
      else if (node == container) {
        return (offset == 0) ? null : node.childNodes[offset - 1];
      }
      // In any other case, we have a regular node. If the cursor is
      // at the end of the node, we use the node itself, if it is at
      // the start, we use the node before it, and in any other
      // case, we look up the child before the cursor and use that.
      else {
        if (offset == node.childNodes.length)
          return topLevelNodeAt(node, container);
        else if (offset == 0)
          return topLevelNodeBefore(node, container);
        else
          return topLevelNodeAt(node.childNodes[offset - 1], container);
      }
    };

    select.focusAfterNode = function(node, container) {
      var win = container.ownerDocument.defaultView,
          range = win.document.createRange();
      range.setStartBefore(container.firstChild || container);
      // In Opera, setting the end of a range at the end of a line
      // (before a BR) will cause the cursor to appear on the next
      // line, so we set the end inside of the start node when
      // possible.
      if (node && !node.firstChild)
        range.setEndAfter(node);
      else if (node)
        range.setEnd(node, node.childNodes.length);
      else
        range.setEndBefore(container.firstChild || container);
      range.collapse(false);
      selectRange(range, win);
    };

    insertNodeAtCursor = function(window, node) {
      var range = selectionRange(window);
      if (!range) return;

      range.deleteContents();
      range.insertNode(node);
      range.setEndAfter(node);
      range.collapse(false);
      selectRange(range, window);
      return node;
    }

    select.insertNewlineAtCursor = function(window) {
      insertNodeAtCursor(window, window.document.createElement("BR"));
    };

    select.insertTabAtCursor = function(window) {
      insertNodeAtCursor(window, window.document.createTextNode(fourSpaces));
    };

    select.cursorPos = function(container, start) {
      var range = selectionRange(window);
      if (!range) return;

      var topNode = select.selectionTopNode(container, start);
      while (topNode && topNode.nodeName != "BR")
        topNode = topNode.previousSibling;

      range = range.cloneRange();
      range.collapse(start);
      if (topNode)
        range.setStartAfter(topNode);
      else
        range.setStartBefore(container);
      return {node: topNode, offset: range.toString().length};
    };

    select.setCursorPos = function(container, from, to) {
      var win = container.ownerDocument.defaultView,
          range = win.document.createRange();

      function setPoint(node, offset, side) {
        if (!node)
          node = container.firstChild;
        else
          node = node.nextSibling;

        if (!node)
          return;

        if (offset == 0) {
          range["set" + side + "Before"](node);
          return true;
        }

        var backlog = []
        function decompose(node) {
          if (node.nodeType == 3)
            backlog.push(node);
          else
            forEach(node.childNodes, decompose);
        }
        while (true) {
          while (node && !backlog.length) {
            decompose(node);
            node = node.nextSibling;
          }
          var cur = backlog.shift();
          if (!cur) return false;

          var length = cur.nodeValue.length;
          if (length >= offset) {
            range["set" + side](cur, offset);
            return true;
          }
          offset -= length;
        }
      }

      to = to || from;
      if (setPoint(to.node, to.offset, "End") && setPoint(from.node, from.offset, "Start"))
        selectRange(range, win);
    };

    select.scrollToCursor = function(container) {
      var body = container.ownerDocument.body, win = container.ownerDocument.defaultView;
      var element = select.selectionTopNode(container, true) || container.firstChild;
      
      // In Opera, BR elements *always* have a scrollTop property of zero. Go Opera.
      while (element && !element.offsetTop)
        element = element.previousSibling;

      var y = 0, pos = element;
      while (pos && pos.offsetParent) {
        y += pos.offsetTop;
        pos = pos.offsetParent;
      }

      var screen_y = y - body.scrollTop;
      if (screen_y < 0 || screen_y > win.innerHeight - 10)
        win.scrollTo(0, y);
    };
  }
}());
