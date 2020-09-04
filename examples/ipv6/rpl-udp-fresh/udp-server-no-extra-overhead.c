#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/ip/uip.h"
#include "net/rpl/rpl.h"

#include "node-id.h" 

#include "net/netstack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef PERIOD
#define PERIOD 600 /* Used only for a periodic ping to controller */
#endif
#define START_INTERVAL		(15 * CLOCK_SECOND)
#define SEND_INTERVAL		(PERIOD * CLOCK_SECOND)
#define SEND_TIME		(random_rand() % (SEND_INTERVAL))

#define MAX_PAYLOAD_LEN		30

//#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

/**** Read from serial port  ******/
#include "dev/serial-line.h"
// is this causing problems if it is bigger ??????????
#define UART_BUFFER_SIZE      45
static uint8_t uart_buffer[UART_BUFFER_SIZE];
static uint8_t uart_buffer_index = 0;
/*********************************/

static struct uip_udp_conn *server_conn;
static struct uip_udp_conn *client_conn;

static uip_ipaddr_t ServerIpAddress;

int counter = 0; // just a rounds counter


PROCESS(udp_server_process, "UDP server process");
//PROCESS(read_serial, "Read serial process");
AUTOSTART_PROCESSES(&udp_server_process);
/*---------------------------------------------------------------------------*/
static void 
tcpip_handler(void) /* CLIENTS' SIDE TRIGGERED */
{
  char *appdata;
  char *server_msg ="AC";
  uip_ipaddr_t *child_node;
  
  if(uip_newdata()) {
    appdata = (char *)uip_appdata;
    appdata[uip_datalen()] = 0;
#define PRINT_DETAILS 0
#if PRINT_DETAILS     /* DON'T FORGET, DATA COMING FROM CLIENT NODES */   
    printf("DATA: '%s' from ", appdata);
    printf("%d",UIP_IP_BUF->srcipaddr.u8[sizeof(UIP_IP_BUF->srcipaddr.u8) - 1]);
    printf("\n");
#endif 

    	 child_node = &UIP_IP_BUF->srcipaddr;
		 /* Need to convert again the sourceIP from global to local
		 * i.e., from fd00 --. fe80. We dont want to play 
		 * with the protocol, hence, return a local variable.
		 */ 				
		 child_node->u8[0] = (uint8_t *)254;
		 child_node->u8[1] = (uint8_t *)128;

/* This is not really needed. Just print all incoming messages */
    if( (appdata[0] == 'N' && appdata[1] == 'P') || /* node is sending a New Parent */
    	  (appdata[0] == 'S' && appdata[1] == 'U') ||/* node is sending UDP data (SU) */
    	  (appdata[0] == 'S' && appdata[1] == 'I') || /* node is sending ICMP data (SI) */
    	  (appdata[0] == 'N' && appdata[1] == '1') || /* node is sending it's neighbors */
    	  (appdata[0] == 'V' && appdata[1] == 'A') ) /* node is under version num attack */
    { 	
    	 /* controller reads UART line starting with 2 chars (NP, etc.) */
		 printf("%s from ", appdata);
		 printLongAddr(child_node);	
		 printf("\n");   
    } 
    else{    
    	/* printing an incoming message, e.g. various enviromental measurements */
		 printf("%s from ", appdata);
		 printLongAddr(child_node);
		 printf("\n");
#if SERVER_REPLY
    	 printf("Server Replying... \n");
    	 send_custom_msg(&UIP_IP_BUF->srcipaddr, server_msg);    
#endif
	}
  }
}
/*---------------------------------------------------------------------------*/
static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  printf("Server IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(state == ADDR_TENTATIVE || state == ADDR_PREFERRED) {
      printLongAddr(&uip_ds6_if.addr_list[i].ipaddr);    
      printf("\n");    
      
      /* will keep the last one fe80 */
      ServerIpAddress = uip_ds6_if.addr_list[i].ipaddr;
      
      /* hack to make address "final" */
      if (state == ADDR_TENTATIVE) {
			uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
ping_only(void){ /* periodically ping the controller */
	printf("Custom: ping only Vassili from ");
	printLongAddr(&ServerIpAddress);
	printf("\n");
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
  
  uip_ipaddr_t ipaddr;
  struct uip_ds6_addr *root_if;

  PROCESS_BEGIN();

  PROCESS_PAUSE();

  printf("UDP server started. nbr:%d routes:%d\n",
         NBR_TABLE_CONF_MAX_NEIGHBORS, UIP_CONF_MAX_ROUTES);

#if UIP_CONF_ROUTER
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
    printf("Created a new RPL dag\n");
  } else {
    printf("Failed to create a new RPL DAG\n");
  }
#endif /* UIP_CONF_ROUTER */
  
  print_local_addresses();

  /* The data sink runs with a 100% duty cycle in order to ensure high 
     packet reception rates. */
  NETSTACK_MAC.off(1);

  server_conn = udp_new(NULL, UIP_HTONS(UDP_CLIENT_PORT), NULL);
  if(server_conn == NULL) {
    printf("No UDP connection available, exiting the process!\n");
    PROCESS_EXIT();
  }
  udp_bind(server_conn, UIP_HTONS(UDP_SERVER_PORT));

/*         
  client_conn = udp_new(NULL, UIP_HTONS(UDP_SERVER_PORT), NULL); 
  if(client_conn == NULL) {
    printf("No UDP connection available, exiting the process!\n");
    PROCESS_EXIT();
  }
  udp_bind(client_conn, UIP_HTONS(UDP_CLIENT_PORT)); 
*/

  static struct etimer periodic;
  static struct ctimer backoff_timer;

  etimer_set(&periodic, SEND_INTERVAL);
  while(1) {
    PROCESS_YIELD();
    if(ev == tcpip_event) {
      tcpip_handler();
    } 
    
    if(etimer_expired(&periodic)) {
      etimer_reset(&periodic);
      //ctimer_set(&backoff_timer, SEND_TIME, ping_only, NULL);
      
      counter++;
      
		printf("R: %d, icmp_send: %d\n",counter, uip_stat.icmp.sent);
		printf("R: %d, icmp_recv: %d\n",counter, uip_stat.icmp.recv);
		   
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
