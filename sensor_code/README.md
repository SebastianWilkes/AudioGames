# sensor_node

## introduction

This code provides a simple sensor node that will response to COAP request send
via 6lowpan using IEEE 802.15.4. The sensor nodes are based on the RIOT-OS and
currently support only the Phytec board, namely `pba-d-01-kw2x` - see RIOT [Wiki](https://github.com/RIOT-OS/RIOT/wiki/Board%3A-Phytec-phyWAVE-KW22), too.
Currently it supports COAP requests for:
* PUT /vibrate {period, pause, count} ;

It also sends COAP Messages on Buttonclicks on PinPort A1 and E2. Make sure to add the path /trigger to your COAP receiver process.

## requirements

To build and run this project you need the _RIOT-OS_ source code, an appropriate
_compiler_ (namely the `arm-none-eabi-gcc`), and the _OpenOCD_ tool to flash the
binary onto the microcontroller board.

* Clone RIOT with `git clone https://github.com/RIOT-OS/RIOT.git`
* Install _GCC_ and _OpenOCD_ as described in the Phytec [Wiki](https://github.com/PHYTEC-Messtechnik-GmbH/phynode-riot-examples/wiki/RIOT-getting-started#toolchain-summary)

That's all, you are all set and good to go ...

## build

To compile the source code set `RIOTBASE`, `PORT`,
and `PATH` environment variables according to your system and run `make` afterwards:

```
export RIOTBASE=<path/to/riot>
export PORT=/dev/tty<usbserial>
export PATH=<path/to/arm-none-eabi-gcc>:$PATH
make
make flash
make term
```

## usage

Typically such a sensor node would sit around somewhere in your house or office
or anywhere, waiting for COAP request via 6lowpan querying sensor values. You
could do so for instance using the Firefox [Copper Plugin](https://addons.mozilla.org/de/firefox/addon/copper-270430/).
Or any other COAP-Client you like, pointing it to the IPv6 address of the sensor
node. See _introduction_ above for currently supported request.

For _debugging_ the nodes also allow for shell access, that is connect to the node
via `make term` and you get a shell. Besides `get` and `put` you can also get
information on the network interface settings using `ifconfig` - or type `help`
for further shell commands.
