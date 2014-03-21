/*
 * Copyright (c) 2014 Jonathan Leach
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *******************************************************************************/

#include "json.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <unistd.h>
#include "MQTTAsync.h"
#include "lightwaverf.h"

#define ADDRESS     "tcp://192.168.1.70:1883"
#define CLIENTID    "lightwaverf-mqtt-adapter"
#define TOPIC       "lightwaverf"
#define QOS         1
#define TIMEOUT     10000L

static byte nibbles[] = {0xF6,0xEE,0xED,0xEB,0xDE,0xDD,0xDB,0xBE,0xBD,0xBB,0xB7,0x7E,0x7D,0x7B,0x77,0x6F};

typedef struct
{
   const char* code;
   int level;
   int unit;
} Command;

volatile MQTTAsync_token deliveredtoken;

int disc_finished = 0;
int subscribed = 0;
int finished = 0;


void json_parse(json_object * jobj, Command* command) {
  enum json_type type;
  json_object_object_foreach(jobj, key, val) { /*Passing through every array element*/

  type = json_object_get_type(val);

  if(strcmp(key,"code") == 0 && type == json_type_string){
	command->code = json_object_get_string(val);
	printf("found code %s\n",command->code);
  }
  if(strcmp(key,"unit") == 0 && type == json_type_int){
	command->unit = json_object_get_int(val);
	printf("found unit %d\n",command->unit);
  }
  if(strcmp(key,"level") == 0 && type == json_type_int){
	command->level = json_object_get_int(val);
	printf("found level %d\n",command->level);
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

    Command command;
    int i;
    char* payloadptr;

    payloadptr = message->payload;
   
    printf("Received JSON request: %s\n",payloadptr);

    json_object * jobj = json_tokener_parse(payloadptr);     
    json_parse(jobj,&command);

    int count;
    const char *pos = command.code;
    byte codeParts[6];
    for(count = 0; count < 6; count++) {
        sscanf(pos, "%2hhx", &codeParts[count]);
        pos += 2;
    }

    byte bytes[10];
    bytes[0] = 0xf6;
    bytes[1] = 0xf6;
    bytes[2] = nibbles[command.unit];
    bytes[3] = nibbles[command.level];
    bytes[4] = codeParts[0];
    bytes[5] = codeParts[1];
    bytes[6] = codeParts[2];
    bytes[7] = codeParts[3];
    bytes[8] = codeParts[4];
    bytes[9] = codeParts[5];

    int j;
    for (j = 0; j < 10; j++)
    {
        if (j > 0) printf(":");
        printf("%02X", bytes[j]);
    }
    printf("\n");

    if(command.level < 2) {
	lw_send(bytes);
    } else if(command.level > 1 && command.level < 32) {
    	lw_cmd(0x80 + command.level,command.unit,LW_ON,codeParts);
    }    

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

	while	(!subscribed){
			sleep(10);
	}

	if (finished)
		goto exit;


	while(!finished){
	    sleep(10);
	}

	disc_opts.onSuccess = onDisconnect;
	if ((rc = MQTTAsync_disconnect(client, &disc_opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start disconnect, return code %d\n", rc);
		exit(-1);	
	}
 	while	(!disc_finished)
			sleep(10);

exit:
	MQTTAsync_destroy(&client);
 	return rc;
}
