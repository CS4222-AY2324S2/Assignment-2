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
#define SEND_INTERVAL (8 * CLOCK_SECOND)
static linkaddr_t dest_addr =         {{ 0x00, 0x12, 0x4b, 0x00, 0x11, 0xa7, 0x73, 0x87 }}; //replace this with your receiver's link address


/*---------------------------------------------------------------------------*/
PROCESS(unicast_process, "One to One Communication");
AUTOSTART_PROCESSES(&unicast_process);

int receiveCount = 0;
int rssiTotal = 0;

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
  static unsigned count = 0;
  PROCESS_BEGIN();



  /* Initialize NullNet */
  nullnet_buf = (uint8_t *)&count; //data transmitted
  nullnet_len = sizeof(count); //length of data transmitted
  nullnet_set_input_callback(input_callback); //initialize receiver callback

  if(!linkaddr_cmp(&dest_addr, &linkaddr_node_addr)) { //ensures destination is not same as sender
    etimer_set(&periodic_timer, SEND_INTERVAL);
    while(1) {
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
      printf("Sending %u to ", count);
      // linkaddr_t addr = dest_addr;
      // printf("%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n", 
      //        addr.u8[0], addr.u8[1], addr.u8[2], addr.u8[3], 
      //        addr.u8[4], addr.u8[5], addr.u8[6], addr.u8[7]);
      // printf("\n");

      NETSTACK_NETWORK.output(&dest_addr); //Packet transmission
      count++;
      etimer_reset(&periodic_timer);
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
