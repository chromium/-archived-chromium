///////////////////////////////////////////////////////////////////////////////
// localStrings:
/**
 * We get strings into the page by using JSTemplate to populate some elements
 * with localized content, then reading the content of those elements into
 * this global strings object.
 * @param {Node} node The DOM node containing all our strings.
 */
function LocalStrings(node) {
  this.strings_ = {};

  var children = node.children;
  for (var i = 0, child; child = children[i]; i++) {
    var id = child.id;
    if (id) {
      this.strings_[id] = child.innerHTML;
    }
  }
}

/**
 * Gets a localized string by its id.
 * @param {string} s The id of the string we want.
 * @return {string} The localized string.
 */
LocalStrings.prototype.getString = function(s) {
  return this.strings_[s] || '';
};

/**
 * Returns a formatted localized string (where all %s contents are replaced
 * by the second argument).
 * @param {string} s The id of the string we want.
 * @param {string} d The string to include in the formatted string.
 * @return {string} The formatted string.
 */
LocalStrings.prototype.formatString = function(s, d) {
  return this.getString(s).replace(/\%s/, d);
};