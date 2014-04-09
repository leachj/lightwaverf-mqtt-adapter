lightwaverf-mqtt-adapter
========================

This is a bridge between an MQTT message bus and the lightwaverf protocol. It uses my lightwaverf-pi project and is designed to interface to a nodeRED installation for home automation

to compile this adapter (on ubuntu) you will need to do the following:

sudo apt-get install libjson0-dev
sudo apt-get install libssl-dev

git clone https://github.com/eclipse/paho.mqtt.c.git
cd paho.mqtt.c
make
sudo make install
cd ..

cd lightwaverf-mqtt-adapter
make
