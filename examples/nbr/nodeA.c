#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/linkaddr.h"
#include "net/packetbuf.h"
#include "node-id.h"
#include "sys/etimer.h"
#include "board-peripherals.h"

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

#define MAX_READINGS 60

// Configures the wake-up timer for neighbour discovery 
#define WAKE_TIME RTIMER_SECOND/10    // 10 HZ, 0.1s
#define SLEEP_CYCLE  9        	      // 0 for never sleep
#define SLEEP_SLOT RTIMER_SECOND/10   // sleep slot should not be too large to prevent overflow
#define NUM_SEND 2

// Constants
#define DISCOVERY_INTERVAL (CLOCK_SECOND * 10)
#define LINK_QUALITY_THRESHOLD -80 

// Global Variables
linkaddr_t node_b_addr; // Node B's link address 
// For neighbour discovery, we would like to send message to everyone. We use Broadcast address:
linkaddr_t dest_addr;

int light_sensor_readings[MAX_READINGS];
int current_index = 0; 
bool receive_packet_callback_flag = false;
static rtimer_clock_t timeout_rtimer = RTIMER_SECOND; 

static light_sensor_reading_struct readings_packet;

PROCESS(nbr_discovery_process, "cc2650 neighbour discovery process");
PROCESS(light_sensing_process, "light sensor process");
AUTOSTART_PROCESSES(&nbr_discovery_process, &light_sensing_process);

// Sensor Functions 
void init_opt_reading(void) {
  SENSORS_ACTIVATE(opt_3001_sensor);
}

void get_light_reading(void) {
  int value = opt_3001_sensor.value(0);
  if (value != CC26XX_SENSOR_READING_ERROR) {
    printf("OPT: Light=%d.%02d lux\n", value / 100, value % 100);
    if (current_index < MAX_READINGS) {
        light_sensor_readings[current_index] = value;
        current_index++;
    }
  } else {
    printf("OPT: Light Sensor's Warming Up\n\n");
  }
}

// Callback Function
void receive_packet_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest) {
  if (len == sizeof(discovery_packet_struct)) {
    discovery_packet_struct *discovery_packet = (discovery_packet_struct *)data;
    int rssi = (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI);

    if (rssi >= LINK_QUALITY_THRESHOLD) {
      	printf("Node A: Link quality is good from Node %lu, sending light data\n", discovery_packet->src_id);
      	receive_packet_callback_flag = true;
	  	node_b_addr = *src;

	  	// Transmit Data if Requested
		printf("Node A: Sending light data to Node B\n");
		// Prepare the light readings packet (Send readings one at a time)
		readings_packet.src_id = node_id;
		readings_packet.timestamp = clock_time();
		
		for (int i = 0; i < current_index; i++)
		{
			printf("Node A: Sending light reading %d to Node B\n", i);
			readings_packet.light_value = light_sensor_readings[i]; // Populate with a single reading

			nullnet_buf = (uint8_t *)&readings_packet;
			nullnet_len = sizeof(readings_packet); 

			NETSTACK_RADIO.on(); 
			NETSTACK_NETWORK.output(&dest_addr); // Transmit to Node B
			NETSTACK_RADIO.off(); 
		}

		receive_packet_callback_flag = false;
		current_index = 0; // Reset index 
    } else {
	  printf("Node A: Link quality is bad from Node %lu\n", discovery_packet->src_id);
	}
  } 
}

// Process for Neighbor Discovery and Light Sensing
PROCESS_THREAD(nbr_discovery_process, ev, data)
{
  static struct etimer discovery_timer;
  // static struct etimer light_sense_timer;
  
  PROCESS_BEGIN();

  nullnet_set_input_callback(receive_packet_callback); 
  linkaddr_copy(&dest_addr, &linkaddr_null);

  while(1) {
    // Discovery
    etimer_set(&discovery_timer, DISCOVERY_INTERVAL);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&discovery_timer)); 

    // Transmit Data if Requested
    if (receive_packet_callback_flag) {
		printf("Node A: Sending light data to Node B\n");
		// Prepare the light readings packet (Send readings one at a time)
		readings_packet.src_id = node_id;
		readings_packet.timestamp = clock_time();
		
		for (int i = 0; i < current_index; i++)
		{
			printf("Node A: Sending light reading %d to Node B\n", i);
			readings_packet.light_value = light_sensor_readings[i]; // Populate with a single reading

			nullnet_buf = (uint8_t *)&readings_packet;
			nullnet_len = sizeof(readings_packet); 

			NETSTACK_RADIO.on(); 
			NETSTACK_NETWORK.output(&dest_addr); // Transmit to Node B
			NETSTACK_RADIO.off(); 
		}

		receive_packet_callback_flag = false;
		current_index = 0; // Reset index 
	}
	else 
	{
		// Initialize discovery packet
		discovery_packet_struct discovery_packet;
		discovery_packet.src_id = node_id;
		discovery_packet.timestamp = clock_time();
		discovery_packet.seq++; // Increment sequence number

		// Prepare for transmission using nullnet
		nullnet_buf = (uint8_t *)&discovery_packet;
		nullnet_len = sizeof(discovery_packet_struct);

		printf("Send seq# %lu  @ %8lu ticks   %3lu.%03lu\n", discovery_packet.seq, discovery_packet.timestamp, discovery_packet.timestamp / CLOCK_SECOND, ((discovery_packet.timestamp % CLOCK_SECOND)*1000) / CLOCK_SECOND);

		// Broadcast the packet 
		// Turn radio on before sending discovery packets
		NETSTACK_RADIO.on(); 
		NETSTACK_NETWORK.output(&dest_addr); 
		NETSTACK_RADIO.off(); // Turn radio off after sending discovery packets
	}
  }

  PROCESS_END();
}

PROCESS_THREAD(light_sensing_process, ev, data)
{
  static struct etimer light_sense_timer;
  
  PROCESS_BEGIN();
  init_opt_reading();

  printf(" The value of RTIMER_SECOND is %d \n",RTIMER_SECOND);
  printf(" The value of timeout_rtimer is %lu \n",timeout_rtimer);

  // while (seconds_elapsed < MAX_READINGS)
  while (1)
  {
    // Sample every 5 seconds
    etimer_set(&light_sense_timer, CLOCK_SECOND * 5); 
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);
    get_light_reading();
  }

  printf("Process Terminated\n"); 
  PROCESS_END();
}
