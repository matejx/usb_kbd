/**
USB HID keyboard using STM32F103

@file		main.c
@author		Matej Kogovsek
@copyright	GPL v2
*/

#include <stm32f10x.h>

//#include "mat/serialq.h"

#include <string.h>
#include <ctype.h>

//#define DBG_UART 1

#include "usb_lib.h"
#include "usb-kbd/usb_endp.h"

void usb_hwinit(void);

//-----------------------------------------------------------------------------
//  KEY definitions
//-----------------------------------------------------------------------------

#define NKEYS 26

const uint8_t keypin[NKEYS] = {
	0,1,2,3,4,5,6,7,8, 9, 10,15,			// PA0..PA10, PA15
	0,1,4,5,6,7,8,9,10,11,12,13,14,15		// PB0, PB1, PB4..PB15
};

GPIO_TypeDef* const keyport[NKEYS] = {
	GPIOA,GPIOA,GPIOA,GPIOA,GPIOA,GPIOA,GPIOA,GPIOA,GPIOA,GPIOA,GPIOA,GPIOA,
	GPIOB,GPIOB,GPIOB,GPIOB,GPIOB,GPIOB,GPIOB,GPIOB,GPIOB,GPIOB,GPIOB,GPIOB,GPIOB,GPIOB
};

const uint8_t keycode[NKEYS] = { // map pins to keyboard "scan codes" (from HID usage tables v1.12)
	4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29 // a..z
};

//-----------------------------------------------------------------------------
//  Global variables
//-----------------------------------------------------------------------------

#ifdef DBG_UART
static uint8_t dbgtxbuf[128];
static uint8_t dbgrxbuf[32];
#endif

volatile uint32_t msTicks;

//-----------------------------------------------------------------------------
//  newlib required functions
//-----------------------------------------------------------------------------

void _exit(int status) { while(1) {} }

//-----------------------------------------------------------------------------
//  SysTick handler
//-----------------------------------------------------------------------------

void SysTick_Handler(void) { msTicks++; } // _delay_ms()

//-----------------------------------------------------------------------------
//  delay functions
//-----------------------------------------------------------------------------

void _delay_ms(uint32_t ms)
{
	uint32_t curTicks = msTicks;
	while ((msTicks - curTicks) < ms);
}

void _delay_us(uint32_t us)
{
    us *= 8;

    asm volatile("mov r0, %[us]             \n\t"
                 "1: subs r0, #1            \n\t"
                 "bhi 1b                    \n\t"
                 :
                 : [us] "r" (us)
                 : "r0"
	);
}

//-----------------------------------------------------------------------------
//  utility functions
//-----------------------------------------------------------------------------

void DDR(GPIO_TypeDef* port, uint16_t pin, GPIOMode_TypeDef mode)
{
	GPIO_InitTypeDef iotd;
	iotd.GPIO_Pin = pin;
	iotd.GPIO_Speed = GPIO_Speed_2MHz;
	iotd.GPIO_Mode = mode;
	GPIO_Init(port, &iotd);
}

//-----------------------------------------------------------------------------
//  MAIN function
//-----------------------------------------------------------------------------

int main(void)
{
	usb_hwinit();

	if( SysTick_Config(SystemCoreClock / 1000) ) { // setup SysTick Timer for 1 msec interrupts
		while( 1 );                                  // capture error
	}
	//NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0); // disable preemption

	kbd_init();

	USB_Init(); // suspends device if no USB connected

#ifdef DBG_UART
	#warning check that dbg uart is not using pins assigned to keys
	ser_printf_devnum = DBG_UART;
	ser_init(DBG_UART, 115200, dbgtxbuf, sizeof(dbgtxbuf), dbgrxbuf, sizeof(dbgrxbuf));
#endif

	uint8_t i,j;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE); // frees PA15, PB3, PB4

	for( i = 0; i < NKEYS; ++i ) { // configure key pins as input with pull up
		DDR(keyport[i], 1<<keypin[i], GPIO_Mode_IPU);
	}

	while( 1 ) {
		int8_t keystate[NKEYS];
		memset(keystate, 0, NKEYS); // zero keystate

		for( i = 0; i < 100; ++i ) { // 100 * 100us = 10ms (EP1 poll rate)
			_delay_us(100);

			for( j = 0; j < NKEYS; ++j ) {
				if( Bit_RESET == GPIO_ReadInputDataBit(keyport[j], 1<<keypin[j]) ) { // key pressed
					if( keystate[j] < 0 ) keystate[j] = 0;
					++keystate[j];
				} else { // key not pressed
					if( keystate[j] > 0 ) keystate[j] = 0;
					--keystate[j];
				}
			}
		}

		for( i = 0; i < NKEYS; ++i ) {
			if( keystate[i] > 70 ) kbd_down(keycode[i]);
			if( keystate[i] < -50 ) kbd_up(keycode[i]);
		}

		kbd_tx();
	}
}
