// quicksort

function sort_quick(sort, left, right) {
  if (arguments.length == 1) {
    left = 0;
    right = sort.size - 1;
  }
  if (left < right) {
    var pivot = left + Math.floor(Math.random()*(right-left));
    //var pivot = Math.floor(left + (right-left)/2);
    partition(sort, left, right, pivot);
  }
}

function partition(sort, left, right, pivot) {
  sort.swap(pivot, right);
  sort.add_work(function(){partition_step(sort, left, right, pivot, left, left);});
} 

function partition_step(sort, left, right, pivot, i, j) {
  if (i < right) {
    if (sort.compare(i, right) <= 0) {
      sort.swap(i, j);
      j++;
    }
    i++;
    sort.add_work(function(){partition_step(sort, left, right, pivot, i, j)});
  } else {
    sort.swap(j, right);
    sort.add_work(function(){sort_quick(sort, left, j-1)});
    sort.add_work(function(){sort_quick(sort, j+1, right)});
  }
}

