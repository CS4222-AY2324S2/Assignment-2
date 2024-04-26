#include <string.h>
#include <stdio.h> 

#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/linkaddr.h"
#include "net/packetbuf.h"
#include "sys/etimer.h"
#include "node-id.h"
#include "lib/random.h"

// Data Structures 
typedef struct {
  unsigned long src_id;
  unsigned long timestamp;
  unsigned long seq;
} discovery_packet_struct; 

typedef struct {
  unsigned long src_id;
  unsigned long timestamp;
  int light_value; 
} light_sensor_reading_struct;

// Constants
#define LISTEN_INTERVAL (CLOCK_SECOND * 5)
#define LISTEN_DURATION  (CLOCK_SECOND) 

// Configures the wake-up timer for neighbour discovery 
#define WAKE_TIME RTIMER_SECOND/10    // 10 HZ, 0.1s
#define SLEEP_CYCLE  9        	      // 0 for never sleep
#define SLEEP_SLOT RTIMER_SECOND/10   // sleep slot should not be too large to prevent overflow
#define NUM_SEND 2

// Global Variables
linkaddr_t node_a_addr; // Node A's address (will be learned dynamically)
linkaddr_t dest_addr;	// For neighbour discovery, we would like to send message to everyone. We use Broadcast address:

// Current time stamp of the node
unsigned long curr_timestamp;

// sender timer implemented using rtimer
static struct rtimer rt;

// Protothread variable
static struct pt pt;

// Packets
static discovery_packet_struct discovery_packet;

// Starts the main contiki neighbour discovery process
PROCESS(nbr_discovery_process, "cc2650 neighbour discovery process");
AUTOSTART_PROCESSES(&nbr_discovery_process);

// Callback Function
void receive_packet_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest) {
  printf("Node B: Received packet of length %u\n", len);
  printf("Size of discovery_packet_struct: %u\n", sizeof(discovery_packet_struct));
  printf("Size of light_sensor_reading_struct: %u\n", sizeof(light_sensor_reading_struct));
  
  if (len == sizeof(discovery_packet_struct)) {
     printf("Node B: Received discovery packet from Node %lu\n", ((discovery_packet_struct *)data)->src_id);  
   } else if (len == sizeof(light_sensor_reading_struct)) {
     printf("Node B: Received light reading from Node %lu\n", ((light_sensor_reading_struct *)data)->src_id);
     light_sensor_reading_struct *reading = (light_sensor_reading_struct *)data;

     // Update node_a_addr if this is the first reading from Node A
     if (linkaddr_cmp(&node_a_addr, &linkaddr_null)) {
        node_a_addr = *src;
     }

     printf("Node B: Received light reading from Node %lu: %d\n", reading->src_id, reading->light_value);
  }  else {
    printf("Node B: Unknown data packet received\n");
  }
}

// Scheduler function for the sender of neighbour discovery packets
char sender_scheduler(struct rtimer *t, void *ptr) {
  static uint16_t i = 0;
  static int NumSleep=0;
 
  // Begin the protothread
  PT_BEGIN(&pt);

  // Get the current time stamp
  curr_timestamp = clock_time();

  printf("Start clock %lu ticks, timestamp %3lu.%03lu\n", curr_timestamp, curr_timestamp / CLOCK_SECOND, 
  ((curr_timestamp % CLOCK_SECOND)*1000) / CLOCK_SECOND);

  while(1) {
    // radio on
    NETSTACK_RADIO.on();

    // send NUM_SEND number of neighbour discovery beacon packets
    for(i = 0; i < NUM_SEND; i++){
       // Initialize the nullnet module with information of packet to be trasnmitted
      nullnet_buf = (uint8_t *)&discovery_packet; // data transmitted
      nullnet_len = sizeof(discovery_packet); // length of data transmitted
      
      discovery_packet.seq++;
      curr_timestamp = clock_time();
      discovery_packet.timestamp = curr_timestamp;

      printf("Send seq# %lu  @ %8lu ticks   %3lu.%03lu\n", discovery_packet.seq, curr_timestamp, curr_timestamp / CLOCK_SECOND, ((curr_timestamp % CLOCK_SECOND)*1000) / CLOCK_SECOND);
      NETSTACK_NETWORK.output(NULL); //Packet transmission
      
      // wait for WAKE_TIME before sending the next packet
      if(i != (NUM_SEND - 1)){
        rtimer_set(t, RTIMER_TIME(t) + WAKE_TIME, 1, (rtimer_callback_t)sender_scheduler, ptr);
        PT_YIELD(&pt);
      }
    }

    // sleep for a random number of slots
    if(SLEEP_CYCLE != 0){
      // radio off
      NETSTACK_RADIO.off();

      // SLEEP_SLOT cannot be too large as value will overflow,
      // to have a large sleep interval, sleep many times instead

      // get a value that is uniformly distributed between 0 and 2*SLEEP_CYCLE
      // the average is SLEEP_CYCLE 
      NumSleep = random_rand() % (2 * SLEEP_CYCLE + 1);  
      printf(" Sleep for %d slots \n",NumSleep);

      // NumSleep should be a constant or static int
      for(i = 0; i < NumSleep; i++){
        rtimer_set(t, RTIMER_TIME(t) + SLEEP_SLOT, 1, (rtimer_callback_t)sender_scheduler, ptr);
        PT_YIELD(&pt);
      }
    }
  }
  
  PT_END(&pt);
}

// Process Thread (Periodic Listening)
PROCESS_THREAD(nbr_discovery_process, ev, data)
{
  // static struct etimer listen_timer;

  PROCESS_BEGIN();
  nullnet_set_input_callback(receive_packet_callback); 

  // initialize data packet sent for neighbour discovery exchange
  discovery_packet.src_id = node_id; //Initialize the node ID
  discovery_packet.seq = 0; //Initialize the sequence number of the packet
  
  nullnet_set_input_callback(receive_packet_callback); //initialize receiver callback
  linkaddr_copy(&dest_addr, &linkaddr_null);

  printf("CC2650 neighbour discovery\n");
  printf("Node %d will be sending packet of size %d Bytes\n", node_id, (int)sizeof(discovery_packet_struct));

  // Start sender in one millisecond.
  rtimer_set(&rt, RTIMER_NOW() + (RTIMER_SECOND / 1000), 1, (rtimer_callback_t)sender_scheduler, NULL);


//   while(1) {
//     etimer_set(&listen_timer, LISTEN_INTERVAL); 
//     PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&listen_timer));  

//     etimer_set(&listen_timer, LISTEN_DURATION);
//     PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&listen_timer)); 

// 	// Initialize discovery packet
// 	discovery_packet_struct discovery_packet;
// 	discovery_packet.src_id = node_id;
// 	discovery_packet.timestamp = clock_time();
// 	discovery_packet.seq++; // Increment sequence number

// 	// Prepare for transmission using nullnet
// 	nullnet_buf = (uint8_t *)&discovery_packet;
// 	nullnet_len = sizeof(discovery_packet_struct);

// 	// Broadcast the packet 
// 	printf("Send seq# %lu  @ %8lu ticks   %3lu.%03lu\n", discovery_packet.seq, discovery_packet.timestamp, discovery_packet.timestamp / CLOCK_SECOND, ((discovery_packet.timestamp % CLOCK_SECOND)*1000) / CLOCK_SECOND);
// 	NETSTACK_RADIO.on(); // Turn radio on for listening
// 	NETSTACK_NETWORK.output(NULL); 
//     NETSTACK_RADIO.off(); // Turn radio off 
//   }

  PROCESS_END();
}
