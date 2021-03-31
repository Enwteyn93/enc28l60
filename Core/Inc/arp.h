#ifndef ARP_H_
#define ARP_H_
//--------------------------------------------------
#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "enc28j60.h"
#include "net.h"
//--------------------------------------------------
uint8_t arp_read(enc28j60_frame_ptr *frame, uint16_t len);
void arp_send(enc28j60_frame_ptr *frame);
//--------------------------------------------------
#endif /* ARP_H_ */
