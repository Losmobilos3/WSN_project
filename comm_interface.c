#include "net/nullnet/nullnet.h"
#include "net/netstack.h"

void send_msg(uint8_t* msg, int len, linkaddr_t src_addr, linkaddr_t dest_addr) {
    nullnet_buf = msg; /* Point NullNet buffer to 'payload' */
    nullnet_len = len; /* Tell NullNet that the payload length is two bytes */
    NETSTACK_NETWORK.output(&dest_addr); /* Send as broadcast */
}

void listen_for_msg(nullnet_input_callback callback) {
    nullnet_set_input_callback(callback);
}