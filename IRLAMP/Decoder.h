/*
 * Decoder.h
 *
 * Created: 10/6/2021 11:43:44 PM
 *  Author: ashan
 */ 


#ifndef Decoder_H_
#define Decoder_H_

#define INPUT_PORT PORTD
#define INPUT_DDR DDRD
#define INPUT_PIN PD2
#define INPUT_PIN_PORT PIND

extern init_decoder();
extern uint8_t check_new_packet(struct IRPacket * received_packet);

struct IRPacket
{
	uint16_t addr;
	uint8_t command;
	uint8_t repeat;
};

#endif /* Decoder_H_ */
