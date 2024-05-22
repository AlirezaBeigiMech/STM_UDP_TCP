#include "lwip/apps/mqtt.h"
#include "lwip_mqtt.h"
#include <string.h>
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"

// ##############################################################

#include <string.h>
#include "lwip.h"
#include "lwip/api.h"
#include "MQTTClient.h"
#include "MQTTInterface.h"

#define BROKER_IP		"192.168.0.2"
//#define MQTT_PORT		1883
#define MQTT_BUFSIZE	1024



osThreadId mqttClientSubTaskHandle;  //mqtt client task handle
osThreadId mqttClientPubTaskHandle;  //mqtt client task handle

Network net; //mqtt network
MQTTClient mqttClient; //mqtt client

uint8_t sndBuffer[MQTT_BUFSIZE]; //mqtt send buffer
uint8_t rcvBuffer[MQTT_BUFSIZE]; //mqtt receive buffer
uint8_t msgBuffer[MQTT_BUFSIZE]; //mqtt message buffer

void MqttClientSubTask(void const *argument)
{
	/*
	while(1)
	{
		//waiting for valid ip address
		if (gnetif.ip_addr.addr == 0 || gnetif.netmask.addr == 0 || gnetif.gw.addr == 0) //system has no valid ip address
		{
			osDelay(1000);
			continue;
		}
		else
		{
			printf("DHCP/Static IP O.K.\n");
			break;
		}
	}
	*/

	while(!mqttClient.isconnected)
	{
		if(!mqttClient.isconnected)
		{
			//try to connect to the broker
			MQTTDisconnect(&mqttClient);
			MqttConnectBroker();
			osDelay(1000);
		}
		else
		{
			MQTTYield(&mqttClient, 1000); //handle timer
			osDelay(100);
		}
	}

	const char* str = "MQTT message from STM32";
	MQTTMessage message;

	while(1)
	{
		if(mqttClient.isconnected)
		{
			message.payload = (void*)str;
			message.payloadlen = strlen(str);

			MQTTPublish(&mqttClient, "test", &message); //publish a message
		}

		osDelay(1000);
	}

}

int MqttConnectBroker()
{
	int ret;

	NewNetwork(&net);
	ret = ConnectNetwork(&net, BROKER_IP, MQTT_PORT);
	osDelay(1000);
	if(ret != MQTT_SUCCESS)
	{
		printf("ConnectNetwork failed.\n");
		return -1;
	}

	MQTTClientInit(&mqttClient, &net, 1000, sndBuffer, sizeof(sndBuffer), rcvBuffer, sizeof(rcvBuffer));

	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	data.willFlag = 0;
	data.MQTTVersion = 3;
	data.clientID.cstring = "STM32F4";
	data.username.cstring = "STM32F4";
	data.password.cstring = "";
	data.keepAliveInterval = 60;
	data.cleansession = 1;

	ret = MQTTConnect(&mqttClient, &data);
	if(ret != MQTT_SUCCESS)
	{
		printf("MQTTConnect failed.\n");
		return ret;
	}

	ret = MQTTSubscribe(&mqttClient, "test", QOS0, MqttMessageArrived);
	if(ret != MQTT_SUCCESS)
	{
		printf("MQTTSubscribe failed.\n");
		return ret;
	}

	printf("MQTT_ConnectBroker O.K.\n");
	return MQTT_SUCCESS;
}
void MqttMessageArrived(MessageData* msg)
{
	//HAL_GPIO_TogglePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin); //toggle pin when new message arrived

	MQTTMessage* message = msg->message;
	memset(msgBuffer, 0, sizeof(msgBuffer));
	memcpy(msgBuffer, message->payload,message->payloadlen);

	printf("MQTT MSG[%d]:%s\n", (int)message->payloadlen, msgBuffer);
}

// ##############################################################


//extern UART_HandleTypeDef huart4;
char buffer[1000];
/* The idea is to demultiplex topic and create some reference to be used in data callbacks
   Example here uses a global variable, better would be to use a member in arg
   If RAM and CPU budget allows it, the easiest implementation might be to just take a copy of
   the topic string and use it in mqtt_incoming_data_cb
*/
static int inpub_id;
void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
  sprintf(buffer,"Incoming publish at topic %s with total length %u\n\r", topic, (unsigned int)tot_len);
//HAL_UART_Transmit(&huart4,buffer,strlen(buffer),1000);
  /* Decode topic string into a user defined reference */
  if(strcmp(topic, "print_payload") == 0) {
    inpub_id = 0;
  } else if(topic[0] == 'A') {
    /* All topics starting with 'A' might be handled at the same way */
    inpub_id = 1;
  }
  else {
    /* For all other topics */
    inpub_id = 9;
  }

}

void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
	  sprintf(buffer,"Incoming publish payload with length %d, flags %u\n\r", len, (unsigned int)flags);
	  //HAL_UART_Transmit(&huart4,buffer,strlen(buffer),1000);

  if(flags & MQTT_DATA_FLAG_LAST) {
    /* Last fragment of payload received (or whole part if payload fits receive buffer
       See MQTT_VAR_HEADER_BUFFER_LEN)  */

    /* Call function or do action depending on reference, in this case inpub_id */
    if(inpub_id == 0) {
      /* Don't trust the publisher, check zero termination */
      if(data[len-1] == 0) {
    	  sprintf(buffer,"mqtt_incoming_data_cb: %s\n\r", (const char *)data);
    	  //HAL_UART_Transmit(&huart4,buffer,strlen(buffer),1000);
      }
 else {
      sprintf(buffer,"mqtt_incoming_data_cb: Ignoring payload...\n\r");
	  ///HAL_UART_Transmit(&huart4,buffer,strlen(buffer),1000);
 }
	}
	}
}






void mqtt_sub_request_cb(void *arg, err_t result)
{
  /* Just print the result code here for simplicity,
     normal behaviour would be to take some action if subscribe fails like
     notifying user, retry subscribe or disconnect from server */
  sprintf(buffer,"Subscribe result: %d\n\r", result);
  //HAL_UART_Transmit(&huart4,buffer,strlen(buffer),1000);

}

void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
  char * topico = "hello_world";
  err_t err;
  if(status == MQTT_CONNECT_ACCEPTED) {
    sprintf(buffer,"mqtt_connection_cb: Successfully connected\n");
	  //HAL_UART_Transmit(&huart4,buffer,strlen(buffer),1000);

    /* Setup callback for incoming publish requests */
    mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, "hello_world");

    /* Subscribe to a topic named "placa" with QoS level 0, call mqtt_sub_request_cb with result */
    err = mqtt_subscribe(client, topico, 0, mqtt_sub_request_cb, "hello_world");

    if(err != ERR_OK) {
      sprintf(buffer,"mqtt_subscribe return: %d\n", err);
	  //HAL_UART_Transmit(&huart4,buffer,strlen(buffer),1000);

    }
  } else {
    sprintf(buffer,"mqtt_connection_cb: Disconnected, reason: %d\n", status);
	  //HAL_UART_Transmit(&huart4,buffer,strlen(buffer),1000);

    /* Its more nice to be connected, so try to reconnect */
    example_do_connect(client,topico);
  }

}


void example_do_connect(mqtt_client_t *client, char *topic)
{
  //struct mqtt_connect_client_info_t ci;
  err_t err = ERR_RST;

  /* Setup an empty client info structure */
  //memset(&ci, 0, sizeof(ci));

  /* Minimal amount of information required is client identifier, so set it here */
  //ci.client_id = "xonga";
  //ci.client_user = "mosquitto";
  //ci.client_pass = "chupasangre"; /* Tiempo en mi caso */



  static const struct mqtt_connect_client_info_t ci =
  {
    "test",
    NULL, /* user */
    NULL, /* pass */
    100,  /* keep alive */
    NULL, /* will_topic */
    NULL, /* will_msg */
    0,    /* will_qos */
    0     /* will_retain */
  #if LWIP_ALTCP && LWIP_ALTCP_TLS
    , NULL
  #endif
  };


  /* Initiate client and connect to server, if this fails immediately an error code is returned
     otherwise mqtt_connection_cb will be called with connection result after attempting
     to establish a connection with the server.
     For now MQTT version 3.1.1 is always used */
  ip_addr_t mqttServerIP;
  IP4_ADDR(&mqttServerIP, 169, 254, 0, 177);
//  err = mqtt_client_connect(client, &mqttServerIP, MQTT_PORT, mqtt_connection_cb, 0, &ci);
  while(err != ERR_OK && err!=ERR_ISCONN){
	  err = mqtt_client_connect(client, &mqttServerIP, MQTT_PORT, mqtt_connection_cb, topic, &ci);
	  osDelay(1000);
  }


  /* For now just print the result code if something goes wrong */
  if(err != ERR_OK) {
    sprintf(buffer,"mqtt_connect return %d\n\r", err);
    //err = mqtt_client_connect(client, &mqttServerIP, MQTT_PORT, mqtt_connection_cb, topic, &ci);

	  //HAL_UART_Transmit(&huart4,buffer,strlen(buffer),1000);

  }
}

/* Called when publish is complete either with sucess or failure */
void mqtt_pub_request_cb(void *arg, err_t result)
{
  if(result != ERR_OK) {
    sprintf(buffer,"Publish result: %d\n", result);
	  //HAL_UART_Transmit(&huart4,buffer,strlen(buffer),1000);
  }
}
void example_publish(mqtt_client_t *client, void *arg)
{
  //const char *pub_payload= "Hola mundo de mierda!";
  const char *pub_payload= arg;
  err_t err;
  u8_t qos = 2; /* 0 1 or 2, see MQTT specification */
  u8_t retain = 0; /* No don't retain such crappy payload... */

  err = mqtt_publish(client, "hello_world", pub_payload, strlen(pub_payload), qos, retain, mqtt_pub_request_cb, arg);
  osDelay(500);
  if(err != ERR_OK) {
    sprintf(buffer,"Publish err: %d\n\r", err);
	  //HAL_UART_Transmit(&huart4,buffer,strlen(buffer),1000);
  }
}





