#!/bin/bash

while true
do
  echo "Checking for plugged usb sticks"

  # Check if it should copy data to the measurements folder
  if [ -d /media/usb/measurements ] && [ -z "$(ls -A /media/usb/measurements)" ]
  then
    rm -rf /media/usb/measurements
    cp /opt/seeing/measurements /media/usb/measurements
  fi

  # Check if the update-file exists in the /media/usb/ directory
  if [ -f /media/usb/seeing.*.tar.xz ]
  then
    echo "Detected available update file"
    # Extract the version number from the file name
    version=$(basename /media/usb/seeing.*.tar.xz | cut -d '.' -f 2)

    # Check if the version number in the file is greater than the current version
    # or if the VERSION file or /opt/seeing directory doesn't exist
    if [ ! -f /opt/seeing/VERSION ] || [ $version -gt $(cat /opt/seeing/VERSION) ]
    then
      # Extract the archive to /opt/seeing/
      mkdir -p /opt/seeing/
      tar -xvf /media/usb/seeing.*.tar.xz -C /opt/seeing/

      # Update the version number in the /opt/seeing/VERSION file
      echo $version > /opt/seeing/VERSION

      # Rebuild seeing
      /opt/seeing/build.sh

	  # If supported by BIOS, make a beep sound to indicate finished update
      echo -e "\a"

      # Restart
      killall -9 seeing
      exit
    fi
  fi

  # Sleep for 5 seconds
  sleep 5
done
