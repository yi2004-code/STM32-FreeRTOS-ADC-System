#include "stdio.h"
#include "usart.h"
#include "string.h"
#include "Onenet.h"
#include "Esp8266.h"
#define ONENET_MAX_BUFFER 512
#define PRODUCT_ID "WzlU4HQVYy"
#define DEVICE_NAME "stm32_sensor"
#define TOKEN "version=2018-10-31&res=products%2FWzlU4HQVYy%2Fdevices%2Fstm32_sensor&et=1936330424&method=md5&sign=vEk7qupevKJRg4QycRes2Q%3D%3D"



Onenet_StatusTypeDef Onenet_Init();
Onenet_StatusTypeDef Onenet_Clear(void);
Onenet_StatusTypeDef Onenet_SendCommand(char *command,char *expect_receive,uint16_t timeout);
void Onenet_SendString(const char *str);
Onenet_StatusTypeDef Onenet_Mqttdata(void);
Onenet_StatusTypeDef Onenet_Mqtt(void);
Onenet_StatusTypeDef Onenet_Mqttreply(void);
Onenet_StatusTypeDef Onenet_Mqttset(void);
Onenet_StatusTypeDef Onenet_MqttSend(char*id,uint16_t value);

Onenet_StatusTypeDef Onenet_Init(){

	return Onenet_OK;
}


/**
* @brief 将缓冲区数组清零，方便下一次传输数据
 */
Onenet_StatusTypeDef Onenet_Clear(void){
    Esp8266_Clearbuff();
	return Onenet_OK;
}


/**
* @brief 发送命令函数
 */
Onenet_StatusTypeDef Onenet_SendCommand(char *command,char *expect_receive,uint16_t timeout){
    if(command==NULL){
	 return Onenet_ERROR;
	}
	Onenet_Clear();  // 发送命令前清空缓冲区
	uint8_t *buffer = ESP8266_GetBuffer();
	uint32_t time=HAL_GetTick();
	Onenet_SendString(command);
	while(HAL_GetTick()-time<timeout){
	  if(strstr((char *)buffer,expect_receive)!=NULL){
	   return Onenet_OK;
	  }
	  if(strstr((char *)buffer,"ERROR")!=NULL ||
        strstr((char*)buffer, "FAIL") != NULL){
	    return Onenet_ERROR;
	}
		HAL_Delay(10);
  }
	return Onenet_TIMEOUT;
}

/**
* @brief 封装串口打印函数
 */
void Onenet_SendString(const char *str){
     ESP8266_SendString(str);
}

/**
* @brief 配置MQTT连接的用户名和密码等信息，这里包含了产品的ID、设备的ID以及签名等认证信息。
 */
Onenet_StatusTypeDef Onenet_Mqttdata(void){
     char cmd[256];
	sprintf(cmd,"AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"\r\n",DEVICE_NAME,PRODUCT_ID,TOKEN);
	return Onenet_SendCommand(cmd,"OK",10000);	
}
/**
* @brief 建立与Onenet的MQTT连接
 */
Onenet_StatusTypeDef Onenet_Mqtt(void){
    char cmd[64];
	sprintf(cmd,"AT+MQTTCONN=0,\"mqtts.heclouds.com\",1883,1\r\n");
	return Onenet_SendCommand(cmd,"OK",10000);
}

/**
* @brief 主题用于接收平台对设备上报属性的响应。
 */
Onenet_StatusTypeDef Onenet_Mqttreply(void){
      char cmd[128];
      sprintf(cmd,"AT+MQTTSUB=0,\"$sys/%s/%s/thing/property/post/reply\",1\r\n",PRODUCT_ID,DEVICE_NAME);
	   return Onenet_SendCommand(cmd,"OK",15000);
}

/**
* @brief 主题用于接收平台对设备属性设置的命令。
 */
Onenet_StatusTypeDef Onenet_Mqttset(void){
       char cmd[128];
       sprintf(cmd,"AT+MQTTSUB=0,\"$sys/%s/%s/thing/property/set\",1\r\n",PRODUCT_ID,DEVICE_NAME);
	   return Onenet_SendCommand(cmd,"OK",15000);
}

/**
* @brief 向onenet发送数据
 */
Onenet_StatusTypeDef Onenet_MqttSend(char*id,uint16_t value){
     char cmd[256];
	 char data[128];
	 sprintf(data,"{\"id\":\"123\",\"params\":{\"%s\":{\"value\":%d}}}",id,value);
	 int len=strlen(data);
	 sprintf(cmd,"AT+MQTTPUBRAW=0,\"$sys/%s/%s/thing/property/post\",%d,1,0\r\n",PRODUCT_ID,DEVICE_NAME,len);
	 Onenet_StatusTypeDef receive=Onenet_SendCommand(cmd,">",10000);
	 if(receive==Onenet_OK){
		return Onenet_SendCommand(data,"OK",10000);
	 }
	 else
     {
	   return Onenet_ERROR;
	 }
	 
}




