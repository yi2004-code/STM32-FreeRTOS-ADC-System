#ifndef __ESP8266_H
#define __ESP8266_H
#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h" 

typedef enum{
ESP8266_OK=0,
ESP8266_TIMEOUT,
ESP8266_ERROR
}Esp8266_StatusTypeDef;

Esp8266_StatusTypeDef Esp8266_Init(UART_HandleTypeDef *huart);
Esp8266_StatusTypeDef Esp8266_Station(uint8_t mode);
Esp8266_StatusTypeDef Esp8266_Clearbuff();
Esp8266_StatusTypeDef Esp8266_DHCP(void);
Esp8266_StatusTypeDef Esp8266_SendCommand(char *command,char * expect_receive,uint16_t Timeout);
void ESP8266_SendString(const char *str);
void ESP8266_HandleRxByte(uint8_t byte);
uint8_t* ESP8266_GetBuffer(void);
UART_HandleTypeDef* ESP8266_GetUartHandle(void);

#ifdef __cplusplus
}
#endif
#endif
