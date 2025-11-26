#include <stdio.h>
#include "contiki.h"
#include "sys/clock.h"
#include "net/nullnet/nullnet.h"
#include "net/netstack.h"
#include "net/linkaddr.h"
#include "sys/etimer.h"
#include "ARQ_protocols.h"

PROCESS(process, "p");
AUTOSTART_PROCESSES(&process);

// This nodes address is contained in linkaddr_node_addr

static linkaddr_t dest_addr = {{0x03, 0x03, 0x03, 0x00, 0x03, 0x74, 0x12, 0x00}};

void input_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest)
{
    // printf("Received message from ");
    // for(int i = 0; i < LINKADDR_SIZE; i++) {
    //     printf("%02x", src->u8[i]);
    // }
    // printf(": ");
    // for(int i = 0; i < len; i++) {
    //     printf("%02x ", ((uint8_t *)data)[i]);
    // }
    // printf("\n");
    // TODO: If ACK
    bool ACK = true; // TODO: make dynamic
    
    if (ACK) {
        ACK_received = true;
        // TODO: end timer if possible
        //   etimer_stop(struct etimer *t)
        // need to reset at some point? etimer_reset(struct etimer *t) etimer_restart(struct etimer *t)
    } else { // Normal message

    }
}

PROCESS_THREAD(process, ev, data)
{
    static struct etimer timer;

    PROCESS_BEGIN();

    /* At process initialization */
    nullnet_set_input_callback(input_callback);

    // Wait for all motes to begin listening
    etimer_set(&timer, CLOCK_SECOND * 5);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));

    uint8_t payload[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    nullnet_buf = payload;
    nullnet_len = 8; 
    NETSTACK_NETWORK.output(&dest_addr); 


    PROCESS_END();
}