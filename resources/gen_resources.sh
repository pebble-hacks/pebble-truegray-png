#!/bin/sh
i=1
while read p; do
  echo "{"
  echo "  \"type\": \"raw\","
  echo "  \"name\": \"IMAGE_$i\","
  echo "  \"file\": \"$p.png\""
  echo "},"
  i=`expr $i + 1`
done < $1
