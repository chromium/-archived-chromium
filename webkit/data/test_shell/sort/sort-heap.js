// heapsort

function sort_heap(sort, end) {
  if (arguments.length == 1) {
    var mid = Math.floor(sort.size/2 - 1);
    sort.add_work(function() { build_heap(sort, mid); }, "build_heap");
  } else if (end > 0) {
    sort.swap(end, 0);
    end--;
    sort.add_work(function() { sort_heap(sort, end); }, "sort_heap");
    sort.add_work(function() { sift_down(sort, 0, end, 0); }, "sift_down");
  }
}

function build_heap(sort, start) {
  if (start >= 0) {
    sort.add_work(function() { build_heap(sort, start-1); }, "build_heap");
    sort.add_work(function() { sift_down(sort, start, sort.size-1, start); },
                  "sift_down");
  } else {
    sort.add_work(function() { sort_heap(sort, sort.size-1); }, 
                  "sort_heap");
  }
}

function sift_down(sort, start, end, root) {
  var child = root * 2 + 1;
  if (child <= end) {
    if (child < end && sort.compare(child, child + 1) < 0) {
      child++;
    }
    if (sort.compare(root, child) < 0) {
      sort.swap(root, child);
      root = child;
      sort.add_work(function() { sift_down(sort, start, end, root); }, 
                    "sift_down");
    }
  }
}

function validate_heap(sort) {
  var i = Math.floor(sort.size/2 - 1);
  while (i >= 0) {
    child = i * 2 + 1;
    if (sort.compare(i, child) < 0)
      return 0;
    if (child + 1 < sort.size)
      if (sort.compare(i, child + 1) < 0)
        return 0;
    i--;
  }
  return 1;
}
