#ifndef COMM_IF
#define COMM_IF

void send_msg(uint8_t msg, int len, linkaddr_t src_addr, linkaddr_t dest_addr);

void listen_for_msg(nullnet_input_callback callback);

#endif