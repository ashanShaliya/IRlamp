/*
 * main.c
 *
 * Created: 10/1/2021 5:27:50 PM
 *  Author: ashan
 */ 
#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>
#include "Decoder.h"

struct IRPacket packet;
uint8_t led_counter;
uint8_t timer0_duty;  // duty cycle 


void PWM_int();
void PWM_increase_duty();
void PWM_decrease_duty();
void LED_up();


void PWM_int()  // initialize timer0 for CTC mode 
{
	TCCR0A = 1 << WGM01;
	TCCR0B = 0b00000010; // prescaler(clk/64)
	TIMSK0 = (1 << OCIE0A) | (1 << OCIE0B);
	OCR0A = 255;
	OCR0B = timer0_duty;
}

void PWM_increase_duty()
{
	if(timer0_duty == 250)
	{
		timer0_duty = 250;
	}
	else
	{
		timer0_duty = timer0_duty + 25;	
	}
	OCR0B = timer0_duty;
}

void PWM_decrease_duty()
{
	if(timer0_duty == 0)
	{
		led_counter--;
		timer0_duty = 250;
	}
	else{
		timer0_duty = timer0_duty - 25;	
	}
	OCR0B = timer0_duty;
}

void LED_up()
{
	if(led_counter == 1)
	{
		PORTB = 0x01;
	}
	else if (led_counter == 2)
	{
		PORTB = 0x03;
	}
	else if(led_counter == 3)
	{
		PORTB = 0x07;
	}
	else if(led_counter == 4)
	{
		PORTB = 0x0f;
	}
	else if((led_counter >= 5) && (led_counter <= 7))
	{
		PORTC = 1 << led_counter - 5;
	}
	else if(led_counter != 0)	
	{
		PORTC = 1 << led_counter - 8;
	}
}

ISR(TIMER0_COMPA_vect) //ISR for Timer0 compare match A
{
	LED_up();
}

ISR(TIMER0_COMPB_vect) // ISR for Timer0 compare match B
{	
	PORTB = 0x00;
	PORTC = 0x00;
}

int main(void)
{	
	DDRB |= 0x0f;
	DDRC |= 0x07;
	timer0_duty = 250;
	led_counter = 0;
	init_decoder();  // initialize timer1 for ir capture
	
	sei();  
	
	while (1)
	{
		cli();
		uint8_t packet_check = get_packet(&packet); 
		sei();
		
		if (packet_check != 0)
		{
			if (packet.repeat > 0)  // if signal is repeated
			{
				if(packet.command == 0x86) // reduce brightness
				{
					PWM_decrease_duty();
					_delay_ms(100);
				}
				if(packet.command == 0x83)  // increase brightness
				{
					PWM_increase_duty();
					_delay_ms(100);
				}
			} 
			else   // if not repeated
			{
				if(packet.command == 0x80)  // turn on LEDs
				{	
					if(led_counter == 10)
					{
						led_counter = 10;
					}
					else
					{
						led_counter++;
						PWM_int();
					}	
				}
				else if(packet.command == 0x86) // bright decrease
				{
					PWM_decrease_duty();
				}
				else if(packet.command == 0x8e)  // turn off LEDs 
				{
					if(led_counter == 0)
					{
						led_counter = 0;
					}
					else
					{
						led_counter--;
					}
				}
				else if(packet.command == 0x83)  // increase brightness
				{
					PWM_increase_duty();
				}
			}
		}
	}
}