#ifndef __OLED_H
#define __OLED_H
void HAL_Write_Cmd(uint8_t cmd);
void HAL_Write_Data(uint8_t data);
void HAL_OLED_Init(void);
void HAL_OLED_SetPos(uint8_t x,uint8_t y);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t x,uint8_t y,char Char);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
#endif
