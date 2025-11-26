#include "ARQ_protocols.h"

struct etimer ack_timer; // TODO: maybe should be static

bool ACK_received = false;

uint8_t eps = 10; // small time to account for processing delays

uint8_t stop_and_wait(uint8_t* package, uint8_t len, linkaddr_t dest_addr, int max_tries, int trip_time) { // Assumption made about symmetric channel
    for (int attempt = 0; attempt < max_tries; attempt++) {
        ACK_received = false;
        // Send the package
        nullnet_buf = package; // TODO: Prepend with msg-type byte
        nullnet_len = len+1; 
        NETSTACK_NETWORK.output(&dest_addr); 

        // Start etimer and wait for ACK
        etimer_set(&ack_timer, 2*trip_time+eps);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&ack_timer));
        if (ACK_received) {
            return 0; // Message sent successfully
        }
    }
    return 1; // Message failed to send after max tries
}