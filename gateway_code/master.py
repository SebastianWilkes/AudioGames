import asyncio
import aiosc
import logging
import signal
import datetime
import aiocoap.resource as resource
import aiocoap
import json

import sys
from aiocoap import *
from subprocess import Popen


Popen(["python3", 'init.py']);
Popen(["python3", 'commander.py']);

logging.basicConfig(level=logging.INFO)

ADDR_INDEX = 0
PORT_INDEX = 1

UPDATE_DEVICE_PATH = 'devices',
UPDATE_USER_PATH   = 'user',
TRIGGER_PATH 	   = 'trigger',

devices = {}
subscriber = {}

#for local testing
#ADDRESS_GATEWAY = '169.254.160.79' 
#ADDRESS_WFS = "169.254.6.217" 

ADDRESS_GATEWAY = '192.168.3.201'
ADDRESS_WFS = "192.168.3.254"

PORT_WFS = 9000

ADDRESS = ADDRESS_GATEWAY

WFS 	= { (ADDRESS_WFS, PORT_WFS) : { "name":"WFS" } }

LEFTHAND  = {"LEFTHAND"  : { "name":"lefthand",  "description":"Ich kann vibrieren und Befehle via Buttonclick versenden", "ipv6":"fd9a:620d:c050:0:e373:4e07:71bb:600a" }}
RIGHTHAND = {"RIGHTHAND" : { "name":"righthand", "description":"Ich kann vibrieren und Befehle via Buttonclick versenden", "ipv6":"fd9a:620d:c050:0:b993:4e72:7187:600a" }}

subscriber.update(WFS)

print(subscriber)

devices.update(LEFTHAND)
devices.update(RIGHTHAND)


class TriggerResource(resource.Resource):

	def __init__(self, loop, future):
		super(TriggerResource, self).__init__()
		self.__loop = loop
		self.__future = future
		self.content = ("This is the resource's default content. It is padded "\
			"with numbers to be large enough to trigger blockwise "\
				"transfer.\n" + "0123456789\n" * 100).encode("ascii")

	@asyncio.coroutine
	def render_put(self, request):
		print('Trigger PUT payload: %s' % request.payload)
		self.content = request.payload
		payload = ("I've accepted the new payload. You may inspect it here in "\
			"Python's repr format:\n\n%r"%self.content).encode('utf8')

		ip = ADDRESS_WFS
		port = PORT_WFS
		payload = request.payload

		data = request.payload.decode(encoding='UTF-8')
		futureString ={}
		futureString["payload"] = data
		futureString["ip"] = ip
		futureString["port"] = port
		print(futureString)

		self.__future.set_result(futureString)
		self.__future = asyncio.Future()
		self.__future.add_done_callback(publish_trigger)

		return aiocoap.Message(code=aiocoap.CHANGED, mtype=1, payload=payload)


class DeviceResource(resource.Resource):

	def __init__(self, loop, future):
		super(DeviceResource, self).__init__()
		self.__loop = loop
		self.__future = future
		self.content = ("This is the resource's default content. It is padded "\
			"with numbers to be large enough to trigger blockwise "\
				"transfer.\n" + "0123456789\n" * 100).encode("ascii")

	@asyncio.coroutine
	def render_put(self, request):
		print('Device PUT payload: %s' % request.payload)
		self.content = request.payload
		payload = ("Riot device tries to subscribe to gateway: \n\n%r"%self.content).encode('utf8')

		self.__future.set_result(request.payload)
		self.__future = asyncio.Future()
		self.__future.add_done_callback(updateDevice)

		return aiocoap.Message(code=aiocoap.CHANGED, payload=payload)


class UserResource(resource.Resource):

	def __init__(self, loop, future):
		super(UserResource, self).__init__()
		self.__loop = loop
		self.__future = future
		self.content = ("This is the resource's default content. It is padded "\
			"with numbers to be large enough to trigger blockwise "\
				"transfer.\n" + "0123456789\n" * 100).encode("ascii")

	@asyncio.coroutine
	def render_put(self, request):
		print('User PUT payload: %s' % request.payload)
		self.content = request.payload
		self.__future.set_result(str(request.payload))
		self.__future = asyncio.Future()
		self.__future.add_done_callback(updateUser)

		return aiocoap.Message(code=aiocoap.CHANGED, payload=payload)


# Test Area
@asyncio.coroutine
def doSomething():
	while True:
		print(".")
		yield from asyncio.sleep(1)


# Important for Project

# devices
def updateDevice(future):
	payload = future.result
	("Device tries to subscribe or unsubscribe to gateway: \n\n%r"%self.content).encode('utf8')
	message = json.loads(payload)
	device = message["device"]
	if("subscribe" in message):
		addDevice(device)
	elif("unsubscribe" in message):
		removeDevice(device)
	else:
		print("WARNING: no valid updateDevice request!!!")

def addDevice(future):
	device = json.loads(future.result)
	devices.update(device)
	print("device " + device["name"] + "was added")
	publishOSC(UPDATE_PATH, devices)

def removeDevice(future):
	key = json.loads(future.result)
	device = devices.pop[key]
	print("device " + device["name"] + " was removed")
	publishOSC(UPDATE_PATH, devices)


# User
def updateUser(future):
	payload = future.result
	("User tries to subscribe or unsubscribe to gateway: \n\n%r"%self.content).encode('utf8')
	message = json.loads(payload)
	user = message["user"]
	if("subscribe" in message):
		subscribeUser(user)
	elif("unsubscribe" in message):
		unsubscribeUser(user)
	else:
		print("WARNING: no valid updateUser request!!!")

def subscribeUser(user):
	user = json.loads(future.result())
	subscriber.update(user)
	print("User " + user["name"] + " has subscribed")

def unsubscribeUser(user):
	key = json.loads(future.result())
	device = devices.pop[key]
	print("User " + user["name"] + " has unsubscribed")


# Communication

def publish_trigger(future):
	futureString = future.result()
	payload = futureString["payload"]
	addr = (str(futureString["ip"]), futureString["port"])

	newloop = asyncio.new_event_loop()
	asyncio.set_event_loop(newloop)
	#for i in range(0, len(subscriber[i])):
	newloop.run_until_complete(
		aiosc.send(addr, '/trigger', payload)
		#aiosc.send((subscriber[i][ADDR_INDEX], subscriber[i][PORT_INDEX]), TRIGGER_PATH, payload)
	)
	newloop.stop()
	asyncio.set_event_loop(loop)

def sendCOAP(message, uri):
	context = yield from Context.create_client_context()
	payload = str(message).encode("utf8")
	request = Message(code=PUT, payload=payload)
	request.set_request_uri(uri)
	response = yield from context.request(request).response
	print("Result: %s %s", response.code, response.payload)

@asyncio.coroutine
def receiveCOAP(loop, futures):
	# Resource tree creation
	root = resource.Site()
	root.add_resource((TRIGGER_PATH), TriggerResource(loop, futures["Trigger"]))
	root.add_resource((UPDATE_USER_PATH), UserResource(loop, futures["UpdateDevice"]))
	root.add_resource((UPDATE_USER_PATH), DeviceResource(loop, futures["UpdateUser"]))
	asyncio.async(aiocoap.Context.create_server_context(root))

# Base Loop
loop = asyncio.get_event_loop()

futureTrigger 		= asyncio.Future()
futureUpdateDevice  = asyncio.Future()
futureUpdateUser 	= asyncio.Future()

futures = {}
futures.update( {"Trigger" : futureTrigger } )
futures.update( {"UpdateDevice" : futureUpdateDevice } )
futures.update( {"UpdateUser" : futureUpdateUser } )

asyncio.async(receiveCOAP(loop, futures))

#asyncio.async(doSomething())

futureTrigger.add_done_callback(publish_trigger)
futureUpdateDevice.add_done_callback(updateDevice)
futureUpdateUser.add_done_callback(updateUser)

try:
	loop.run_forever()
finally:
	loop.close()
