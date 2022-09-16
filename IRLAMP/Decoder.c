/*
 * Decoder.c
 *
 * Created: 10/6/2021 11:44:21 PM
 *  Author: ashan
 */ 

#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "Decoder.h"

#define CYCLES_PER_1MS 250 
#define MAX_DELAY_FOR_REPEAT 120 
#define MAX_DELAY 16 
#define MAX_BITS 32

#define START 0  
#define READ 1       
#define PACKET_READY 2         

#define MAX_REPEAT 255
#define REPEAT_ALLOWED 1
#define REPEAT_UNALLOWED 0

static uint8_t repeat_allow_or_not = REPEAT_UNALLOWED;
static uint8_t state = START;
static uint8_t bit_counter = 0;
volatile static	uint16_t pulse_len_counter = 0;
volatile static uint16_t space_len_counter = 0;

static uint8_t remote_id;
static uint8_t remote_id_inverse;
static uint16_t remote_ext_id;
static uint8_t remote_command;
static uint8_t remote_command_inverse;
static uint8_t remote_repeat;

void init_Input_PIN();
void start_timer(uint8_t time_ms);
void stop_timer();
void reset_decoder();
void clear_buffers();
void AGC_burst();
void packet_ready();
void data(uint8_t bit);
void repeat();
void read_bit();
extern uint8_t get_packet(struct IRPacket * packet);
extern init_decoder();


void init_Input_PIN()      // setting up pin change interrupts
{
	INPUT_DDR &= ~(1 << INPUT_PIN);
	INPUT_PORT |= (1 << INPUT_PIN);
	PCMSK2 = (1 << PCINT18);
	PCICR = (1 << PCIE2);
}

void start_timer(uint8_t time_ms)     // start time with time outs
{
	TCNT1 = 0x00;
	TCCR1A = 0x00;
	TCCR1B |=  (1 << CS10) | (1 << CS11) | (1 << WGM12);    // ctc mode wit clk / 64 prescaler
	OCR1A = CYCLES_PER_1MS * time_ms;
	TIMSK1  |=  1 << OCIE1A;
}

void stop_timer()      // stop timer by selecting no clock source
{
	TCCR1B &= ~(1 << CS10) | ~(1 << CS11) | ~(1 << CS12);    
}

void reset_decoder()     // reseting decoder by clearing buffers and pulse and space counters
{	
	pulse_len_counter = 0;
	space_len_counter = 0;
	stop_timer();
	clear_buffers();
	state = START;
	repeat_allow_or_not = REPEAT_UNALLOWED;
}

void clear_buffers()     // clearing buffers
{
	bit_counter = 0;
	remote_id = 0;
	remote_id_inverse = 0;
	remote_command = 0;
	remote_command_inverse = 0;
	remote_repeat = 0;
	remote_ext_id = 0;
}

void AGC_burst()      // at the start pulse
{
	clear_buffers();
	state = READ;
}

void packet_ready()     // when packet ready set timer time out for repeat
{	
	bit_counter = 0;
	state = PACKET_READY;
	repeat_allow_or_not = REPEAT_ALLOWED;
	start_timer(MAX_DELAY_FOR_REPEAT);
}

void data(uint8_t bit)    // assigning 8 bits of data to revelent fields
{	
	if ((bit_counter < MAX_BITS) && (state == READ))
	{
		if (bit != 0)
		{			
			if (bit_counter < 8)  // first 8 bits are address
			{
				remote_id |= (1<<bit_counter);
			} else if (bit_counter >=8 && bit_counter < 16)  // second 8 bits are complement of address
			{
				remote_id_inverse |= (1<<(bit_counter - 8));
			} else if (bit_counter >= 16 && bit_counter < 24)   // third 8 bits are the command 
			{	
				remote_command |= (1<<(bit_counter - 16));    // forth 8 bits are for the complement of command
			} else if (bit_counter >= 24) 
			{
				remote_command_inverse |= (1<<(bit_counter - 24));
			}
		}				
		bit_counter++;
		if (bit_counter == MAX_BITS) // if all bits are received
		{				
			
			if ((remote_command + remote_command_inverse) == 0xFF)  // error checking of command bits
			{
				packet_ready();
			} 
			else    // reset decoder and discard packet
			{	
				reset_decoder(); 
			}
		} 
	}		
}


void repeat()   // capture repeat count
{
	if(repeat_allow_or_not == REPEAT_ALLOWED && (state == PACKET_READY || state == START))
	{
		if (remote_repeat < MAX_REPEAT)
		{			
			remote_repeat++; 
		}
		state = PACKET_READY;
		start_timer(MAX_DELAY_FOR_REPEAT);
	} 
	else 
	{
		reset_decoder();  // reset decoder and clear buffers
	}
}


void read_bit()
{
	if (pulse_len_counter > 0 && space_len_counter > 0)
	{	
		if (pulse_len_counter > (7 * CYCLES_PER_1MS) && pulse_len_counter < (11 * CYCLES_PER_1MS))   // capture if pulse is AGC burst or repeat (9.5ms)
		{			
			if (space_len_counter > (3.2 * CYCLES_PER_1MS) && space_len_counter < (6 * CYCLES_PER_1MS)) // 4,5ms space time if new packet
			{
				AGC_burst();
			} 
			else if (space_len_counter > (1.6 * CYCLES_PER_1MS) && space_len_counter <= (3.2 * CYCLES_PER_1MS)) // 2.25ms space time if repeat
			{
				repeat();
			}						
		}
		else if (pulse_len_counter > 0.36 * CYCLES_PER_1MS && pulse_len_counter < (0.76 * CYCLES_PER_1MS))  // 560 microseconds of pulse if logical 1 or 0 and capture
		{
			if (space_len_counter > (1.5 * CYCLES_PER_1MS) && space_len_counter < (1.9 * CYCLES_PER_1MS))   // 1.75ms of space time if logical 1
			{	
				data(1);
			} 
			else if (space_len_counter > (0.36 * CYCLES_PER_1MS) && space_len_counter < (0.76 * CYCLES_PER_1MS))   // 560 microseconds of space time if logical 0
			{
				data(0);
			}
		} 
		else
		{
			reset_decoder();  // if captured pulse lengths and space lengths miss match discard packet and reset decoder
		}
	}
	pulse_len_counter = 0;
	space_len_counter = 0;
}

extern uint8_t get_packet(struct IRPacket * packet)  // struct to export captured packet 
{
	if (state == PACKET_READY)
	{	
		packet->addr = remote_id;
		packet->command = remote_command;
		packet->repeat = remote_repeat;
		state = START;	
		return 1;
	}
	return 0;
}

extern init_decoder()  
{
	char cSREG;     
	cSREG = SREG;    // copy status register
	cli();
	init_Input_PIN();    // initialize ir reciver connected pin
	reset_decoder();
	SREG = cSREG;
}

ISR(PCINT2_vect)     // vector for pin change
{
	if (state == PACKET_READY)
	{
		return;
	}
	uint8_t capture_edge = (INPUT_PIN_PORT & (1<<INPUT_PIN));
	if (capture_edge == 0)   // if space
	{				
		if (pulse_len_counter == 0)     
		{	
			start_timer(MAX_DELAY);		// set max timeout to capture AGC burst
		} 
		else 
		{		
			uint16_t time = TCNT1;
			space_len_counter = time - pulse_len_counter;   // measure space time
			start_timer(MAX_DELAY);
			read_bit();
		}
	} 
	else     // if a pulse
	{			
		pulse_len_counter = TCNT1;      // measure pulse time
		if(pulse_len_counter == 0)      // if no pulse captured rest decoder and discard packet
		{
			reset_decoder();     
		}
	}
}

ISR(TIMER1_COMPA_vect)   //  resetting timer if time out
{
	stop_timer(); 
	if (state == PACKET_READY) // if packet is ready no repeat allowed becasue max repeat time is exceeded
	{
		repeat_allow_or_not = REPEAT_UNALLOWED;
	}
	else
	{
		reset_decoder();   //if time out discard packet
	}	
}



