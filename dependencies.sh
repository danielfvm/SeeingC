sudo apt-get install libtiff-dev -y
sudo apt-get install libccfits-dev -y
sudo apt-get install libboost-all-dev -y

wget https://github.com/CrowCpp/Crow/releases/download/v1.0%2B5/crow-v1.0+5.deb
sudo dpkg -i crow-v1.0+5.deb
rm crow-v1.0+5.deb
