/**
 * The local strings get injected into the page usig a varaible named
 * {@code templateData}. This class provides a simpler interface to access those
 * strings.
 * @constructor
 */
function LocalStrings() {
}

/**
 * Gets a localized string by its id.
 * @param {string} s The id of the string we want.
 * @return {string} The localized string.
 */
LocalStrings.prototype.getString = function(s) {
  return templateData[s] || '';
};

/**
 * Returns a formatted localized string (where all %s contents are replaced
 * by the second argument).
 * @param {string} s The id of the string we want.
 * @param {string} d The string to include in the formatted string.
 * @return {string} The formatted string.
 */
LocalStrings.prototype.formatString = function(s, d) {
  return this.getString(s).replace(/%s/, d);
};
