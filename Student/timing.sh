#!/bin/bash

for value in {22..50..3}
do
  make clean > /dev/null
  make > /dev/null
  echo "threads: " $value
  time ./downloader $1 $value $2 | tail
  echo ""
done
