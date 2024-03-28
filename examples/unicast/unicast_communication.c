#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/packetbuf.h"

#include <string.h>
#include <stdio.h> 

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

/* Configuration */
// #define SEND_INTERVAL (8 * CLOCK_SECOND)
#define SEND_INTERVAL (CLOCK_SECOND / 4) // 1/4 of a second
#define MAX_COUNT 240 // 60 seconds / 0.25 seconds
static linkaddr_t dest_addr = {{ 0x00, 0x12, 0x4b, 0x00, 0x0f, 0x0c, 0xe4, 0x06 }}; //replace this with your receiver's link address
// static linkaddr_t dest_addr = {{ 0x00, 0x12, 0x4b, 0x00, 0x12, 0x05, 0x04, 0x48 }}; //replace this with your receiver's link address


/*---------------------------------------------------------------------------*/
PROCESS(unicast_process, "One to One Communication");
AUTOSTART_PROCESSES(&unicast_process);

#include <stdio.h>
#include "net/linkaddr.h"

void print_lladdr(const linkaddr_t *addr) {
    if(addr == NULL) {
        printf("NULL");
    } else {
        for(unsigned i = 0; i < LINKADDR_SIZE; i++) {
            if(i > 0) {
                printf(":");
            }
            printf("%02x", addr->u8[i]);
        }
    }
    printf("\n");
}

/*---------Callback executed immediately after reception---------*/
void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest) 
{
  receiveCount++;
  if(len == sizeof(unsigned)) {
    unsigned count;
    memcpy(&count, data, sizeof(count));
    printf("Received %u with rssi %d from", count, (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI));
    printf(" Current count: %d\n", receiveCount);
    
    rssiTotal += (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI);
    int avgRssi = rssiTotal / receiveCount;
    printf("Average RSSI: %d\n", avgRssi);
    // linkaddr_t addr = *src;
    // printf("%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n", 
    //        addr.u8[0], addr.u8[1], addr.u8[2], addr.u8[3], 
    //        addr.u8[4], addr.u8[5], addr.u8[6], addr.u8[7]);
    printf("                       ");
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(unicast_process, ev, data)
{
  static struct etimer periodic_timer;
  static unsigned count = 1;
  static unsigned send_count = 0; // counter for sent packets

  PROCESS_BEGIN();

  /* Initialize NullNet */
  nullnet_buf = (uint8_t *)&count; //data transmitted
  nullnet_len = sizeof(count); //length of data transmitted
  nullnet_set_input_callback(input_callback); //initialize receiver callback

  if(!linkaddr_cmp(&dest_addr, &linkaddr_node_addr)) { //ensures destination is not same as sender
    printf("UNICAST starting\n");
    etimer_set(&periodic_timer, SEND_INTERVAL);
    // while(1) {
    while(send_count < MAX_COUNT) {
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
      printf("Sending %u to ", count);
      print_lladdr(&dest_addr);
      printf("\n");
      NETSTACK_NETWORK.output(&dest_addr); //Packet transmission
      count++;
      send_count++;
      etimer_reset(&periodic_timer);
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
