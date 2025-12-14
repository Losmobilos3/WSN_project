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

typedef enum {
    NO_RELAY,
    N_PACKET_RELAY,
    EWMA_N_PACKET_RELAY
} protocol;

protocol selected_protocol = EWMA_N_PACKET_RELAY; // Change protocol here

uint8_t eps = 10; // small time to account for processing delays

process_event_t acknowledgement;
process_event_t exp_done;

// This nodes address is contained in linkaddr_node_addr
static linkaddr_t node_a_addr = {{0x01, 0x01, 0x01, 0x00, 0x01, 0x74, 0x12, 0x00}};
static linkaddr_t dest_addr = {{0x03, 0x03, 0x03, 0x00, 0x03, 0x74, 0x12, 0x00}};
static linkaddr_t relay_addr = {{0x02, 0x02, 0x02, 0x00, 0x02, 0x74, 0x12, 0x00}};

static int n_ack;
static int n_attempts;
static int n_relay_ack;
static int n_relay_attempts;

static int ewma_estimate = 0;
static int alpha = 97;

// ---------------- Message functions ----------------
void send_ack(const linkaddr_t *addr) {
    // Set data to send
    uint8_t payload[1] = { 1 }; // Starts with 1
    nullnet_buf = payload;
    nullnet_len = 1; 

    // Sending logic here
    NETSTACK_NETWORK.output(addr); 
}

void send_msg(const linkaddr_t *addr) {
    // Set data to send
    uint8_t payload[128]; 
    // Init message
    for (int i = 0; i < 128; i++) {
        payload[i] = (uint8_t)(i); // Starts from 0
    }

    nullnet_buf = payload;
    nullnet_len = 8; 

    // Sending logic here
    NETSTACK_NETWORK.output(addr); 
}


// ---------------- Processes ----------------
PROCESS(main_process, "main process");
PROCESS(no_relay_process, "no_relay_process");
PROCESS(n_packet_relay_process, "n_packet_relay_process");
PROCESS(EWMA_n_packet_relay_process, "n_packet_relay_process");
AUTOSTART_PROCESSES(&main_process);

bool addr_equal(const linkaddr_t *a, const linkaddr_t *b) {
    for(int i = 0; i < LINKADDR_SIZE; i++) {
        if(a->u8[i] != b->u8[i]) {
            return false;
        }
    }
    return true;
}

// ---------------- Input callback N_RELAY --------------
void input_callback_no_relay(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest)
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

// ---------------- Input callback N_PACKET_RELAY --------------
void input_callback_n_packet_relay(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest)
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
        // If B received ACK
        if (addr_equal(&linkaddr_node_addr, &relay_addr)) {
            printf("Forwarding ACK to node A\n");
            send_ack(&node_a_addr);
        } else { // Else A received ACK
            // Activate main process with acknowledgement event
            if (selected_protocol == N_PACKET_RELAY) {
                process_post(&n_packet_relay_process, acknowledgement, NULL);
            } else if (selected_protocol == EWMA_N_PACKET_RELAY) {
                process_post(&EWMA_n_packet_relay_process, acknowledgement, NULL);
            }
        }
    // Normal message
    } else {
        // If B received message
        if (addr_equal(&linkaddr_node_addr, &relay_addr)) {
            // If message from relay, send ACK back to relay
            printf("Forwarding message to node C\n");
            send_msg(&dest_addr);
            return;
        } else { // else C received message
            // Make copy of src
            linkaddr_t* src_copy = malloc(sizeof(linkaddr_t));
            for(int i = 0; i < len; i++) {
                src_copy->u8[i] = src->u8[i];
            }

            send_ack(src_copy);
            
            free(src_copy);
        }
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
    if (selected_protocol == NO_RELAY) {
        nullnet_set_input_callback(input_callback_no_relay); // Setup the input callback (IS ALREADY A PROTOTHREAD?!?)
    } else if (selected_protocol == N_PACKET_RELAY) {
        nullnet_set_input_callback(input_callback_n_packet_relay); // Setup the input callback (IS ALREADY A PROTOTHREAD?!?)
    } else if (selected_protocol == EWMA_N_PACKET_RELAY) {
        nullnet_set_input_callback(input_callback_n_packet_relay); // Setup the input callback (IS ALREADY A PROTOTHREAD?!?)
    }
    etimer_set(&timer, CLOCK_SECOND * 5);

    // Run the experiment REPEATS times.
    printf("Node ID: %u\n", node_id);
    if (node_id == 1) {
        n_ack = 0;
        n_relay_ack = 0;
        n_attempts = 0;
        n_relay_attempts = 0;
        for (i = 0; i < REPEATS; i++) {
            // Wait for all motes to begin listening
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
            printf("Starting experiment %d/%d.\n", i + 1, REPEATS);

            if (selected_protocol == NO_RELAY) {
                process_start(&no_relay_process, NULL); // Starts the no_relay_process
            } else if (selected_protocol == N_PACKET_RELAY) {
                process_start(&n_packet_relay_process, NULL); // Starts the n_packet_relay_process
            } else if (selected_protocol == EWMA_N_PACKET_RELAY) {
                process_start(&EWMA_n_packet_relay_process, NULL); // Starts the n_packet_relay_process
            }
            PROCESS_WAIT_EVENT_UNTIL(exp_done == ev); // Wait until experiment is done
            
            printf("Experiment %d/%d done.\n", i + 1, REPEATS);
            etimer_set(&timer, CLOCK_SECOND * 5);
        }
        printf("A to C: %d out of %d attempts were successful. \n", n_ack, n_attempts);
        printf("Relay to C: %d out of %d attempts were successful. \n", n_relay_ack, n_relay_attempts);
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

PROCESS_THREAD(n_packet_relay_process, ev, data)
{
    static struct etimer ack_timer;
    static int attempt = 0;
    PROCESS_BEGIN();

    printf("STARTED N-RELAY PROCESS\n");
        
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
    if (attempt == max_tries) {
        for (attempt = 0; attempt < max_tries; attempt++) {
            // After max tries, send to relay
            send_msg(&relay_addr);
            
            // Start etimer and wait for ACK
            etimer_set(&ack_timer, 4*CLOCK_SECOND*5+2*eps);
            PROCESS_WAIT_EVENT_UNTIL(acknowledgement == ev || etimer_expired(&ack_timer));

            n_relay_attempts++;
            if (ev == acknowledgement) {
                // ACK received
                n_relay_ack++;
                printf("ACK received from relay.\n");
                break;
            } else {
                // Timeout occurred, no ACK received
                printf("No ACK received from relay after max tries.\n");
            }
        }
    }
    process_post(&main_process, exp_done, NULL);
    PROCESS_END();
}

PROCESS_THREAD(EWMA_n_packet_relay_process, ev, data)
{
    static struct etimer ack_timer;
    static int attempt = 0;
    static bool atoc_skipped = false;
    PROCESS_BEGIN();

    printf("STARTED EWMA RELAY PROCESS\n");
        
    for (attempt = 0; attempt < max_tries; attempt++) {
        if (attempt != 0 && ewma_estimate < 30) { // 30%
            atoc_skipped = true;
            break; // Go straight to relay
        } 

        // Send message
        send_msg(&dest_addr);
        
        // Start etimer and wait for ACK
        etimer_set(&ack_timer, 2*CLOCK_SECOND*5+eps);
        PROCESS_WAIT_EVENT_UNTIL(acknowledgement == ev || etimer_expired(&ack_timer));

        n_attempts++;
        if (ev == acknowledgement) {
            // Update EWMA estimate
            int mu_n = (n_ack * 100) / n_attempts;
            ewma_estimate = ((100-alpha)*mu_n + alpha*ewma_estimate) / 100; // Should be stored in %

            // ACK received
            n_ack++;
            printf("ACK received, stopping retransmissions.\n");
            break;
        } else {
            // Timeout occurred, no ACK received
            printf("No ACK received, attempt %d/%d\n", attempt + 1, max_tries);
        }
    }
    if (atoc_skipped || attempt == max_tries) {
        for (attempt = 0; attempt < max_tries; attempt++) {
            // After max tries, send to relay
            send_msg(&relay_addr);
            
            // Start etimer and wait for ACK
            etimer_set(&ack_timer, 4*CLOCK_SECOND*5+2*eps);
            PROCESS_WAIT_EVENT_UNTIL(acknowledgement == ev || etimer_expired(&ack_timer));

            n_relay_attempts++;
            if (ev == acknowledgement) {
                // ACK received
                n_relay_ack++;
                printf("ACK received from relay.\n");
                break;
            } else {
                // Timeout occurred, no ACK received
                printf("No ACK received from relay after max tries.\n");
            }
        }
    }
    process_post(&main_process, exp_done, NULL);
    PROCESS_END();
}