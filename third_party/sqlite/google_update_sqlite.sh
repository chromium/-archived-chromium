#!/bin/bash
#
# A simple script to make it easier to merge in newer versions of sqlite.
# It may not work perfectly, in which case, it at least serves as an outline
# of the procedure to follow.

if [ "$1" = "" ]; then
  echo "Usage: $0 <Date to pull from CVS> [<merge tool>]"
  echo "Example: $0 '2007/01/24 09:54:56'"
  exit 1
fi

if [ "$2" = "" ]; then
  MERGE="kdiff3 -m"
fi

BASE_DATE=`cat VERSION_DATE`
NEW_DATE="$1"

cd ..
echo "_____ Logging in to sqlite.org cvs (log in as anonymous)..."
cvs -d :pserver:anonymous@www.sqlite.org:/sqlite login
cvs -d :pserver:anonymous@www.sqlite.org:/sqlite checkout -P -D "$BASE_DATE" -d sqlite-base sqlite
cvs -d :pserver:anonymous@www.sqlite.org:/sqlite checkout -P -D "$NEW_DATE" -d sqlite-latest sqlite

echo "_____ Running merge tool..."
$MERGE sqlite-base sqlite-latest sqlite

cd sqlite

echo "_____ Updating VERSION_DATE to be $NEW_DATE ..."
echo $NEW_DATE > VERSION_DATE

echo "_____ Processing generated files..."
./google_generate_preprocessed.sh
