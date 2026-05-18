#include "Esp8266.h"
#include <string.h>
#include "stdio.h"
#include "usart.h"

#define ESP8266_MAX_BUFFER 512
#define ESP8266_SSID "8266"
#define ESP8266_PASSWORD "12345678"

static UART_HandleTypeDef *esp_uart;
static uint8_t esp_buffer[ESP8266_MAX_BUFFER];
static uint16_t esp_buffer_index = 0;
static uint8_t rx_byte = 0; 
//所有函数声明
Esp8266_StatusTypeDef Esp8266_Init(UART_HandleTypeDef *huart);
Esp8266_StatusTypeDef Esp8266_Station(uint8_t mode);
Esp8266_StatusTypeDef Esp8266_Clearbuff();
Esp8266_StatusTypeDef Esp8266_DHCP(void);
Esp8266_StatusTypeDef Esp8266_SendCommand(char *command,char * expect_receive,uint16_t Timeout);
void ESP8266_SendString(const char *str);
void ESP8266_HandleRxByte(uint8_t byte);
uint8_t* ESP8266_GetBuffer(void);
UART_HandleTypeDef* ESP8266_GetUartHandle(void);

/**
* @brief 初始化Esp8266
 */
Esp8266_StatusTypeDef Esp8266_Init(UART_HandleTypeDef *huart){
      esp_uart=huart;
	  esp_buffer_index = 0;
	if (huart->Instance==USART2) {
        HAL_UART_Receive_IT(esp_uart, &rx_byte, 1);
	}
	return ESP8266_OK;
}

/**
* @brief 清空数据缓冲区
 */
Esp8266_StatusTypeDef Esp8266_Clearbuff(){
      esp_buffer_index=0;
      memset(esp_buffer,0,sizeof(esp_buffer));
	  return ESP8266_OK;
}


/**
 * @brief 设置ESP8266工作模式（station / AP / 混合）
 * @param mode 1=station, 2=AP, 3=station+AP
 * @return 执行结果
 */
Esp8266_StatusTypeDef Esp8266_Station(uint8_t mode){
     char cmd[64];
	sprintf(cmd,"AT+CWMODE=%d\r\n",mode);
     return Esp8266_SendCommand(cmd,"OK",2000);
}


/**
* @brief 启动DHCP服务，使Esp8266可以获得ip地址
 */
Esp8266_StatusTypeDef Esp8266_DHCP(void){
	char cmd[32];
	sprintf(cmd,"AT+CWDHCP=1,1\r\n");
	return Esp8266_SendCommand(cmd,"OK",1000);
}

/**
* @brief 连接到WiFi网络
  @param ESP8266_SSID 为WIFI，ESP8266_PASSWORD 为密码
 */
Esp8266_StatusTypeDef Esp8266_Wifi(void){
     char cmd[64];
     sprintf(cmd,"AT+CWJAP=\"%s\",\"%s\"\r\n",ESP8266_SSID,ESP8266_PASSWORD);
	 return Esp8266_SendCommand(cmd,"WIFI CONNECTED",15000);
}

Esp8266_StatusTypeDef Esp8266_SendCommand(char *command,char * expect_receive,uint16_t Timeout){
     Esp8266_Clearbuff();
	 if(command==NULL|| esp_uart == NULL){
	  return ESP8266_ERROR;
	 }
	 ESP8266_SendString(command);
	 uint32_t time=HAL_GetTick();
	 while(HAL_GetTick()-time<Timeout){
	 if(strstr((char *)esp_buffer,expect_receive)!=NULL){
	 return ESP8266_OK;
	 }
	 if(strstr((char *)esp_buffer,"ERROR")!=NULL ||
        strstr((char*)esp_buffer, "FAIL") != NULL){
	 return ESP8266_ERROR;
	 }
	 HAL_Delay(10);
 }
	 return ESP8266_TIMEOUT;
}


/**
* @brief 封装串口打印函数
 */
void ESP8266_SendString(const char *str) {
    HAL_UART_Transmit(esp_uart, (uint8_t *)str, strlen(str), 1000);
}

/**
* @brief 将数据存在静态数组中供中断回调函数调用
 */
void ESP8266_StoreByte(uint8_t data){
     if(esp_buffer_index<ESP8266_MAX_BUFFER-1)
	{
	   esp_buffer[esp_buffer_index++]=data;
	   esp_buffer[esp_buffer_index]='\0'; 
     }
}

void ESP8266_HandleRxByte(uint8_t byte){
    ESP8266_StoreByte(byte);
}

uint8_t* ESP8266_GetBuffer(void){
    return esp_buffer;
}

UART_HandleTypeDef* ESP8266_GetUartHandle(void){
    return esp_uart;
}












