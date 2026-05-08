
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "adc.h"
//创建互斥锁保护串口打印任务
osMutexId_t printfMutexHandle; 
//创建结构体以此完成运输3个adc通道的采集数据
typedef struct{
uint16_t ch0;
uint16_t ch1;
uint16_t ch2;

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
  .stack_size = 128 * 4,
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
//创建信号量
osSemaphoreId_t myBinarySem01Handle;
const osSemaphoreAttr_t myBinarySem01_attributes = {
  .name = "myBinarySem01"
};



void StartDefaultTask(void *argument);
void StartADC_Task(void *argument);
void StartUsart_Task(void *argument);
void StartWarning_Task(void *argument);

void MX_FREERTOS_Init(void); 

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
   printfMutexHandle = osMutexNew(NULL);

  myBinarySem01Handle = osSemaphoreNew(1, 0, &myBinarySem01_attributes);

  myQueue01Handle = osMessageQueueNew (3, sizeof(ADC_Data), &myQueue01_attributes);

  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  ADC_TaskHandle = osThreadNew(StartADC_Task, NULL, &ADC_Task_attributes);

  Usart_TaskHandle = osThreadNew(StartUsart_Task, NULL, &Usart_Task_attributes);

  Warning_TaskHandle = osThreadNew(StartWarning_Task, NULL, &Warning_Task_attributes);



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
#define FILTER_WINDOW_SIZE 8 
uint16_t ADC_Value[3];
static  uint16_t filter_buffer[FILTER_WINDOW_SIZE];   //存放历史数据的数组
static  uint32_t filter_sum=0;                        //总的数据和
static  uint8_t filter_index = 0;                     // 当前存放数据的下标
static  uint8_t filter_count = 0;                     //存放的数据计数

void StartADC_Task(void *argument)
{
	 HAL_ADC_Start_DMA(&hadc1,(uint32_t *)ADC_Value,3);
	 ADC_Data pack;
	 
	//初始化滤波缓冲区
	filter_sum=0;
	for(int i=0;i<FILTER_WINDOW_SIZE;i++){
	 filter_buffer[i]=0;
	}
	filter_index = 0;
	
	
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
	 pack.ch1=filter_ch1;
	 pack.ch2=ADC_Value[2];
	 
	  //判断是否数据是否异常如果异常则点亮报警灯停止打印数据
	  if(pack.ch0<100){
		
	   osSemaphoreRelease(myBinarySem01Handle);
		 
     }
	 else{
	osMessageQueuePut(myQueue01Handle,&pack,0,100);
	 }
	   osDelay(50);
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
	 size_t freeHeapSize; 
while(1){
	
	if(osMessageQueueGet(myQueue01Handle,&receive,0,100)==osOK)
		{
			osMutexAcquire(printfMutexHandle, osWaitForever);
            printf("数据 -> CH0: %d, CH1: %d, CH2: %d \r\n", 
                   receive.ch0, receive.ch1, receive.ch2);

			 osMutexRelease(printfMutexHandle);
        }
	osDelay(50);
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


