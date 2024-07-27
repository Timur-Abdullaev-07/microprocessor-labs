/**
  ******************************************************************************
  * \file    uart_led.c
  * \author  Абдуллаев Тимур
  * \version 1.0.1
  * \date    22.04.2023
  * \brief   Программа на языке C для учебного стенда на базе
  *          STM32F072RBT6 в среде разработки Keil uVision 5.
  *          Подключение библиотек поддержки МК STM32F072RBT6 осуществляется
  *          средствами IDE Keil через менеджер пакетов Run-Time Environment (RTE).
  *          Разработать программу, демонстрирующую работу интерфейса UART
  *          При вводе в терминал команды "START" в МК запускается секунднаый таймер,
  *          При вводе команды "STOP" таймер останавливается и на терминал выводится 
  *          количество прошедших с запуска секунд.
  *          Если введена неизвестная команда, то на терминал выводится слово
  *          "ERROR".
  *          Программа работает в режиме 0 учебного стенда (S1 = 0, S2 = 0).
  ******************************************************************************
  */

/* Подключение заголовочного файла с макроопределениями всех регистров специальных
   функций МК STM32F072RBT6. */
#include <stm32f0xx.h>

uint16_t data =0;
uint16_t cnt =0;
uint8_t high_byte=0;
uint16_t up_cnt=0; 
char data_ASCII;
uint16_t finishNamber = 0;
void timer_init()
{
    /* Включение тактирования TIM1 */
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;

    /* Расчет предделителя частоты и кода переполнения таймера
       (максимальный код таймера).
       Пусть таймер счетчик переключается каждую 1 мкс или 1 МГц,
       тогда при частоте тактирования МК fmcu = 8 МГц, предделитель требуется
       на 8 - 8 МГц / 1 МГц = 8.
       Пусть переполнение происходит каждую 1 мс или 1 кГц,
       тогда код переполнения должен быть 1 МГц / 1 кГц = 1000 */
    /* Предделитель частоты */
    TIM1->PSC = 8;

    /* Максимальный код таймера (счет идет от 0 до 999) */
    TIM1->ARR = 999;

    /* Включение прерывания по переполнению */
    TIM1->DIER |= TIM_DIER_UIE;

    /* Включить таймер */
    TIM1->CR1 |= TIM_CR1_CEN;

    /* Установка приоритета прерывания по переполнению таймера.
       В Cortex-M0 четыре уровня приоритета - 0-3.
       0 - наибольший приоритет, 3 - наименьший. */
    NVIC_SetPriority(TIM1_BRK_UP_TRG_COM_IRQn, 0);

    /* Разрешение прервания по переполнению */
    NVIC_EnableIRQ(TIM1_BRK_UP_TRG_COM_IRQn);

}

/* Подпрограмма обработчик прерываний по переполнению таймера */
void TIM1_BRK_UP_TRG_COM_IRQHandler(void)
{
    /* Сброс флага вызвавшего прерывание */
    TIM1->SR &= ~TIM_SR_UIF;

    /* Подсчет количества переполнений таймера.
       Между каждым переполнением проходит по 1 мс */
    up_cnt++;
		if (up_cnt > 999) {
			data++;
			up_cnt = 0;
		} 
}

/* Функция инициализации приемника/передатчика USART2 */
void usart_init(void)
{
    /* Включить тактирование порта A */
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

    /* Включить тактирование USART2 */
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* Включить режим альтернативной функции 1 (USART2) на линиях 2 и 3 */
    GPIOA->AFR[0] |= (1 << GPIO_AFRL_AFRL2_Pos) | (1 << GPIO_AFRL_AFRL3_Pos);

    /* Включить режим альтернативной функции на линиях PA2 и PA3 */
    GPIOA->MODER |=
        GPIO_MODER_MODER2_1 | GPIO_MODER_MODER3_1;

    /* Установить коэффициент деления.
       BRR = fbus / baudrate
       Пусть baudrate = 9600 бод, частота шины fbs = 8 МГц,
       тогда BRR = 8000000 / 9600 */
    USART2->BRR = 8000000 / 9600;
    /* Включить передатчик */
    USART2->CR1 |= USART_CR1_TE;
    /* Включить приемник */
    USART2->CR1 |= USART_CR1_RE;
	
    /* Включить USART */
    USART2->CR1 |= USART_CR1_UE;

    /* Чтение регистра данных для сброса флагов */
    uint16_t dummy = USART2->RDR;
}

/* Функция передачи байта в бесконечном цикле */
void usart_transmit(uint8_t data)
{
    /* Записать байт в регистр данных */
    USART2->TDR = data;
    /* Ожидание флага окончания передачи TC (Transmission Complete) */
    while(!(USART2->ISR & USART_ISR_TC));
}

/* Функция приема байта в бесконечном цикле */
uint8_t usart_receive(void)
{
    /* Ожидание флага буфер приемника не пуст RXNE (Receiver buffer not empty) */
    while(!(USART2->ISR & USART_ISR_RXNE));
    /* Чтение принятого байта и возврат из функции */
    return USART2->RDR;
}

/* Функция сравнения двух строк.
   Строки представлены в ASCII кодировке.
   Строки являются нуль терминированными - код окончания строки - 0 (ноль).
   В стандартной библиотеке C уже есть функция strcmp.
   В примере для учебных целей показана простейшая реализация
   такой функции. */
int32_t _strncmp(const char s1[], const char s2[], uint32_t n)
{
    int32_t i = 0;

    while (s1[i] != '\0' && s2[i] != '\0' && --n > 0)
    {
        if (s1[i] != s2[i])
        {
            break;
        }
        i++;
    }

    return s1[i] - s2[i];
}

/* Функция преобразования строки в число.
   В стандартной библиотеке C уже есть функция atoi.
   В примере для учебных целей показана простейшая реализация
   такой функции. */
int32_t _atoi(const char s[])
{
    int32_t n = 0;
    int32_t i = 0;

    while (s[i] != '\0' && s[i] >= '0' && s[i] <= '9')
    {
        n *= 10;
        n += s[i] - '0';
        i++;
    }

    return n;
}

/* Функция передачи строки */
void putstr(char* str)
{
    while (*str != '\0')
    {
        usart_transmit(*str);
        str++;
    }
}


char int_to_char (uint8_t s){
	return (s+0x30);
}

int main(void) {

    timer_init();
    usart_init();
    char buf[20] = {0};
    int32_t pos = 0;
    while (1) {
        char ch = usart_receive();
        if (ch != '\r'){
            if (pos < 20){
                buf[pos] = ch;
                pos++;
            }
        }
        else
        {
            if (_strncmp(buf, "STOP", 4) == 0){	
				finishNamber = data;
				usart_transmit(int_to_char((finishNamber%10000)/1000));
				usart_transmit(int_to_char((finishNamber%1000)/100));
				usart_transmit(int_to_char((finishNamber%100)/10));
				usart_transmit(int_to_char(finishNamber%10));
				putstr(" CEK.\n");		
            } else if (_strncmp(buf, "START", 5) == 0){
				putstr("START\n");
				data=0;
            } else {  
                putstr("ERROR\n");
            }
            pos = 0;
            for(int i = 0; i < 20; i++) {
                buf[i] = '\0';
            }
        }
    }
}