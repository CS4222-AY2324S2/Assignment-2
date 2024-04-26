#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/linkaddr.h"
#include "net/packetbuf.h"
#include "sys/etimer.h"
#include "node-id.h"

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
  int pad;
} light_sensor_reading_struct;

// Constants
#define LISTEN_INTERVAL (CLOCK_SECOND * 5)
#define LISTEN_DURATION  (CLOCK_SECOND) 

// Global Variables
linkaddr_t node_a_addr; // Node A's address (will be learned dynamically)

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

// Process Thread (Periodic Listening)
PROCESS_THREAD(nbr_discovery_process, ev, data)
{
  static struct etimer listen_timer;

  PROCESS_BEGIN();
  nullnet_set_input_callback(receive_packet_callback); 

  while(1) {
    etimer_set(&listen_timer, LISTEN_INTERVAL); 
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&listen_timer));  

    etimer_set(&listen_timer, LISTEN_DURATION);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&listen_timer)); 

	// Initialize discovery packet
	discovery_packet_struct discovery_packet;
	discovery_packet.src_id = node_id;
	discovery_packet.timestamp = clock_time();
	discovery_packet.seq++; // Increment sequence number

	// Prepare for transmission using nullnet
	nullnet_buf = (uint8_t *)&discovery_packet;
	nullnet_len = sizeof(discovery_packet_struct);

	// Broadcast the packet 
	printf("Send seq# %lu  @ %8lu ticks   %3lu.%03lu\n", discovery_packet.seq, discovery_packet.timestamp, discovery_packet.timestamp / CLOCK_SECOND, ((discovery_packet.timestamp % CLOCK_SECOND)*1000) / CLOCK_SECOND);
	NETSTACK_RADIO.on(); // Turn radio on for listening
	NETSTACK_NETWORK.output(NULL); 
    NETSTACK_RADIO.off(); // Turn radio off 
  }

  PROCESS_END();
}
