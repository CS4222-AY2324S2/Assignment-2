/*
 * CS4222/5422: Assignment 3b
 * Perform neighbour discovery
 */

#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/packetbuf.h"
#include "lib/random.h"
#include "net/linkaddr.h"
#include <string.h>
#include <stdio.h>
#include "node-id.h"

// Identification information of the node

#define TIME_SLOT RTIMER_SECOND / 2 // divide time into equal slots, 0.5

// first prime number
#define PRIME_ONE 3

// second prime number
#define PRIME_TWO 5

// For neighbour discovery, we would like to send message to everyone. We use Broadcast address:
linkaddr_t dest_addr;

#define NUM_SEND 2
/*---------------------------------------------------------------------------*/
typedef struct
{
  unsigned long src_id;
  unsigned long timestamp;
  unsigned long seq;
} data_packet_struct;

typedef struct {
  unsigned long id;
  unsigned long timestamp;
  unsigned long seq;
  int lightReadings[10];
} light_packet_struct;

// sender timer implemented using rtimer
static struct rtimer rt;

// Protothread variable
static struct pt pt;

// Structure holding the data to be transmitted
static data_packet_struct data_packet;
static light_packet_struct light_packet;

// Current time stamp of the node
unsigned long curr_timestamp;

// Received time stamp from other node
unsigned long recv_timestamp;

// sleep cycle
int sleep_cycle = PRIME_TWO;

// Starts the main contiki neighbour discovery process
PROCESS(nbr_discovery_process, "cc2650 neighbour discovery process");
AUTOSTART_PROCESSES(&nbr_discovery_process);

// Function called after reception of a packet
void receive_packet_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest)
{
  // Check if the received packet size matches with what we expect it to be
  if (len == sizeof(data_packet))
  {
    static data_packet_struct received_packet_data;

    // Copy the content of packet into the data structure
    memcpy(&received_packet_data, data, len);

    // Print the details of the received packet
    unsigned long id = received_packet_data.src_id;
    curr_timestamp = clock_time();
    recv_timestamp = received_packet_data.timestamp;
    printf("Received timestamp: %3lu.%03lu | Own timestamp: %3lu.%03lu\n",
           recv_timestamp / CLOCK_SECOND,
           ((recv_timestamp % CLOCK_SECOND) * 1000) / CLOCK_SECOND,
           curr_timestamp / CLOCK_SECOND,
           ((curr_timestamp % CLOCK_SECOND) * 1000) / CLOCK_SECOND);

    printf("Timestamp: %3lu.%03lu, DETECT %lu\n", 
      recv_timestamp / CLOCK_SECOND, 
      ((recv_timestamp % CLOCK_SECOND) * 1000) / CLOCK_SECOND, 
      id
    );

    printf("Received neighbour discovery packet %lu with rssi %d from %ld", received_packet_data.seq, (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI), received_packet_data.src_id);
    printf("---------------- ");
  }
  else if (len == sizeof(light_packet)) {
    static light_packet_struct received_packet_data;

    // Copy the content of packet into the data structure
    memcpy(&received_packet_data, data, len);

    printf("Light: ");
    for (int j = 0; j < 10; j++)
    {
      printf("%d.%02d lux", received_packet_data.lightReadings[j] / 100, received_packet_data.lightReadings[j] % 100);
      if (j != 10 - 1)
      {
        printf(", ");
      }
    }
    printf("\n");
  }
}

// Scheduler function for the sender of neighbour discovery packets
char sender_scheduler(struct rtimer *t, void *ptr)
{

  static uint16_t i = 0;

  static int NumSleep = 0;

  // Begin the protothread
  PT_BEGIN(&pt);

  // Get the current time stamp
  curr_timestamp = clock_time();

  // printf("Start clock %lu ticks, timestamp %3lu.%03lu\n", curr_timestamp, curr_timestamp / CLOCK_SECOND,
  //        ((curr_timestamp % CLOCK_SECOND) * 1000) / CLOCK_SECOND);

  while (1)
  {

    // radio on
    NETSTACK_RADIO.on();

    // send NUM_SEND number of neighbour discovery beacon packets
    for (i = 0; i < NUM_SEND; i++)
    {

      // Initialize the nullnet module with information of packet to be trasnmitted
      nullnet_buf = (uint8_t *)&data_packet; // data transmitted
      nullnet_len = sizeof(data_packet);     // length of data transmitted

      data_packet.seq++;

      curr_timestamp = clock_time();

      data_packet.timestamp = curr_timestamp;

      // printf("Send seq# %lu  @ %8lu ticks   %3lu.%03lu\n", data_packet.seq, curr_timestamp, curr_timestamp / CLOCK_SECOND, ((curr_timestamp % CLOCK_SECOND) * 1000) / CLOCK_SECOND);

      NETSTACK_NETWORK.output(&dest_addr); // Packet transmission

      if (i != (NUM_SEND - 1))
      {

        rtimer_set(t, RTIMER_TIME(t) + TIME_SLOT, 1, (rtimer_callback_t)sender_scheduler, ptr);
        PT_YIELD(&pt);
      }
    }

    // radio off
    NETSTACK_RADIO.off();

    // DISCO version 1 with prime numbers 2 and 3
    NumSleep = sleep_cycle;
    if (sleep_cycle == PRIME_ONE)
    {
      sleep_cycle = PRIME_TWO;
    }
    else
    {
      sleep_cycle = PRIME_ONE;
    }
    // printf(" Sleep for %d slots \n", NumSleep);

    // NumSleep should be a constant or static int
    for (i = 0; i < NumSleep; i++)
    {
      rtimer_set(t, RTIMER_TIME(t) + TIME_SLOT, 1, (rtimer_callback_t)sender_scheduler, ptr);
      PT_YIELD(&pt);
    }
  }

  PT_END(&pt);
}

// Main thread that handles the neighbour discovery process
PROCESS_THREAD(nbr_discovery_process, ev, data)
{

  // static struct etimer periodic_timer;

  PROCESS_BEGIN();

  // initialize data packet sent for neighbour discovery exchange
  data_packet.src_id = node_id; // Initialize the node ID
  data_packet.seq = 0;          // Initialize the sequence number of the packet

  nullnet_set_input_callback(receive_packet_callback); // initialize receiver callback
  linkaddr_copy(&dest_addr, &linkaddr_null);

  printf("CC2650 neighbour discovery\n");
  printf("Node %d will be sending packet of size %d Bytes\n", node_id, (int)sizeof(data_packet_struct));

  // Start sender in one millisecond.
  rtimer_set(&rt, RTIMER_NOW() + (RTIMER_SECOND / 1000), 1, (rtimer_callback_t)sender_scheduler, NULL);

  PROCESS_END();
}
