import asyncio
import aiosc
import logging
import json
import signal
import sys
from aiocoap import *
from subprocess import Popen

logging.basicConfig(level=logging.INFO)

#Gateway in local network
#ADDRESS_GATEWAY = '169.254.160.79'

#Gateway in OSC-Network
ADDRESS_GATEWAY = '192.168.3.201'

ADDRESS = ADDRESS_GATEWAY

PORT_COMM = 9000
PORT_LEFTHAND = 9001
PORT_RIGHTHAND = 9002

devices = '{ "9001": { "name":"lefthand", "ipv6":"fd9a:620d:c050:0:e373:4e07:71bb:600a" }, "9002": { "name":"righthand", "ipv6":"fd9a:620d:c050:0:b993:4e72:7187:600a" }, "9000": { "name":"default", "ipv6":"::1" }}'
devicesJSON = json.loads(devices)

class OSCserver(aiosc.OSCProtocol):

	period = 0
	pause = 0
	count = 0

	def __init__(self):
		super().__init__(handlers={
		'/sys/exit' 	: self.exit,
		'/subscribe' 	: self.subscribeUser,
		'/unsubscribe' 	: self.unsubscribeUser,
		'/vibrate' 	: self.vibrate,
		'/devices' 	: self.getDevices,
		'/update' 	: self.updateDevices })

	def connection_made(self, transport):
		self.transport = transport

	def exit(self, *args):
		syncio.get_event_loop().stop()

	def error_received(self, exc):
		print('Error received:', exc)

	def connection_lost(self, exc):
		print('stop', exc)

	def vibrate(self, addr, path, *args):
		period = args[0]
		pause  = args[1]
		count  = args[2]

		print("vibrate: incoming message from {}: {} {}".format(addr, path, args))

		destAddr = self.transport.get_extra_info('sockname')
		destPort = str(destAddr[1])
		#print(destPort)

		newloop = asyncio.new_event_loop()
		asyncio.set_event_loop(newloop)
		newloop.run_until_complete(cmdVibrate(dest=destPort, period=args[0], pause=args[1], count=args[2]))

	def subscribeUser(self, addr, path, *args):
		print("subscribe: incoming request from {}: {} {}".format(addr, path, args))

	def unsubscribeUser(self, addr, path, *args):
		print("unsubscribe: incoming request from {}: {} {}".format(addr, path, args))

	def updateDevices(self, addr, path, *args):
		print("updateDevices: incoming request from {}: {} {}".format(addr, path, args))

	def getDevices(self, addr, path, *args):
		print("devices: incoming request from {}: {} {}".format(addr, path, args))


#_COMMANDS _____________________________________________________________________

def cmdVibrate(dest, period, pause, count):
	print("cmdVibrate" + " period: " + str(period) + " ms, pause: " + str(pause) + " ms, count: " + str(count) + " times")
	context = yield from Context.create_client_context()
	message = str(str(period) + "/" + str(pause) + "/" + str(count))
	payload = str(message).encode('utf8')
	device = devicesJSON[dest]
	uri = str("coap://["+ device["ipv6"] +"]/vibrate")
	request = Message(code=PUT, payload=payload)
	request.set_request_uri(uri)

	response = yield from context.request(request).response
	print("Result: %s %s", response.code, response.payload)
	yield from asyncio.sleep(0)

def updateMaster(addr):
	context = yield from Context.create_client_context()
	payload = addr
	request = Message(code=PUT, payload=payload)
	uri = str("coap://[::1]/update")
	request.set_request_uri(uri)
	response = yield from context.request(request).response
	print("Result: %s %s", response.code, response.payload)
	yield from asyncio.sleep(0)


print("KOMMUNIKATION    -OSC Server is running on port " + str(PORT_COMM))
print("LEFTHAND         -OSC Server is running on port " + str(PORT_LEFTHAND))
print("RIGHTHAND        -OSC Server is running on port " + str(PORT_RIGHTHAND))

# Event Loop
loop = asyncio.get_event_loop()
coro_comm = loop.create_datagram_endpoint(OSCserver, local_addr=(ADDRESS, PORT_COMM))
coro_lefthand = loop.create_datagram_endpoint(OSCserver, local_addr=(ADDRESS, PORT_LEFTHAND))
coro_righthand = loop.create_datagram_endpoint(OSCserver, local_addr=(ADDRESS, PORT_RIGHTHAND))
transport, protocol = loop.run_until_complete(coro_comm)
transport, protocol = loop.run_until_complete(coro_lefthand)
transport, protocol = loop.run_until_complete(coro_righthand)
loop.run_forever()
loop.close()
