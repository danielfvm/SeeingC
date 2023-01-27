#!/bin/bash

_term() { 
  echo "Caught SIGTERM signal!" 
  kill -TERM "$CHILD1" 2>/dev/null
  kill -TERM "$CHILD2" 2>/dev/null
}

trap _term SIGTERM

BASEDIR=$(dirname "$0")

WD=$BASEDIR $BASEDIR/build/bin/seeing &
CHILD1=$! 

$BASEDIR/autoupdater.sh &
CHILD2=$!

wait "$CHILD1"
wait "$CHILD2"
