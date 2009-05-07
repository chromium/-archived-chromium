find();
window.addEventListener("focus", find);

function find() {
  if (window == top) {
    // Find all the RSS link elements.
    var result = document.evaluate(
        '//link[@rel="alternate"][contains(@type, "rss") or ' +
        'contains(@type, "atom") or contains(@type, "rdf")]',
        document, null, 0, null);

    var feeds = [];
    var item;
    while (item = result.iterateNext())
      feeds.push(item.href);

    chrome.extension.connect().postMessage(feeds);
  }
}
