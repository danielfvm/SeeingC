#!/bin/bash

_term() { 
  echo "Caught SIGTERM signal!" 
  kill -TERM "$CHILD_UPDATER" 2>/dev/null
  kill -TERM "$CHILD_SEEING" 2>/dev/null
  exit
}

trap _term SIGTERM
trap _term SIGSTOP

BASEDIR=$(dirname "$0")

# Start autoupdater and store pid, so that we can kill it if this 
# script gets terminated
$BASEDIR/usbdaemon.sh &
CHILD_UPDATER=$!

# Start seeing in infinite loop, in case that it crashes we can
# restart it after a 5 sec timeout
while true
do
  echo "Starting seeing with working directory $BASEDIR"
  WD=$BASEDIR $BASEDIR/build/bin/seeing
  CHILD_SEEING=$!
  echo "Seeing stopped, restarting in 5 seconds"
  sleep 5
done
