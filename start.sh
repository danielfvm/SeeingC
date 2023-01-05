#!/bin/bash

_term() { 
  echo "Caught SIGTERM signal!" 
  kill -TERM "$CHILD" 2>/dev/null
}

trap _term SIGTERM

BASEDIR=$(dirname "$0")
WD=$BASEDIR $BASEDIR/build/bin/seeing &
CHILD=$! 

wait "$CHILD"
