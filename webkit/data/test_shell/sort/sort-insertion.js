// insertion sort

function sort_insertion(sort, x, y) {
  if (arguments.length == 1 || x == undefined) {
    x = 1; y = 1;
  }
  var len = sort.bars.length;
  if (x < len && y > 0) {
    if (sort.compare(y, y - 1) < 0) {
      sort.swap(y, y - 1);
      y--;
      if (y == 0) {
        x++;
        y = x;
      }
    } else {
      x++;
      y = x;
    }
    if (x < len) {
      sort.add_work(function () { sort_insertion(sort, x, y); });
      return;
    }
  }
}

