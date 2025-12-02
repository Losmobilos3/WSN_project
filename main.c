#include <stdio.h>
#include "contiki.h"
#include "sys/clock.h"
#include "net/nullnet/nullnet.h"
#include "net/netstack.h"
#include "net/linkaddr.h"
#include "sys/etimer.h"
#include <stdbool.h>
// #include "dev/moteid.h"
#include "sys/node-id.h"
#include <stdlib.h>

#define max_tries 5
#define REPEATS 100

uint8_t eps = 10; // small time to account for processing delays

process_event_t acknowledgement;
process_event_t exp_done;

// This nodes address is contained in linkaddr_node_addr
static linkaddr_t dest_addr = {{0x03, 0x03, 0x03, 0x00, 0x03, 0x74, 0x12, 0x00}};

static int n_ack;
static int n_attempts;

// ---------------- Message functions ----------------
void send_ack(const linkaddr_t *addr) {
    // Set data to send
        uint8_t payload[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }; // Starts with 1
        nullnet_buf = payload;
        nullnet_len = 8; 

        // Sending logic here
        NETSTACK_NETWORK.output(addr); 
}

void send_msg(const linkaddr_t *addr) {
    // Set data to send
        uint8_t payload[8] = { 0, 1, 2, 3, 4, 5, 6, 7 }; // Starts with 0
        nullnet_buf = payload;
        nullnet_len = 8; 

        // Sending logic here
        NETSTACK_NETWORK.output(addr); 
}


// ---------------- Processes ----------------
PROCESS(main_process, "main process");
PROCESS(no_relay_process, "no_relay_process");
AUTOSTART_PROCESSES(&main_process);

bool addr_equal(const linkaddr_t *a, const linkaddr_t *b) {
    for(int i = 0; i < LINKADDR_SIZE; i++) {
        if(a->u8[i] != b->u8[i]) {
            return false;
        }
    }
    return true;
}

// ---------------- Input callback --------------
void input_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest)
{
    // Skip packages not for self
    if (dest != NULL && !addr_equal(dest, &linkaddr_node_addr)) {
        printf("Skipped!");
        return;
    }

    printf("Received message from ");
    for(int i = 0; i < LINKADDR_SIZE; i++) {
        printf("%02x", src->u8[i]);
    }
    printf(": ");
    for(int i = 0; i < len; i++) {
        printf("%02x ", ((uint8_t *)data)[i]);
    }
    printf("\n");

    // Check if message is ACK
    bool ACK = (((uint8_t *)data)[0] == 1); // Simple check for ACK
    
    printf("ACK STATUS: %d \n", ACK);

    if (ACK) {
        printf("Received ACK\n");
        // process_start(&no_relay_process);
        process_post(&no_relay_process, acknowledgement, NULL);
    } else { // Normal message
        // Make copy of src
        linkaddr_t* src_copy = malloc(sizeof(linkaddr_t));
        for(int i = 0; i < len; i++) {
            src_copy->u8[i] = src->u8[i];
        }

        send_ack(src_copy);
        
        free(src_copy);
    }
}

// ---------------- Processes ----------------
PROCESS_THREAD(main_process, ev, data)
{
    static struct etimer timer;
    static int i;

    PROCESS_BEGIN();
    
    // Allocate event once
    acknowledgement = process_alloc_event();
    exp_done = process_alloc_event();

    /* At process initialization */
    nullnet_set_input_callback(input_callback); // Setup the input callback (IS ALREADY A PROTOTHREAD?!?)
    etimer_set(&timer, CLOCK_SECOND * 5);

    // Run the experiment REPEATS times.
    printf("Node ID: %u\n", node_id);
    if (node_id == 1) {
        n_ack = 0;
        n_attempts = 0;
        for (i = 0; i < REPEATS; i++) {
            // Wait for all motes to begin listening
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
            printf("Starting experiment %d/%d.\n", i + 1, REPEATS);

            // process_poll(&send_process); // Sends the message
            process_start(&no_relay_process, NULL); // Starts the no_relay_process
            PROCESS_WAIT_EVENT_UNTIL(exp_done == ev); // Wait until experiment is done
            
            printf("Experiment %d/%d done.\n", i + 1, REPEATS);
            etimer_set(&timer, CLOCK_SECOND * 5);
        }
        printf("ACKs received: %d out of %d attempts.\n", n_ack, n_attempts);
    }

    PROCESS_END();
}

// ---------------- Relay Protocols ----------------
PROCESS_THREAD(no_relay_process, ev, data)
{
    static struct etimer ack_timer;
    static int attempt = 0;
    PROCESS_BEGIN();

    printf("STARTED NO RELAY PROCESS\n");
        
    for (attempt = 0; attempt < max_tries; attempt++) {

        // Send message
        send_msg(&dest_addr);
        
        // Start etimer and wait for ACK
        etimer_set(&ack_timer, 2*CLOCK_SECOND*5+eps);
        PROCESS_WAIT_EVENT_UNTIL(acknowledgement == ev || etimer_expired(&ack_timer));

        n_attempts++;
        if (ev == acknowledgement) {
            // ACK received
            n_ack++;
            printf("ACK received, stopping retransmissions.\n");
            break;
        } else {
            // Timeout occurred, no ACK received
            printf("No ACK received, attempt %d/%d\n", attempt + 1, max_tries);
        }
    }
    process_post(&main_process, exp_done, NULL);
    PROCESS_END();
}