#!/bin/bash

_term() { 
  echo "Caught SIGTERM signal!" 
  kill -TERM "$CHILD" 2>/dev/null
  exit
}

trap _term SIGTERM

BASEDIR=$(dirname "$0")

# Start autoupdater and store pid, so that we can kill it if this 
# script gets terminated
$BASEDIR/autoupdater.sh &
CHILD=$!

# Start seeing in infinite loop, in case that it crashes we can
# restart it after a 5 sec timeout
while true
do
  echo "Starting seeing with working directory $BASEDIR"
  WD=$BASEDIR $BASEDIR/build/bin/seeing
  echo "Seeing stopped, restarting in 5 seconds"
  sleep 5
done
