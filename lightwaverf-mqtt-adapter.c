/*******************************************************************************
 * Copyright (c) 2012, 2013 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution. 
 *
 * The Eclipse Public License is available at 
 *   http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at 
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial contribution
 *******************************************************************************/

#include "json.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "MQTTAsync.h"
#include "lightwaverf.h"

#define ADDRESS     "tcp://192.168.1.70:1883"
#define CLIENTID    "lightwaverf-mqtt-adapter"
#define TOPIC       "lightwaverf"
#define QOS         1
#define TIMEOUT     10000L

static byte nibbles[] = {0xF6,0xEE,0xED,0xEB,0xDE,0xDD,0xDB,0xBE,0xBD,0xBB,0xB7,0x7E,0x7D,0x7B,0x77,0x6F};

char* code;
int level;
int unit;

volatile MQTTAsync_token deliveredtoken;

int disc_finished = 0;
int subscribed = 0;
int finished = 0;


void json_parse(json_object * jobj) {
  enum json_type type;
  json_object_object_foreach(jobj, key, val) { /*Passing through every array element*/

  type = json_object_get_type(val);

  if(strcmp(key,"code") == 0 && type == json_type_string){
	code = json_object_get_string(val);
	printf("found code %s\n",code);
  }
  if(strcmp(key,"unit") == 0 && type == json_type_int){
	unit = json_object_get_int(val);
	printf("found unit %d\n",unit);
  }
  if(strcmp(key,"level") == 0 && type == json_type_int){
	level = json_object_get_int(val);
	printf("found level %d\n",level);
  }
  }
}

void connlost(void *context, char *cause)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	int rc;

	printf("\nConnection lost\n");
	printf("     cause: %s\n", cause);

	printf("Reconnecting\n");
	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start connect, return code %d\n", rc);
	    finished = 1;
	}
}


int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    int i;
    char* payloadptr;

    payloadptr = message->payload;
    
    json_object * jobj = json_tokener_parse(payloadptr);     
    json_parse(jobj);
    long number = strtol(code, NULL, 16);

    char xlate[] = "0123456789abcdef";
    byte b1 = ((strchr(xlate, *code) - xlate) * 16) + ((strchr(xlate, *(code + 1)) - xlate));
    byte b2 = ((strchr(xlate, *(code+2)) - xlate) * 16) + ((strchr(xlate, *(code + 3)) - xlate));
    byte b3 = ((strchr(xlate, *(code+4)) - xlate) * 16) + ((strchr(xlate, *(code + 5)) - xlate));
    byte b4 = ((strchr(xlate, *(code+6)) - xlate) * 16) + ((strchr(xlate, *(code + 7)) - xlate));
    byte b5 = ((strchr(xlate, *(code+8)) - xlate) * 16) + ((strchr(xlate, *(code + 9)) - xlate));
    byte b6 = ((strchr(xlate, *(code+10)) - xlate) * 16) + ((strchr(xlate, *(code + 11)) - xlate));

    printf("%02x %02x %02x %02x %02x %02x", b1,b2,b3,b4,b5,b6);

    byte bytes[10];
    bytes[0] = 0xf6;
    bytes[1] = 0xf6;
    bytes[2] = nibbles[unit];
    bytes[3] = nibbles[level];
    bytes[4] = b1;
    bytes[5] = b2;
    bytes[6] = b3;
    bytes[7] = b4;
    bytes[8] = b5;
    bytes[9] = b6;

    lw_send(bytes);

    json_object_put(jobj);
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}


void onDisconnect(void* context, MQTTAsync_successData* response)
{
	printf("Successful disconnection\n");
	disc_finished = 1;
}


void onSubscribe(void* context, MQTTAsync_successData* response)
{
	printf("Subscribe succeeded\n");
	subscribed = 1;
}

void onSubscribeFailure(void* context, MQTTAsync_failureData* response)
{
	printf("Subscribe failed, rc %d\n", response ? response->code : 0);
	finished = 1;
}


void onConnectFailure(void* context, MQTTAsync_failureData* response)
{
	printf("Connect failed, rc %d\n", response ? response->code : 0);
	finished = 1;
}


void onConnect(void* context, MQTTAsync_successData* response)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	int rc;

	printf("Successful connection\n");

	printf("Subscribing to topic %s\nfor client %s using QoS%d\n\n"
           "Press Q<Enter> to quit\n\n", TOPIC, CLIENTID, QOS);
	opts.onSuccess = onSubscribe;
	opts.onFailure = onSubscribeFailure;
	opts.context = client;

	deliveredtoken = 0;

	if ((rc = MQTTAsync_subscribe(client, TOPIC, QOS, &opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start subscribe, return code %d\n", rc);
		exit(-1);	
	}
}


int main(int argc, char* argv[])
{
	lw_setup();

	MQTTAsync client;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	MQTTAsync_token token;
	int rc;
	int ch;

	MQTTAsync_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

	MQTTAsync_setCallbacks(client, NULL, connlost, msgarrvd, NULL);

	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	conn_opts.onSuccess = onConnect;
	conn_opts.onFailure = onConnectFailure;
	conn_opts.context = client;
	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start connect, return code %d\n", rc);
		exit(-1);	
	}

	while	(!subscribed)
		#if defined(WIN32)
			Sleep(100);
		#else
			usleep(10000L);
		#endif

	if (finished)
		goto exit;

	do 
	{
		ch = getchar();
	} while (ch!='Q' && ch != 'q');

	disc_opts.onSuccess = onDisconnect;
	if ((rc = MQTTAsync_disconnect(client, &disc_opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start disconnect, return code %d\n", rc);
		exit(-1);	
	}
 	while	(!disc_finished)
		#if defined(WIN32)
			Sleep(100);
		#else
			usleep(10000L);
		#endif

exit:
	MQTTAsync_destroy(&client);
 	return rc;
}
