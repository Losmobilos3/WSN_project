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
#include "dev/radio/cc2420/cc2420.h"
#include "sys/energest.h"
#include "packetbuf.h"

#define max_tries 7
#define REPEATS 100
#define max_msg_size 104 // Max payload size for MAC-frame in 802.15.4 is 104 bytes

#define ACK_TIMEOUT (CLOCK_SECOND / 4)

typedef enum {
    NO_RELAY,
    N_PACKET_RELAY,
    EWMA_N_PACKET_RELAY
} protocol;

protocol selected_protocol = EWMA_N_PACKET_RELAY; // Change protocol here

uint8_t eps = 10; // small time to account for processing delays

static uint8_t tx_buffer[max_msg_size];
static uint8_t msg_exchange_buffer[sizeof(linkaddr_t) + max_msg_size];

process_event_t acknowledgement;
process_event_t relay_acknowledgement;
process_event_t exp_done;
process_event_t MESSAGE_HANDLED;

// This nodes address is contained in linkaddr_node_addr
// static linkaddr_t node_a_addr = {{0x01, 0x01, 0x01, 0x00, 0x01, 0x74, 0x12, 0x00}};
static linkaddr_t dest_addr = {{0x03, 0x03, 0x03, 0x00, 0x03, 0x74, 0x12, 0x00}};
static linkaddr_t relay_addr = {{0x02, 0x02, 0x02, 0x00, 0x02, 0x74, 0x12, 0x00}};

static int n_ack;
static int n_attempts;
static int n_relay_ack;
static int n_relay_attempts;
static int current_max_retries = max_tries;

static int ewma_estimate = 100; // Initial estimate 100% delivery from A-C
static int alpha = 40;
static bool relay_phase = false;

// ---------------- Processes ----------------
PROCESS(send_message_process, "send_message_process");

PROCESS(main_process, "main process");
PROCESS(no_relay_process, "no_relay_process");
PROCESS(n_packet_relay_process, "n_packet_relay_process");
PROCESS(EWMA_n_packet_relay_process, "n_packet_relay_process");
AUTOSTART_PROCESSES(&main_process);

static unsigned long to_seconds(uint64_t time)
{
  return (unsigned long)(time / ENERGEST_SECOND);
}

void print_energest_data() {
    // Print energest data
    energest_flush();

    printf("\nEnergest:\n");
    printf(" CPU          %4lus LPM      %4lus DEEP LPM %4lus  Total time %lus\n Radio LISTEN %4lus TRANSMIT %4lus OFF      %4lus\n",
        to_seconds(energest_type_time(ENERGEST_TYPE_CPU)),
        to_seconds(energest_type_time(ENERGEST_TYPE_LPM)),
        to_seconds(energest_type_time(ENERGEST_TYPE_DEEP_LPM)),
        to_seconds(ENERGEST_GET_TOTAL_TIME()),
        to_seconds(energest_type_time(ENERGEST_TYPE_LISTEN)),
        to_seconds(energest_type_time(ENERGEST_TYPE_TRANSMIT)),
        to_seconds(ENERGEST_GET_TOTAL_TIME()
                    - energest_type_time(ENERGEST_TYPE_TRANSMIT)
                    - energest_type_time(ENERGEST_TYPE_LISTEN)));
}

static void send_msg_callback(void *ptr, int status, int transmissions) {
    // Check status of message
    if(status == MAC_TX_OK) { // Success
        if (!relay_phase) {
            n_ack++;
        }
        printf("SUCCESS: MAC ACK received after %d tries\n", transmissions);
    } else if(status == MAC_TX_NOACK) { // Failure
        printf("FAILURE: No ACK after %d tries\n", transmissions);
    } else if(status == MAC_TX_COLLISION) { // Collision
        printf("FAILURE: Collision (medium busy)\n");
    }

    if (!relay_phase) {
        n_attempts += transmissions;
    } else {
        n_relay_attempts += transmissions;
    }

    
    process_post(&send_message_process, MESSAGE_HANDLED, (void *) status);
}

// ---------------- Message functions ----------------
PROCESS_THREAD(send_message_process, ev, data)
{
    // These must be static to survive PROCESS_YIELD
    static linkaddr_t destination_addr;
    
    PROCESS_BEGIN();
    
    if(data == NULL) {
        PROCESS_EXIT();
    }

    // Correct Pointer Arithmetic
    uint8_t *data_ptr = (uint8_t *)data;
    
    // Copy address safely into our static variable
    memcpy(&destination_addr, data_ptr, sizeof(linkaddr_t));
    
    // Point to payload (1 byte type + 8 bytes addr)
    uint8_t *msg_payload = &data_ptr[sizeof(linkaddr_t)];

    // Use the static tx_buffer instead of a local stack array
    memcpy(tx_buffer, msg_payload, max_msg_size);
    
    nullnet_buf = tx_buffer;
    nullnet_len = max_msg_size;

    // Load into Packetbuf and Send
    packetbuf_clear();
    packetbuf_copyfrom(nullnet_buf, nullnet_len);
    packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &destination_addr);

    packetbuf_set_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS, current_max_retries);

    NETSTACK_MAC.send(send_msg_callback, NULL);

    // Wait for the callback to trigger MESSAGE_HANDLED
    PROCESS_YIELD_UNTIL(ev == MESSAGE_HANDLED);

    // Tell the relay process that an ACK has been received
    int status = (int) data;
    if (selected_protocol == NO_RELAY) {
        process_post(&no_relay_process, acknowledgement, NULL);
    } else if (selected_protocol == N_PACKET_RELAY) {
        process_post(&n_packet_relay_process, acknowledgement, (void*) status);
    } else if (selected_protocol == EWMA_N_PACKET_RELAY) {
        process_post(&EWMA_n_packet_relay_process, acknowledgement, (void*) status);
    }
    
    PROCESS_END();
}

void send_msg(const linkaddr_t *addr) {
    // Set data to send
    uint8_t payload[max_msg_size] = { 0 }; 
    // Init message
    for (int i = 0; i < max_msg_size; i++) {
        payload[i] = (uint8_t)(i); // Starts from 0
    }

    memcpy(msg_exchange_buffer, addr, sizeof(linkaddr_t));
    memcpy(&msg_exchange_buffer[sizeof(linkaddr_t)], payload, max_msg_size);
    
    // Start send_message_process
    process_start(&send_message_process, (void *) msg_exchange_buffer);
}

void send_relay_msg(const linkaddr_t *relay_addr, const linkaddr_t *final_dest_addr, bool is_ack) {
    // Set data to send
    uint8_t payload[max_msg_size] = { 0 }; 
    // Init message
    for (int i = 0; i < max_msg_size; i++) {
        payload[i] = (uint8_t)(i+1); // Starts from 1
    }
    // Place final_dest_addr in msg payload
    memcpy(&payload[1], final_dest_addr->u8, sizeof(linkaddr_t)); // First bytes are final dest address
    if (is_ack) {
        payload[sizeof(linkaddr_t) + 1] = 0; // Set message type to ack relay
    } else {
        payload[sizeof(linkaddr_t) + 1] = 1; // Set message type to relay msg
    }

    // Create data for send_message_process
    memcpy(msg_exchange_buffer, relay_addr->u8, sizeof(linkaddr_t));
    memcpy(&msg_exchange_buffer[sizeof(linkaddr_t)], payload, max_msg_size);
    
    
    // Start send_message_process
    process_start(&send_message_process, (void *) msg_exchange_buffer);
}

void relay_msg(const uint8_t* data, const linkaddr_t* src_addr) {
    // Extract final destination address
    memcpy(msg_exchange_buffer, &data[1], sizeof(linkaddr_t));
    // Prepare message to send
    msg_exchange_buffer[sizeof(linkaddr_t)] = 2; // Set message type to relayed msg
    memcpy(&msg_exchange_buffer[sizeof(linkaddr_t) + 1], src_addr->u8, sizeof(linkaddr_t)); // Place original src addr after msg type
    memcpy(&msg_exchange_buffer[2*sizeof(linkaddr_t) + 1], &data[sizeof(linkaddr_t) + 1], max_msg_size);

    // Start send_message_process
    process_start(&send_message_process, (void *) msg_exchange_buffer);
}

void send_ack(const linkaddr_t *src_addr, const linkaddr_t *relay_addr) {
    // Set data to send
    send_relay_msg(relay_addr, src_addr, true);
}

void send_done() {
    cc2420_set_txpower(31); // Set to max power

    // Set data to send
    uint8_t payload[1] = { 3 }; // Starts with 3
    nullnet_buf = payload;
    nullnet_len = 1; 

    // Sending logic here
    NETSTACK_NETWORK.output(&relay_addr); 

    // Set data to send
    nullnet_buf = payload;
    nullnet_len = 1; 

    // Sending logic here
    NETSTACK_NETWORK.output(&dest_addr); 
}



// ---------------- Input callback N_RELAY --------------
void input_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest)
{
    // Skip packages not for self
    if (!linkaddr_cmp(dest, &linkaddr_null) && !linkaddr_cmp(dest, &linkaddr_node_addr)) {
        printf("Skipped!");
        return;
    }
    
    // Message type
    uint8_t m_type = ((uint8_t *)data)[0]; // Direct (0), Initial relay (1), final relay (2), Done (3)

    printf("Message type: %d \n", m_type);
    if (m_type == 3) {
        printf("Experiment done signal received.\n");
        print_energest_data();
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
    
    if (m_type > 0) { // Relay message
        printf("Received Relay message \n");

        // Extract contained address
        linkaddr_t contained_addr;
        memcpy(&contained_addr, &((uint8_t *) data)[1], sizeof(linkaddr_t));

        if (m_type == 1) {
            printf("Forwarding message to dest\n");

            // Relay the message to final destination
            relay_msg(data, src);

        } else if (m_type == 2) {

            // Check if ack
            int ack_or_msg = ((uint8_t *)data)[sizeof(linkaddr_t) + 1];
            printf("Ack or msg: %d \n", ack_or_msg);
            if (ack_or_msg == 0) { // Is ack
                n_relay_ack++;
                printf("Received Relay ACK from relay node. \n");
                if (selected_protocol == N_PACKET_RELAY) {
                    process_post(&n_packet_relay_process, relay_acknowledgement, NULL); // Wake up n_packet_relay_process to handle ack
                } else if (selected_protocol == EWMA_N_PACKET_RELAY){
                    process_post(&EWMA_n_packet_relay_process, relay_acknowledgement, NULL); // Wake up EWMA_n_packet_relay_process to handle ack
                }
            } else { // Is message
                printf("Sending ack back to src\n");
                send_ack(&contained_addr, src);
            }
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
    relay_acknowledgement = process_alloc_event();
    exp_done = process_alloc_event();
    MESSAGE_HANDLED = process_alloc_event();

    /* At process initialization */
    nullnet_set_input_callback(input_callback); // Setup the input callback (IS ALREADY A PROTOTHREAD?!?)

    // Change TX_power
    cc2420_set_txpower(3); // -25 dBm

    // Wait for all motes to start up
    etimer_set(&timer, CLOCK_SECOND * 5);

    // Run the experiment REPEATS times.
    printf("Node ID: %u\n", node_id);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
    if (node_id == 1) {
        n_ack = 0;
        n_relay_ack = 0;
        n_attempts = 0;
        n_relay_attempts = 0;
        for (i = 0; i < REPEATS; i++) {
            // Wait for all motes to begin listening
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
            // etimer_set(&timer, CLOCK_SECOND * 5);
        }
        printf("A to C: %d out of %d attempts were successful. \n", n_ack, n_attempts);
        printf("Relay to C: %d out of %d attempts were successful. \n", n_relay_ack, n_relay_attempts);
        print_energest_data();

        send_done(); // Notify end of experiment to all nodes
    }

    PROCESS_END();
}

// ---------------- Relay Protocols ----------------
PROCESS_THREAD(no_relay_process, ev, data)
{
    PROCESS_BEGIN();

    printf("STARTED NO RELAY PROCESS\n");
        
    // Send message
    send_msg(&dest_addr); // Send message to C
    
    // Start etimer and wait for ACK
    PROCESS_WAIT_EVENT_UNTIL(ev == acknowledgement);

    process_post(&main_process, exp_done, NULL);
    PROCESS_END();
}

PROCESS_THREAD(n_packet_relay_process, ev, data)
{
    static struct etimer ack_timer;

    PROCESS_BEGIN();

    printf("STARTED N-RELAY PROCESS\n");

    // Send message
    send_msg(&dest_addr);
    
    // Start etimer and wait for ACK
    PROCESS_WAIT_EVENT_UNTIL(acknowledgement == ev);

    int status = (int) data;

    if (status != MAC_TX_OK) {
        relay_phase = true;

        // After max tries without success, try relay
        send_relay_msg(&relay_addr, &dest_addr, false);
        etimer_set(&ack_timer, 4*ACK_TIMEOUT+eps);
        
        // Check if message was recieved by relay node
        PROCESS_WAIT_EVENT_UNTIL(acknowledgement == ev);

        int status = (int) data;

        if (status != MAC_TX_OK) {
            printf("Relay failed to receive message.\n");
            process_post(&main_process, exp_done, NULL);
        } else {
            // Wait for relay ACK
            PROCESS_WAIT_EVENT_UNTIL(relay_acknowledgement == ev || etimer_expired(&ack_timer));
        }


        relay_phase = false;
    }
    process_post(&main_process, exp_done, NULL);
    PROCESS_END();
}

PROCESS_THREAD(EWMA_n_packet_relay_process, ev, data)
{
    static struct etimer ack_timer;

    PROCESS_BEGIN();

    printf("STARTED EWMA-N-RELAY PROCESS\n");

    if (ewma_estimate < 30) {
        current_max_retries = 1;
    } else {
        current_max_retries = max_tries;
    }
    // Send message
    send_msg(&dest_addr);
    
    // Start etimer and wait for ACK
    PROCESS_WAIT_EVENT_UNTIL(acknowledgement == ev);

    // Update EWMA estimate
    int mu_n = (n_ack * 100) / n_attempts;
    ewma_estimate = ((100-alpha)*mu_n + alpha*ewma_estimate) / 100; // Should be stored in %

    // Get status of direct messages
    int status = (int) data;
    

    if (status != MAC_TX_OK) {
        relay_phase = true;
        current_max_retries = max_tries;


        // After max tries without success, try relay
        send_relay_msg(&relay_addr, &dest_addr, false);
        etimer_set(&ack_timer, 4*ACK_TIMEOUT+eps);
        
        // Check if message was recieved by relay node
        PROCESS_WAIT_EVENT_UNTIL(acknowledgement == ev);

        int status = (int) data;

        if (status != MAC_TX_OK) {
            printf("Relay failed to receive message.\n");
            process_post(&main_process, exp_done, NULL);
        } else {
            // Wait for relay ACK
            PROCESS_WAIT_EVENT_UNTIL(relay_acknowledgement == ev || etimer_expired(&ack_timer));
        }

        relay_phase = false;
    }
    process_post(&main_process, exp_done, NULL);
    PROCESS_END();
}