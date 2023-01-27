#!/bin/bash
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

# install
cp -r . /opt/seeing

# build
/opt/seeing/build.sh

# enable autostart
cp seeing.service /etc/systemd/system/seeing.service
chmod 640 /etc/systemd/system/seeing.service
systemctl daemon-reload
systemctl enable seeing.service
systemctl stop seeing.service
sleep 1
systemctl start seeing.service
