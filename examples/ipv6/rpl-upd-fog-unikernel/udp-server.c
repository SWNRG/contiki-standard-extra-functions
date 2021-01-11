#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/ip/uip.h"
#include "net/rpl/rpl.h"
#include "net/rpl/rpl-conf.h"  // coral

#include "net/netstack.h"
#include "dev/button-sensor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
 
#include "node-id.h"   // coral

//#define DEBUG DEBUG_PRINT
#define DEBUG 0 //George QUESTION: THE HELLO MESSAGES ARE COUNTED???

#include "net/ip/uip-debug.h"

#include "net/rime/timesynch.h"

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#define UDP_EXAMPLE_ID  190

rpl_dag_t *dag;

static struct uip_udp_conn *server_conn;

/*---------------------------------------------------------------------------*/
PROCESS(udp_server_process, "UDP server process");
PROCESS(print_metrics_process, "Printing Server metrics process");
AUTOSTART_PROCESSES(&udp_server_process,&print_metrics_process);
/*---------------------------------------------------------------------------*/

//George: NOT GOOD. We have to use the dag->instance->...
//extern uint8_t rpl_dio_interval_min;
//extern uint8_t rpl_dio_interval_doublings;

/*---------------------------------------------------------------------------*/

static void
tcpip_handler(void)
{
  char *appdata;

  if(uip_newdata()) {
    appdata = (char *)uip_appdata;
    appdata[uip_datalen()] = 0;
    PRINTF("DATA recv '%s' from ", appdata);
    PRINTF("%d",
           UIP_IP_BUF->srcipaddr.u8[sizeof(UIP_IP_BUF->srcipaddr.u8) - 1]);
    PRINTF("\n");
#if SERVER_REPLY
    PRINTF("DATA sending reply\n");
    uip_ipaddr_copy(&server_conn->ripaddr, &UIP_IP_BUF->srcipaddr);
    uip_udp_packet_send(server_conn, "Reply", sizeof("Reply"));
    uip_create_unspecified(&server_conn->ripaddr);
#endif
  }
}
/*---------------------------------------------------------------------------*/

static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  PRINTF("Server IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(state == ADDR_TENTATIVE || state == ADDR_PREFERRED) {
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      PRINTF("\n");
      /* hack to make address "final" */
      if (state == ADDR_TENTATIVE) {
	uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
  uip_ipaddr_t ipaddr;
  struct uip_ds6_addr *root_if;

  PROCESS_BEGIN();

  PROCESS_PAUSE();

  SENSORS_ACTIVATE(button_sensor);

  PRINTF("UDP server started. nbr:%d routes:%d\n",
         NBR_TABLE_CONF_MAX_NEIGHBORS, UIP_CONF_MAX_ROUTES);

#if UIP_CONF_ROUTER
/* The choice of server address determines its 6LoWPAN header compression.
 * Obviously the choice made here must also be selected in udp-client.c.
 *
 * For correct Wireshark decoding using a sniffer, add the /64 prefix to the
 * 6LowPAN protocol preferences,
 * e.g. set Context 0 to fd00::. At present Wireshark copies Context/128 and
 * then overwrites it.
 * (Setting Context 0 to fd00::1111:2222:3333:4444 will report a 16 bit
 * compressed address of fd00::1111:22ff:fe33:xxxx)
 * Note Wireshark's IPCMV6 checksum verification depends on the correct
 * uncompressed addresses.
 */
 
#if 0
/* Mode 1 - 64 bits inline */
   uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 1);
#elif 1
/* Mode 2 - 16 bits inline */
  uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0x00ff, 0xfe00, 1);
#else
/* Mode 3 - derived from link local (MAC) address */
  uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
#endif

  uip_ds6_addr_add(&ipaddr, 0, ADDR_MANUAL);
  root_if = uip_ds6_addr_lookup(&ipaddr);
  if(root_if != NULL) {
    rpl_dag_t *dag;
    dag = rpl_set_root(RPL_DEFAULT_INSTANCE,(uip_ip6addr_t *)&ipaddr);
    uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
    rpl_set_prefix(dag, &ipaddr, 64);
    PRINTF("created a new RPL dag\n");
  } else {
    PRINTF("failed to create a new RPL DAG\n");
  }
#endif /* UIP_CONF_ROUTER */
  
  print_local_addresses();

  /* The data sink runs with a 100% duty cycle in order to ensure high 
     packet reception rates. */
  NETSTACK_MAC.off(1);

  server_conn = udp_new(NULL, UIP_HTONS(UDP_CLIENT_PORT), NULL);
  if(server_conn == NULL) {
    PRINTF("No UDP connection available, exiting the process!\n");
    PROCESS_EXIT();
  }
  udp_bind(server_conn, UIP_HTONS(UDP_SERVER_PORT));

  PRINTF("Created a server connection with remote address ");
  PRINT6ADDR(&server_conn->ripaddr);
  PRINTF(" local/remote port %u/%u\n", UIP_HTONS(server_conn->lport),
         UIP_HTONS(server_conn->rport));

  while(1) {
    PROCESS_YIELD();
    if(ev == tcpip_event) {
      tcpip_handler();
    } else if (ev == sensors_event && data == &button_sensor) {
      PRINTF("Initiaing global repair\n");
      rpl_repair_root(RPL_DEFAULT_INSTANCE);
    }
  }

  PROCESS_END();
}

PROCESS_THREAD(print_metrics_process, ev, data){
  static struct etimer periodic_timer;
  
  //setting Imin manually
  //rpl_dio_interval_min=8;
  
  //setting Idouble manually
  //rpl_dio_interval_doublings=12;

  // GET CURRENT DAG
  dag = rpl_get_any_dag();
  
  //variable to count the printing rounds
  static int counter=0;
	
  PROCESS_BEGIN();
  PRINTF("Printing Metrics...\n");

  // 60*CLOCK_SECOND should print for RM090 every one (1)  min
  etimer_set(&periodic_timer, 60*CLOCK_SECOND);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    etimer_reset(&periodic_timer);
    
//Changing Imin when counter ==? ----------------------------	
	if(counter == 1){
       printf("DAG-CHANGING IMIN==============================\n");
       dag->instance->dio_intmin = 8;
       dag->instance->dio_intdoubl = 1;
    }
    
    
    counter++; //new round of stats
    
    //printf("R:%d, SINK, Imin:%d, Idoubling:%d\n",counter, rpl_dio_interval_min, rpl_dio_interval_doublings);
   
    printf("R:%d, Imin:%d, Idoubling:%d\n", counter, dag->instance->dio_intmin, dag->instance->dio_intdoubl);
    
    //sink does not sent udp packets
	 //printf("R:%d, SINK, udp_sent:%d\n",counter, uip_stat.udp.sent);
    
    printf("R:%d, SINK, udp_recv:%d\n",counter, uip_stat.udp.recv);	   
    //printf("R:%d, SINK, icmp_sent:%d\n",counter, uip_stat.icmp.sent);
    //printf("R:%d, SINK, icmp_recv:%d\n",counter, uip_stat.icmp.recv);
	 
	 //Should be set to 1 if node is mobile
    //printf("R:%d, SINK, leaf_only:%d\n",counter,RPL_LEAF_ONLY); 
  }

  PROCESS_END();
}


/*---------------------------------------------------------------------------*/
