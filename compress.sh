#!/bin/bash

if [ $# -eq 0 ]
then
  echo "Usage: $0 version_number"
  exit 1
fi

tar -czvf seeing.$1.bak.tar.xz .
