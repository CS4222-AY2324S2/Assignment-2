/*
* CS4222/5422: Assignment 3b
* Perform neighbour discovery
*/

#include <string.h>
#include <stdio.h> 
#include <stdint.h>

#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/packetbuf.h"
#include "lib/random.h"
#include "net/linkaddr.h"
#include "node-id.h"

#include "sys/rtimer.h"
#include "board-peripherals.h"

// Identification information of the node


// Configures the wake-up timer for neighbour discovery 
#define WAKE_TIME RTIMER_SECOND/10    // 10 HZ, 0.1s

#define SLEEP_CYCLE  9        	      // 0 for never sleep
#define SLEEP_SLOT RTIMER_SECOND/10   // sleep slot should not be too large to prevent overflow
#define MAX_READINGS 60  // Store up to 60 readings (one per second)

// For neighbour discovery, we would like to send message to everyone. We use Broadcast address:
linkaddr_t dest_addr;

#define NUM_SEND 2
/*---------------------------------------------------------------------------*/
typedef struct {
  unsigned long src_id;
  unsigned long timestamp;
  unsigned long seq;
  
} data_packet_struct;

typedef struct
{
  unsigned long src_id;
  unsigned long timestamp;
  unsigned long seq;
  int light_sensor;

} light_sensor_packet_struct;

typedef struct
{
  unsigned long src_id;
  unsigned long timestamp;
  unsigned long seq;

} relay_packet_struct;

typedef struct
{
  unsigned long src_id;
  unsigned long timestamp;

} request_packet_struct;

typedef struct
{
  unsigned long src_id;
  unsigned long timestamp;
  int light_sensor_readings[MAX_READINGS];

} light_sensor_readings_packet_struct;

/*---------------------------------------------------------------------------*/
// duty cycle = WAKE_TIME / (WAKE_TIME + SLEEP_SLOT * SLEEP_CYCLE)
/*---------------------------------------------------------------------------*/

// sender timer implemented using rtimer
static struct rtimer rt;

// Protothread variable
static struct pt pt;

// Structure holding the data to be transmitted or received
static data_packet_struct data_packet;
static light_sensor_packet_struct light_sensor_packet;
static light_sensor_readings_packet_struct light_sensor_readings_packet;

// Current time stamp of the node
unsigned long curr_timestamp;

static int counter_rtimer;
static struct rtimer timer_rtimer;
static rtimer_clock_t timeout_rtimer = RTIMER_SECOND; 

int light_sensor_readings[MAX_READINGS];
int current_index = 0;
int seconds_elapsed = 0;

/*---------------------------------------------------------------------------*/
static void init_opt_reading(void);
static void get_light_reading(void);

// Starts the main contiki neighbour discovery process
PROCESS(nbr_discovery_process, "cc2650 neighbour discovery process");
PROCESS(light_sensor_process, "light sensor process");
AUTOSTART_PROCESSES(&nbr_discovery_process, &light_sensor_process);

// Function called after reception of a packet
void receive_packet_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest) 
{
  // Check if the received packet size matches with what we expect it to be
  if (len == sizeof(data_packet)) {
    static data_packet_struct received_packet_data;
    
    // Copy the content of packet into the data structure
    memcpy(&received_packet_data, data, len);
    
    // Print the details of the received packet
    printf("Received neighbour discovery packet %lu with rssi %d from %ld", received_packet_data.seq, (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI),received_packet_data.src_id);
    printf("\n");

    // Check if link quality is good based on RSSI
    if(packetbuf_attr(PACKETBUF_ATTR_RSSI) < -50){
      printf("NODE A: Link quality is bad\n");
      return;
    }

    printf("NODE A: Link quality is good\n");
    unsigned long curr_time = clock_time();

    // Initialize the nullnet module with information of packet to be trasnmitted
    nullnet_buf = (uint8_t *)&light_sensor_readings_packet; // data transmitted
    nullnet_len = sizeof(light_sensor_readings_packet);     // length of data transmitted

    light_sensor_readings_packet.src_id = received_packet_data.src_id;
    light_sensor_readings_packet.timestamp = clock_time();
    for (int j = 0; j < MAX_READINGS; j++)
    {
      light_sensor_readings_packet.light_sensor_readings[j] = light_sensor_readings[j];
    }

    printf("Sending Light Sensor Data\n");
    printf("%lu NODE A: TRANSFER %lu\n", curr_time / CLOCK_SECOND, received_packet_data.src_id);

    NETSTACK_NETWORK.output(src); // Only send to node that requested
    
    // Reset light readings array after sending to Node B
    current_index = 0;
    memset(light_sensor_readings, 0, sizeof(light_sensor_readings));
  }
  else
  {
    printf("NODE A: Unknown data packet received\n");
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

  // Replace %d with %lu to print unsigned long
  printf("Start clock %lu ticks, timestamp %3lu.%03lu\n", curr_timestamp, curr_timestamp / CLOCK_SECOND, 
  ((curr_timestamp % CLOCK_SECOND) * 1000) / CLOCK_SECOND);

  while (1){
    // radio on
    NETSTACK_RADIO.on();

    // send NUM_SEND number of neighbour discovery beacon packets
    for(i = 0; i < NUM_SEND; i++){
       // Initialize the nullnet module with information of packet to be trasnmitted
      nullnet_buf = (uint8_t *)&light_sensor_packet; //data transmitted
      nullnet_len = sizeof(light_sensor_packet); //length of data transmitted
      
      light_sensor_packet.seq++;
      light_sensor_packet.timestamp = curr_timestamp;
      curr_timestamp = clock_time();

      printf("Send seq# %lu  @ %8lu ticks   %3lu.%03lu\n", light_sensor_packet.seq, curr_timestamp, curr_timestamp / CLOCK_SECOND, ((curr_timestamp % CLOCK_SECOND)*1000) / CLOCK_SECOND);

      NETSTACK_NETWORK.output(&dest_addr); //Packet transmission
      
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


/**
 * LIGHT SENSOR METHODS
*/
void
do_rtimer_timeout(struct rtimer *timer, void *ptr)
{

  rtimer_clock_t now = RTIMER_NOW();

  rtimer_set(&timer_rtimer, RTIMER_NOW() + timeout_rtimer, 0, do_rtimer_timeout, NULL);

  int s, ms1,ms2,ms3;
  s = now /RTIMER_SECOND;
  ms1 = (now% RTIMER_SECOND)*10/RTIMER_SECOND;
  ms2 = ((now% RTIMER_SECOND)*100/RTIMER_SECOND)%10;
  ms3 = ((now% RTIMER_SECOND)*1000/RTIMER_SECOND)%10;
  
  counter_rtimer++;
  printf(": %d (cnt) %lu (ticks) %d.%d%d%d (sec) \n",counter_rtimer, now, s, ms1,ms2,ms3); 
  get_light_reading();
}

static void
init_opt_reading(void)
{
  SENSORS_ACTIVATE(opt_3001_sensor);
}

static void
get_light_reading()
{
  int value;

  value = opt_3001_sensor.value(0);
  if(value != CC26XX_SENSOR_READING_ERROR) {
    printf("OPT: Light=%d.%02d lux\n", value / 100, value % 100);

    if (current_index < MAX_READINGS) {
        light_sensor_readings[current_index] = value;
        current_index++;
    }
  } else {
    printf("OPT: Light Sensor's Warming Up\n\n");
  }
  init_opt_reading();
}

// Main thread that handles the neighbour discovery process
PROCESS_THREAD(nbr_discovery_process, ev, data)
{
 // static struct etimer periodic_timer;
 // static struct etimer periodic_timer;

  // static struct etimer periodic_timer;

  PROCESS_BEGIN();

    // initialize data packet sent for neighbour discovery exchange
  light_sensor_packet.src_id = node_id; //Initialize the node ID
  light_sensor_packet.seq = 0; //Initialize the sequence number of the packet
  // light_sensor_packet.light_sensor = 1; // Inform receivers Light Sensor Node discovered

  light_sensor_readings_packet.src_id = node_id;
  
  nullnet_set_input_callback(receive_packet_callback); //initialize receiver callback
  linkaddr_copy(&dest_addr, &linkaddr_null);

  printf("CC2650 neighbour discovery\n");
  printf("Node %d will be sending packet of size %d Bytes\n", node_id, (int)sizeof(data_packet_struct));

  // Start sender in one millisecond.
  rtimer_set(&rt, RTIMER_NOW() + (RTIMER_SECOND / 1000), 1, (rtimer_callback_t)sender_scheduler, NULL);

  PROCESS_END();
}


PROCESS_THREAD(light_sensor_process, ev, data)
{
  PROCESS_BEGIN();
  init_opt_reading();

  printf(" The value of RTIMER_SECOND is %d \n",RTIMER_SECOND);
  printf(" The value of timeout_rtimer is %lu \n",timeout_rtimer);

  while(seconds_elapsed < MAX_READINGS) {  // Run for 60 seconds
    rtimer_set(&timer_rtimer, RTIMER_NOW() + timeout_rtimer, 0,  do_rtimer_timeout, NULL);
    PROCESS_YIELD();
    seconds_elapsed++; 
  }

  printf("Process Terminated\n"); 
  PROCESS_END();
}