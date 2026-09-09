/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1306.h"
#include "ssd1306_tests.h"
#include "ssd1306_fonts.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* ========================= Constants ========================= */

#define ADC_MAX 4095.0f

#define FAN_ON_TEMP       25.0f
#define FAN_OFF_TEMP      24.0f

#define HEAT_START_TEMP   18.0f
#define HEAT_FULL_TEMP    11.0f

#define RX_BUF_LEN 64


/* ========================= PINS ========================= */

#define FAN_GPIO_Port   GPIOB
#define FAN_Pin         GPIO_PIN_0

#define PUMP_GPIO_Port  GPIOB
#define PUMP_Pin        GPIO_PIN_1

#define HEAT_TIM        htim2
#define HEAT_CH         TIM_CHANNEL_1

#define CURTAIN_TIM     htim2
#define CURTAIN_CH      TIM_CHANNEL_2

#define DS18B20_PORT GPIOA
#define DS18B20_PIN  GPIO_PIN_2

/* ========================= VARIABLES ========================= */

volatile uint16_t AD_RES_BUFFER[2];  // [0]=HUM (Rank1), [1]=LDR (Rank2)

typedef enum {
  MODE_AUTO = 0,
  MODE_MANUAL = 1
} SystemMode_t;

SystemMode_t g_mode = MODE_AUTO; // just auto for now
uint8_t door_state = 0;   // 0 = CLOSED , 1 = OPEN
uint8_t btn_mode_last = GPIO_PIN_SET;

uint32_t adc_hum = 0;  // PA0
uint32_t adc_ldr = 0;  // PA1


uint8_t Temp_byte1, Temp_byte2;
uint16_t TEMP;
float Temperature = 0;
uint8_t Presence = 0;


static uint8_t rx_ch;
static char    rx_line[RX_BUF_LEN];
static uint8_t rx_idx = 0;

/*********************************************/
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* ==================================== UART CONTROL =================================== */

static void PWM_SetPercent(TIM_HandleTypeDef *htim, uint32_t channel, uint8_t duty)
{
    if (duty > 100) duty = 100;
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(htim);
    uint32_t ccr = ((arr + 1U) * (uint32_t)duty) / 100U;
    __HAL_TIM_SET_COMPARE(htim, channel, ccr);
}

static void UART_Send(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)s, (uint16_t)strlen(s), 200);
}

static void Trim(char *s)
{
    size_t n = strlen(s);
    while (n && (s[n-1] == '\r' || s[n-1] == '\n' || s[n-1] == ' ' || s[n-1] == '\t')) s[--n] = 0;

    size_t i = 0;
    while (s[i] == ' ' || s[i] == '\t') i++;
    if (i) memmove(s, s + i, strlen(s + i) + 1);
}

static void ToUpperStr(char *s)
{
    for (; *s; s++) if (*s >= 'a' && *s <= 'z') *s = (char)(*s - 'a' + 'A');
}

static void ProcessCommand(char *cmd)
{
    Trim(cmd);
    ToUpperStr(cmd);

    // ===== STATUS command (works in AUTO and MANUAL) =====
    if (strcmp(cmd, "STATUS") == 0)
    {
        char buf[128];

        uint32_t hum_p = (adc_hum * 100U) / 4095U;
        uint32_t ldr_p = (adc_ldr * 100U) / 4095U;

        // Temperature with 2 decimals (like OLED)
        int temp_int  = (int)Temperature;
        int temp_frac = (int)((Temperature - temp_int) * 100 + 0.5f);
        if (temp_frac >= 100) { temp_frac = 0; temp_int++; }

        snprintf(buf, sizeof(buf),
                 "HUM=%lu%% LDR=%lu%% TEMP=%d.%02dC MODE=%s DOOR=%s\r\n",
                 hum_p, ldr_p,
                 temp_int, temp_frac,
                 (g_mode == MODE_AUTO) ? "AUTO" : "MANUAL",
                 door_state ? "OPEN" : "CLOSED");

        UART_Send(buf);
        return;
    }


    if (g_mode == MODE_AUTO) { UART_Send("AUTO MODE\r\n"); return; }

    if (strcmp(cmd, "FAN ON") == 0)  { HAL_GPIO_WritePin(FAN_GPIO_Port, FAN_Pin, GPIO_PIN_SET);   UART_Send("OK\r\n"); return; }
    if (strcmp(cmd, "FAN OFF") == 0) { HAL_GPIO_WritePin(FAN_GPIO_Port, FAN_Pin, GPIO_PIN_RESET); UART_Send("OK\r\n"); return; }

    if (strcmp(cmd, "PUMP ON") == 0)  { HAL_GPIO_WritePin(PUMP_GPIO_Port, PUMP_Pin, GPIO_PIN_SET);   UART_Send("OK\r\n"); return; }
    if (strcmp(cmd, "PUMP OFF") == 0) { HAL_GPIO_WritePin(PUMP_GPIO_Port, PUMP_Pin, GPIO_PIN_RESET); UART_Send("OK\r\n"); return; }

    if (strncmp(cmd, "HEAT ", 5) == 0) {
        int duty = atoi(cmd + 5);
        if (duty < 0) duty = 0;
        if (duty > 100) duty = 100;
        PWM_SetPercent(&HEAT_TIM, HEAT_CH, (uint8_t)duty);
        UART_Send("OK\r\n");
        return;
    }

    if (strncmp(cmd, "CURTAIN ", 8) == 0) {
        int duty = atoi(cmd + 8);
        if (duty < 0) duty = 0;
        if (duty > 100) duty = 100;
        PWM_SetPercent(&CURTAIN_TIM, CURTAIN_CH, (uint8_t)duty);
        UART_Send("OK\r\n");
        return;
    }

    UART_Send("ERR\r\n");
}

/**************************************************************************************************************************************/


/* ==================================== DS18B20 CONTROL =================================== */

void delay_us(uint16_t time)
{
__HAL_TIM_SET_COUNTER(&htim2,0);
while ((__HAL_TIM_GET_COUNTER(&htim2))<time);
}

void Set_Pin_Output(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

void Set_Pin_Input(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}




uint8_t DS18B20_Start (void)
{
	uint8_t Response = 0;
	Set_Pin_Output(DS18B20_PORT, DS18B20_PIN);   // set the pin as output
	HAL_GPIO_WritePin (DS18B20_PORT, DS18B20_PIN, 0);  // pull the pin low
	delay_us(480);   // delay according to datasheet

	Set_Pin_Input(DS18B20_PORT, DS18B20_PIN);    // set the pin as input
	delay_us(80);

	if (!(HAL_GPIO_ReadPin (DS18B20_PORT, DS18B20_PIN))) Response = 1;    // if the pin is low i.e the presence pulse is detected
		else Response = -1;

		delay_us (400); // 480 us delay totally.

		return Response;
	}

void DS18B20_Write (uint8_t data)
{
	Set_Pin_Output(DS18B20_PORT, DS18B20_PIN);  // set as output

	for (int i=0; i<8; i++)
	{
		if ((data & (1<<i))!=0)  // if the bit is high
		{
			// write 1
			Set_Pin_Output(DS18B20_PORT, DS18B20_PIN);  // set as output
			HAL_GPIO_WritePin (DS18B20_PORT, DS18B20_PIN, 0);  // pull the pin LOW
			delay_us (1);  // wait for 1 us

			Set_Pin_Input(DS18B20_PORT, DS18B20_PIN);  // set as input
			delay_us (50);  // wait for 50 us
		}

		else  // if the bit is low
		{
			// write 0
			Set_Pin_Output(DS18B20_PORT, DS18B20_PIN);
			HAL_GPIO_WritePin (DS18B20_PORT, DS18B20_PIN, 0);  // pull the pin LOW
			delay_us (50);  // wait for 50 us

			Set_Pin_Input(DS18B20_PORT, DS18B20_PIN);
		}
	}
}

uint8_t DS18B20_Read (void)
{
	uint8_t value=0;
	  Set_Pin_Input (DS18B20_PORT, DS18B20_PIN);

	for (int i=0;i<8;i++)
	{
		Set_Pin_Output (DS18B20_PORT, DS18B20_PIN);
		HAL_GPIO_WritePin (DS18B20_PORT,DS18B20_PIN, 0);  // pull the data pin LOW
		delay_us (2);  // wait for 2 us
		Set_Pin_Input (DS18B20_PORT, DS18B20_PIN);
		if (HAL_GPIO_ReadPin (DS18B20_PORT, DS18B20_PIN))  // if the pin is HIGH
		{
			value |= 1<<i;  // read = 1
		}
		delay_us (60);  // wait for 60 us
	}
	return value;
}



static uint32_t MapTempToHeaterPWM(float temp_c)
{
    if (temp_c >= HEAT_START_TEMP) return 0;
    if (temp_c <= HEAT_FULL_TEMP) return 100;

    float ratio = (HEAT_START_TEMP - temp_c) / (HEAT_START_TEMP - HEAT_FULL_TEMP);
    float duty  = ratio * 100.0f;

    if (duty < 0.0f) duty = 0.0f;
    if (duty > 100.0f) duty = 100.0f;

    return (uint32_t)(duty + 0.5f);
}

void Temp_Actuators_Update(float temp_c)
{
    static uint8_t fan_on = 0;

    if (!fan_on && temp_c >= FAN_ON_TEMP) fan_on = 1;
    else if (fan_on && temp_c <= FAN_OFF_TEMP) fan_on = 0;

    HAL_GPIO_WritePin(FAN_GPIO_Port, FAN_Pin,
                      fan_on ? GPIO_PIN_SET : GPIO_PIN_RESET);

    uint32_t duty_percent = 0;

    if (!fan_on)
        duty_percent = MapTempToHeaterPWM(temp_c);

    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim2);
    uint32_t ccr = ((arr + 1U) * duty_percent) / 100U;

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, ccr);
}
/***************************************************************************************************************************************/


/* ==================================== PUSHBUTTONS CONTROL =================================== */

void HandleModeButton(void)
{
    GPIO_PinState btn_now = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3);

    if (btn_mode_last == GPIO_PIN_SET && btn_now == GPIO_PIN_RESET)
    {
        g_mode = (g_mode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
        HAL_Delay(200);
    }

    btn_mode_last = btn_now;

    // --- MODE Indicator on LED PC13 (Active-LOW) ---
        if (g_mode == MODE_AUTO)
        {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        }
        else
        {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        }

        // ===== Button 2 : PA4 → Door Sensor =====
        static uint8_t btn2_last = GPIO_PIN_SET;

        GPIO_PinState btn2_now = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4);

        if (btn2_last == GPIO_PIN_SET && btn2_now == GPIO_PIN_RESET)
        {
            door_state = !door_state;
            HAL_Delay(200);
        }

        btn2_last = btn2_now;

        if (door_state)
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);   // OPEN
        else
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); // CLOSED
}

/***********************************************************************************************************************************/

/* ==================================== PUMP LED CONTROL =================================== */

float Humidity_ToPercent(uint32_t adc)
{
    return (adc * 100.0f) / ADC_MAX;
}

void Pump_ControlByHumidity()
{

	const float HUM_TH = 50.0f;

	float hum_percent = Humidity_ToPercent(adc_hum);


  HAL_GPIO_WritePin(PUMP_GPIO_Port, PUMP_Pin, (hum_percent < HUM_TH) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/***********************************************************************************************************************************/

/* ==================================== OLED DISPLAY CONTROL =================================== */

 void OLED_ShowValues()
{

    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("Door:", Font_11x18, White);
    ssd1306_SetCursor(60, 0);
    ssd1306_WriteString((door_state) ? "OPEN" : "CLOSED", Font_11x18, White);
    ssd1306_SetCursor(0, 16);
    ssd1306_WriteString("Mode:", Font_11x18, White);
    ssd1306_SetCursor(60, 16);
    ssd1306_WriteString((g_mode == MODE_AUTO) ? "AUTO" : "MANUAL", Font_11x18, White);
    ssd1306_UpdateScreen();
    HAL_Delay(1500);

       uint32_t hum_p = (adc_hum * 100U) / 4095U;
       uint32_t ldr_p = (adc_ldr * 100U) / 4095U;

       char line1[24];
       char line2[24];
       char line3[24];


       int temp_int = (int)Temperature;
       int temp_frac = (int)((Temperature - temp_int) * 100 + 0.5f);

       if (temp_frac >= 100) { temp_frac = 0; temp_int++; }

       snprintf(line3, sizeof(line3), "TEMP: %d.%02d C", temp_int, temp_frac);
       snprintf(line1, sizeof(line1), "HUM: %lu %%", hum_p);
       snprintf(line2, sizeof(line2), "LDR: %lu %%", ldr_p);

       ssd1306_Fill(Black);
       ssd1306_SetCursor(0, 0);
       ssd1306_WriteString(line1, Font_11x18, White);
       ssd1306_SetCursor(0, 16);
       ssd1306_WriteString(line2, Font_11x18, White);
       ssd1306_SetCursor(0, 32);
       ssd1306_WriteString(line3, Font_11x18, White);
       ssd1306_UpdateScreen();
       HAL_Delay(3000);

           ssd1306_Fill(Black);
           ssd1306_SetCursor(14, 23);
           ssd1306_WriteString("READY :)", Font_11x18, White);
           ssd1306_UpdateScreen();
}

 /***********************************************************************************************************************************/


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  HAL_UART_Receive_IT(&huart1, &rx_ch, 1);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)AD_RES_BUFFER, 2);
  ssd1306_Init();
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

  ssd1306_Fill(Black);
      ssd1306_SetCursor(16, 19);
      ssd1306_WriteString("HELLOW", Font_16x26, White);
      ssd1306_UpdateScreen();
      HAL_Delay(2000);



  //ssd1306_SetDisplayOn(0);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {


	  /***************** DS18B20 Command **************/
	         Presence = DS18B20_Start ();
	  	     HAL_Delay(1);
	  	     DS18B20_Write (0xCC);  // skip ROM
	  	     DS18B20_Write (0x44);  // convert t
	  	     HAL_Delay(800);

	  	     Presence = DS18B20_Start ();
	  	     HAL_Delay(1);
	  	     DS18B20_Write (0xCC);  // skip ROM
	  	     DS18B20_Write (0xBE);  // Read Scratch-pad

	  	     Temp_byte1 = DS18B20_Read();
	  	     Temp_byte2 = DS18B20_Read();
	  	     TEMP = ((Temp_byte2<<8))|Temp_byte1;
	  	     Temperature = (float)TEMP/16.0;  // resolution is 0.0625

	  	     HAL_Delay(3000);


	  /*********************************************************/


	  if (g_mode == MODE_AUTO)
	  	  	  {

	  	      Temp_Actuators_Update(Temperature);

	     	  Pump_ControlByHumidity();

	  	  	  }


	  	  OLED_ShowValues();


	  	for (int i = 0; i < 30; i++)   // 50 × 30ms = 1500ms
	  	{
	  	    HandleModeButton();
	  	    HAL_Delay(50);
	  	}




    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        adc_hum = AD_RES_BUFFER[0];
        adc_ldr = AD_RES_BUFFER[1];

        if (g_mode == MODE_AUTO)
        {
            uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim2);
            uint32_t pwm = (AD_RES_BUFFER[1] * (arr + 1U)) / 4095U;
            TIM2->CCR2 = (arr + 1U) - pwm;
        }
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        if (rx_ch == '\r' || rx_ch == '\n')
        {
            rx_line[rx_idx] = '\0';
            if (rx_idx > 0) ProcessCommand(rx_line);
            rx_idx = 0;
        }
        else
        {
            if (rx_idx < (RX_BUF_LEN - 1)) rx_line[rx_idx++] = (char)rx_ch;
            else rx_idx = 0;
        }

        HAL_UART_Receive_IT(&huart1, &rx_ch, 1);
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
