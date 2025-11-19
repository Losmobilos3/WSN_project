#include <stdio.h>
#include <string.h>
#include "contiki.h"
#include "net/nullnet/nullnet.h"
#include "net/netstack.h"
#include "net/linkaddr.h"
#include "sleep.h"

PROCESS(process, "p");
AUTOSTART_PROCESSES(&process);

// This nodes address is contained in linkaddr_node_addr

static linkaddr_t dest_addr = {{0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest)
{
    printf("Received message from ");
    for(int i = 0; i < LINKADDR_SIZE; i++) {
        printf("%02x", src->u8[i]);
    }
    printf(": ");
    for(int i = 0; i < len; i++) {
        printf("%02x ", ((uint8_t *)data)[i]);
    }
    printf("\n");
}


PROCESS_THREAD(process, ev, data)
{
    PROCESS_BEGIN();

    /* At process initialization */
    nullnet_set_input_callback(input_callback);

    sleep(5);

    uint8_t payload[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    nullnet_buf = payload;
    nullnet_len = 8; 
    NETSTACK_NETWORK.output(&dest_addr); /* Send as broadcast */


    PROCESS_END();
}