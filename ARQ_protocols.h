#ifndef ARQ_PROTOCOLS_H_
#define ARQ_PROTOCOLS_H_

#include <stdint.h>
#include <stdbool.h>
#include "sys/etimer.h"
#include "net/nullnet/nullnet.h"
#include "net/netstack.h"
#include "contiki.h"

struct etimer ack_timer; 

uint8_t ACK_received;

uint8_t stop_and_wait(uint8_t* package, uint8_t len, linkaddr_t dest_addr, int max_tries, int trip_time);

#endif