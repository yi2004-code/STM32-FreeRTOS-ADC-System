
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "adc.h"
#include "OLED.h"
#include "Esp8266.h"
#include "usart.h"
#include "Onenet.h"
#include <math.h>

#define VREF           3.3f      // ADC 参考电压 (V)
#define ADC_MAX        4095.0f   // 12位 ADC 最大值
#define R_FIXED        10000.0f  // 固定分压电阻 (Ω)
#define R0             10000.0f  // NTC 在 25°C 时的阻值 (Ω)
#define B              3950.0f   // NTC B值 (K)
#define T0_K           298.15f   // 25°C 开尔文

#define R_REF       10000.0f
#define VREF        3.3f
#define ADC_MAX     4095.0f
#define GAMMA       0.7f
#define R_10LUX     15000.0f




//创建互斥锁保护串口打印任务
osMutexId_t printfMutexHandle; 
osMutexId_t oledMutexHandle;
osMutexId_t espMutexHandle;
//创建结构体以此完成运输3个adc通道的采集数据
typedef struct{
uint16_t ch0;
float temperature; // 温度（摄氏度）
float lux;         // 照度（Lux）

}ADC_Data;

//创建任务
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 32 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

osThreadId_t ADC_TaskHandle;
const osThreadAttr_t ADC_Task_attributes = {
  .name = "ADC_Task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t Usart_TaskHandle;
const osThreadAttr_t Usart_Task_attributes = {
  .name = "Usart_Task",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t Wifi_TaskHandle;
const osThreadAttr_t Wifi_Task_attributes = {
  .name = "Wifi_Task",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t Onenet_TaskHandle;
const osThreadAttr_t Onenet_Task_attributes = {
  .name = "Onenet_Task",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t Warning_TaskHandle;
const osThreadAttr_t Warning_Task_attributes = {
  .name = "Warning_Task",
  .stack_size = 64 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
//创建队列
osMessageQueueId_t myQueue01Handle;
const osMessageQueueAttr_t myQueue01_attributes = {
  .name = "myQueue01"
};
osMessageQueueId_t myQueue02Handle;
const osMessageQueueAttr_t myQueue02_attributes = {
  .name = "myQueue02"
};

//创建信号量
osSemaphoreId_t myBinarySem01Handle;
const osSemaphoreAttr_t myBinarySem01_attributes = {
  .name = "myBinarySem01"
};



void StartDefaultTask(void *argument);
void StartADC_Task(void *argument);
void StartUsart_Task(void *argument);
void StartWarning_Task(void *argument);
void StartWifi_Task(void *argument);
void StartOnenet_Task(void *argument);
void MX_FREERTOS_Init(void); 
float NTC_ADC_To_Temperature(uint16_t adc_value);
float LDR_ADC_To_Lux_Gamma(uint16_t adc_value);

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
   printfMutexHandle = osMutexNew(NULL);
   oledMutexHandle = osMutexNew(NULL);
	espMutexHandle = osMutexNew(NULL);
  myBinarySem01Handle = osSemaphoreNew(1, 0, &myBinarySem01_attributes);

  myQueue01Handle = osMessageQueueNew (3, sizeof(ADC_Data), &myQueue01_attributes);
  
  myQueue02Handle = osMessageQueueNew (3, sizeof(ADC_Data), &myQueue02_attributes);
	
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  ADC_TaskHandle = osThreadNew(StartADC_Task, NULL, &ADC_Task_attributes);

  Usart_TaskHandle = osThreadNew(StartUsart_Task, NULL, &Usart_Task_attributes);

  Warning_TaskHandle = osThreadNew(StartWarning_Task, NULL, &Warning_Task_attributes);

  Wifi_TaskHandle = osThreadNew(StartWifi_Task, NULL, &Wifi_Task_attributes);

  Onenet_TaskHandle = osThreadNew(StartOnenet_Task, NULL, &Onenet_Task_attributes);

}

/**
  * @brief  Function 判断freertos是否正常运行
  * @param  argument: Not used
  * @retval None
  */
void StartDefaultTask(void *argument)
{

  for(;;)
  {
    HAL_GPIO_TogglePin(GPIOA,GPIO_PIN_0);
    osDelay(200); 
  }
}

/**
* @brief Function 运用队列发送通过DMA通道采集到的adc数据，判断数据是否异常当数据异常时通过释放信号量，传递给warning_task.
* @param argument: Not used
* @retval None
*/
#define FILTER_WINDOW_SIZE 4 
uint16_t ADC_Value[3];
static  uint16_t filter_buffer[FILTER_WINDOW_SIZE];   //存放历史数据的数组
static  uint32_t filter_sum=0;                        //总的数据和
static  uint8_t filter_index = 0;                     // 当前存放数据的下标
static  uint8_t filter_count = 0;                     //存放的数据计数
static  uint16_t last_ch0 = 0;
void StartADC_Task(void *argument)
{

	 ADC_Data pack;
	 
	//初始化滤波缓冲区
	filter_sum=0;
	for(int i=0;i<FILTER_WINDOW_SIZE;i++){
	 filter_buffer[i]=0;

	}
	filter_index = 0;
	last_ch0 = ADC_Value[0];
	
   while(1){
	 //对通道ch1的数据进行滤波
	 filter_sum-=filter_buffer[filter_index];           //清除采集到的老数据
	 filter_sum+=ADC_Value[1];
	 filter_buffer[filter_index]=ADC_Value[1];          //将采集到的数据存放到缓冲区中
	 filter_index=(filter_index+1)%FILTER_WINDOW_SIZE;  //更新数据下标
	  
	   if(filter_count<FILTER_WINDOW_SIZE){
	       filter_count++;                        
	  }
	 
	 uint16_t filter_ch1=filter_sum/filter_count;
     
	 
	 
	  
	 pack.ch0=ADC_Value[0];
	 pack.temperature=NTC_ADC_To_Temperature(filter_ch1);
	 pack.lux=LDR_ADC_To_Lux_Gamma(ADC_Value[2]);
	 
	  //判断是否数据是否异常如果异常则点亮报警灯停止打印数据
	uint16_t diff = (last_ch0 > pack.ch0) ? (last_ch0 - pack.ch0) : (pack.ch0 - last_ch0);
    last_ch0 = pack.ch0;

    if(pack.ch0 < 100 || diff > 50){
		
	   osSemaphoreRelease(myBinarySem01Handle);
		 
     }
	 else{
	    osMessageQueuePut(myQueue01Handle,&pack,0,100);
		 
		osMessageQueuePut(myQueue02Handle,&pack,0,100);
	 }
	   osDelay(20);
   }

}

/**
* @brief Function 进行串口打印采集到的adc数据
* @param argument: Not used
* @retval None
*/
void StartUsart_Task(void *argument)
{
	ADC_Data receive;
while(1){
	
	if(osMessageQueueGet(myQueue01Handle,&receive,0,osWaitForever)==osOK)
		{

            osMutexAcquire(oledMutexHandle, osWaitForever);
			OLED_ShowNum(1,1,receive.ch0,4);
			OLED_ShowNum(3,1,receive.temperature,4);
			OLED_ShowNum(5,1,receive.lux,4);
			osMutexRelease(oledMutexHandle);

        }
	osDelay(10);
}
}

/**
* @brief Function 接收信号量接收到是进行报警
* @param argument: Not used
* @retval None
*/
void StartWarning_Task(void *argument)
{

	while(1)
  {
	  osStatus_t state;
	  state=osSemaphoreAcquire(myBinarySem01Handle,osWaitForever);
	if(state==osOK){
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
	  osDelay(1000);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
	}
  }
 }

void StartWifi_Task(void *argument) {
	char msg[] = "Wifi_Task is running\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
    Esp8266_StatusTypeDef status;
    osDelay(2000);

    printf("初始化ESP8266...\r\n");
    status = Esp8266_Init(&huart2);   

    if (status != ESP8266_OK) {
        printf("ESP8266初始化失败(驱动内部错误)\r\n");
        vTaskDelete(NULL);
    }
    printf("ESP8266初始化成功 \r\n");

    // 设置 Station 模式
    status = Esp8266_Station(1);
    if (status == ESP8266_OK)
        printf("Station模式设置成功\r\n");
    else if (status == ESP8266_TIMEOUT)
        printf("Station模式超时\r\n");
    else
        printf("Station模式错误\r\n");

	printf("正在配置DHCP...\r\n");
    status = Esp8266_DHCP();
    if (status == ESP8266_OK)
        printf("DHCP 连接成功!\r\n");
    else if (status == ESP8266_TIMEOUT)
        printf("DHCP 连接超时\r\n");
    else
        printf("DHCP 连接失败\r\n");
	
    // 连接 WiFi
    printf("正在连接 WiFi...\r\n");
    status = Esp8266_Wifi();
    if (status == ESP8266_OK)
        printf("WiFi 连接成功!\r\n");
    else if (status == ESP8266_TIMEOUT)
        printf("WiFi 连接超时\r\n");
    else
        printf("WiFi 连接失败(密码错误或未搜到热点)\r\n");

    // 可选：查询 IP 地址
    char cmd[32];
    sprintf(cmd, "AT+CIFSR\r\n");
    status = Esp8266_SendCommand(cmd, "OK", 3000);
    if (status == ESP8266_OK)
        printf("IP 地址查询成功\r\n");

    for (;;) {
        osDelay(10000);   // 后续可以周期性检查连接状态
    }
}


void StartOnenet_Task(void *argument){
 

    Onenet_StatusTypeDef status;
    
    osDelay(15000);  // 等待WiFi连接完成
    
    printf("初始化OneNet...\r\n");
    status = Onenet_Init();
    if(status == Onenet_OK)
        printf("OneNet初始化成功 \r\n");
    
    printf("配置MQTT用户信息...\r\n");
    status = Onenet_Mqttdata();
    if(status == Onenet_OK)
        printf("MQTT用户配置成功\r\n");
    else if(status == Onenet_TIMEOUT)
        printf("MQTT用户配置超时\r\n");
    else
        printf("MQTT用户配置失败\r\n");
    
    printf("连接OneNet MQTT服务器...\r\n");
    status = Onenet_Mqtt();
    if(status == Onenet_OK)
        printf("OneNet MQTT连接成功\r\n");
    else if(status == Onenet_TIMEOUT)
        printf("OneNet MQTT连接超时\r\n");
    else
        printf("OneNet MQTT连接失败\r\n");
	
        printf("订阅属性响应主题...\r\n");
        status = Onenet_Mqttreply();
        if(status == Onenet_OK)
		   {printf("订阅成功\r\n");}

        printf("订阅属性设置主题...\r\n");
        status = Onenet_Mqttset();
        if(status == Onenet_OK)
		  {printf("订阅成功\r\n");}
    
    for(;;){
		ADC_Data Onenet_receive;
		char *temperature="temperature";
		char *beam="beam";
        if(osMessageQueueGet(myQueue02Handle,&Onenet_receive,0,osWaitForever)==osOK){
		   Onenet_MqttSend(temperature,(uint16_t)Onenet_receive.temperature);
		   Onenet_MqttSend(beam,(uint16_t)Onenet_receive.lux);
		}
		osDelay(10);
    }
}





float NTC_ADC_To_Temperature(uint16_t adc_value) {
    if (adc_value == 0) adc_value = 1;
    if (adc_value >= ADC_MAX) adc_value = ADC_MAX - 1;
    float voltage = (float)adc_value * VREF / ADC_MAX;
    float R_ntc = R_FIXED * voltage / (VREF - voltage);
    // 限制电阻范围，避免 log 参数过小或过大
    if (R_ntc < 100.0f) R_ntc = 100.0f;   // 不会低于100Ω
    if (R_ntc > 200000.0f) R_ntc = 200000.0f;
    float T_kelvin = 1.0f / (1.0f/T0_K + (1.0f/B) * logf(R_ntc / R0));
    float T_celsius = T_kelvin - 273.15f;
    // 输出范围限制
    if (T_celsius < -20.0f) T_celsius = -20.0f;
    if (T_celsius > 80.0f) T_celsius = 80.0f;
    return T_celsius;
}


float LDR_ADC_To_Lux_Gamma(uint16_t adc_value) {
    if (adc_value == 0) adc_value = 1;
    if (adc_value >= ADC_MAX) adc_value = ADC_MAX - 1;

    float voltage = (float)adc_value * VREF / ADC_MAX;
    float Rx = R_REF * (VREF - voltage) / voltage;   // 电路: VCC - R_REF - ADC - LDR - GND
    float lux = powf(10.0f, (log10f(R_10LUX) - log10f(Rx)) / GAMMA) * 10.0f;
    return lux;
}