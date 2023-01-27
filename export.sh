#!/bin/bash

if [ $# -eq 0 ]
then
  echo "Usage: $0 version_number"
  exit 1
fi


# Update version number
echo $1 > VERSION

# Remove existing archives
rm -rf seeing.*.tar.xz

# Create new archive
tar -czvf seeing.$1.tar.xz --exclude='build' --exclude='seeing.*.tar.xz' --exclude='settings.txt' .

rm -r VERSION

echo ""
echo "Finished exporting seeing.$1.tar.xz"
