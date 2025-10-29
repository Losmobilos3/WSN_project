#include <stdio.h>
#include "contiki.h"
#include "net/nullnet/nullnet.h"
#include "net/netstack.h"

PROCESS(process, "p");
AUTOSTART_PROCESSES(&process);

static linkaddr_t dest_addr = {{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 }};

// TODO: Set dynamically?
static linkaddr_t own_addr = {{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }};

void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest)
{
  if(len == sizeof(unsigned)) {
    unsigned count;
    memcpy(&count, data, sizeof(count));
    LOG_INFO("Received %u from ", count);
    LOG_INFO_LLADDR(src);
    LOG_INFO_("\n");
  }
}

PROCESS_THREAD(process, ev, data)
{
    PROCESS_BEGIN();
    
    uint8_t payload[64] = { 0 };
    nullnet_buf = payload;
    nullnet_len = 2; 

    static linkaddr_t dest_addr = {{0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11}};

    PROCESS_END();
}