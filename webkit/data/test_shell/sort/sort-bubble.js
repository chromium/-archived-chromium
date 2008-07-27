// bubble sort

function sort_bubble(sort, x, y) {
  if (arguments.length == 1) {
    x = 1; y = 0;
  }
  var len = sort.bars.length;
  if (x < len && y < len) {
    if (sort.compare(x, y) < 0) {
      sort.swap(x, y);
    }
    y++;
    if (y == x) {
      y = 0;
      x++;
    }
    if (x < len) {
      sort.add_work(function() { sort_bubble(sort, x, y); });
      return;
    }
  }
}

