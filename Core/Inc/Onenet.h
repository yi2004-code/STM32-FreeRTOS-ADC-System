#ifndef __ONENET_H
#define __ONENET_H
typedef enum{
 Onenet_OK,
 Onenet_TIMEOUT,
 Onenet_ERROR
}Onenet_StatusTypeDef;

Onenet_StatusTypeDef Onenet_Init();
Onenet_StatusTypeDef Onenet_Clear(void);
Onenet_StatusTypeDef Onenet_SendCommand(char *command,char *expect_receive,uint16_t timeout);
void Onenet_SendString(const char *str);
Onenet_StatusTypeDef Onenet_Mqttdata(void);
Onenet_StatusTypeDef Onenet_Mqtt(void);
Onenet_StatusTypeDef Onenet_Mqttreply(void);
Onenet_StatusTypeDef Onenet_Mqttset(void);
Onenet_StatusTypeDef Onenet_MqttSend(char*id,uint16_t value);
#endif