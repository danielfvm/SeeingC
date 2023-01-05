#!/bin/bash
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

# install and build seeing
cp -r . /opt/seeing
cd /opt/seeing
mkdir -p build
cd build
cmake ..
make
cd ..

# enable autostart
cp seeing.service /etc/systemd/system/seeing.service
chmod 640 /etc/systemd/system/seeing.service
systemctl daemon-reload
systemctl enable seeing.service
systemctl restart seeing.service
