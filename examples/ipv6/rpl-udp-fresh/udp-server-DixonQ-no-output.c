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
#define DIXONQ_PRINT_OFF 0

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

/* When the controller detects version number attack, it orders to stop
 * resetting the tricle timer. The variable lies in rpl-dag.c
 */
#include "net/rpl/rpl-dag.c"
extern uint8_t ignore_version_number_incos;

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
#if DIXONQ_PRINT_OFF
		 printf("%s from ", appdata);
		 printLongAddr(child_node);	
		 printf("\n");  
#endif		  
    } 
    else{    
    	/* printing an incoming message, e.g. various enviromental measurements */
#if DIXONQ_PRINT_OFF
		 printf("%s from ", appdata);
		 printLongAddr(child_node);
		 printf("\n");
#endif
#if SERVER_REPLY
    	 PRINTF("Server Replying... \n");
    	 send_custom_msg(&UIP_IP_BUF->srcipaddr, server_msg);    
#endif
	}
  }
}
/*-------------- All direct children and their descentants -------------------*/
static void
print_all_routes(void)
{
	uip_ds6_route_t *r;
	uip_ipaddr_t *nexthop;
	uip_ipaddr_t *local_child; 	 	 	
		 	 	
	for(r = uip_ds6_route_head();
		r != NULL;
		r = uip_ds6_route_next(r)) {
		
		 nexthop = uip_ds6_route_nexthop(r);
		 local_child = &r->ipaddr;

		 PRINTF("Counter: %d Route: %02d -> %02d\n", counter, 
					r->ipaddr.u8[15], nexthop->u8[15]);

		/* BE CAREFUL: WE DONT WANT TO MESS WITH THE IPs in RPL.
		 * Hence local_child will be transformed from global IP to
		 * local IP, by transforming local_child[0] from fd00 to fe80
		 */		 
		 local_child->u8[0] = (uint8_t *)254;
		 local_child->u8[1] = (uint8_t *)128;

		 /* Controller is reading a line starting from "Route " */
#if DIXONQ_PRINT_OFF
		 printf("Route: ");
		 printIP6Address(local_child); //direct child
		 //printLongAddr(&r->ipaddr); // fd00:...
		 printf(" ");
		 printIP6Address(nexthop); // all decentant(s)
		 /* when lt >>> 0, the connection does not exist any more */
		 printf(" lt:%lu\n", r->state.lifetime);	 
#endif
	}//for *r 
} 	
/*---------------------------------------------------------------------------*/
static void
print_stats(void)
{
	printf("Printing all ENABLED stats\n");  
#define PRINTROUTES 0
#if PRINTROUTES  
	print_all_routes();
#endif	 

#define PRINTNBRS 0
#if PRINTNBRS
	print_all_neighbors(); // it seems to have problem...
#endif
}
/****************NOT USED! Direct children, only once **********************/
static void 
print_all_neighbors(void)
{
	/*
	 * The same information (sink's direct children),
	 * can be aquired from the routes. When child = father,
	 * this is a direct child of sink.
	 */	 
	uip_ds6_nbr_t *nbr = nbr_table_head(ds6_neighbors);
	//printf("Counter %d: My nbr-children only: \n",counter);    	
	
	while(nbr != NULL) {
#if DIXONQ_PRINT_OFF
		printf("Sinks child: ");
		printLongAddr(&nbr->ipaddr);
		printf("\n");
		nbr = nbr_table_next(ds6_neighbors, nbr);
#endif
	}
	//printf("End of neighbors\n"); 		
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
serial_input_byte(unsigned char c)
{
	int i=0;
	int t=0;
	uint8_t node_ip_length = 41; /* 32(IP) + 7(:) + 2([]) 0-40 */ 
	uint8_t node_ip[node_ip_length];  
	uip_ipaddr_t uip_node_ip;
	
	char buf[MAX_PAYLOAD_LEN];
	char in_comm[3];//[3];
	//in_comm = (char *) malloc(3);

	PRINTF("DATA in from UART\n"); /* java crashes if too fast */
				
	if(c != '\n' && uart_buffer_index < UART_BUFFER_SIZE){
	  uart_buffer[uart_buffer_index++] = c;
	}
	else{
      uart_buffer[uart_buffer_index] = '\0';
      uart_buffer_index = 0;

      printf("Controller message: %s\n",uart_buffer); 

		in_comm[0] = uart_buffer[0];
		in_comm[1] = uart_buffer[1]; // e.g. "SP"
		in_comm[2] ='\0';
		PRINTF("in_comm %s\n",in_comm);

		i = i+3; /* jump the space char " ", but include the [ */
						
		/* Start processing the message */
		while(uart_buffer[i]!='\0'){
			node_ip[t]=uart_buffer[i];					
			PRINTF("buf_in:%c, node_ip[%u] %c\n",uart_buffer[i],t,node_ip[t]);
			t++; i++;   
		}
		PRINTF("END INCOMING IP\n");
		
		/* Transform IP to global (fd00) : */
		node_ip[2]='d';
		node_ip[3]='0';

//if successfuly transform{
	   /* Convertion from String to IPv6 */
		//uiplib_ip6addrconv("[fd00:0000:0000:0000:c30c:0000:0000:0002]", &uip_node_ip);
		uiplib_ip6addrconv(node_ip, &uip_node_ip);
		sprintf(buf, in_comm);
		uip_udp_packet_sendto(client_conn, buf, strlen(buf),
		 		&uip_node_ip, UIP_HTONS(UDP_SERVER_PORT));
#define PRINT_DET 0
#if PRINT_DET
		printf("#SEND %s to node ",in_comm);
		printShortAddr(&uip_node_ip);
		printf("\n");
	
		printLongAddr(&uip_node_ip);
		printf(", in_comm msg: %s\n", in_comm);
#endif	
		 //}else{
#define print_output 0
#if print_output
			 printf("Failed incoming IPv6: ");
			 int g;
			 for (g = 0; g<sizeof(node_ip)+1; g++){
				 printf("%c",&node_ip[g]);
			 }
			 printf("\n");
			 printf("Output: ");
			 printLongAddr(&uip_node_ip);
			 printf("\n");
#endif
			// printf("FAILED SENDING MESSAGE!\n\n");
		   //}
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
         
  client_conn = udp_new(NULL, UIP_HTONS(UDP_SERVER_PORT), NULL); 
  if(client_conn == NULL) {
    printf("No UDP connection available, exiting the process!\n");
    PROCESS_EXIT();
  }
  udp_bind(client_conn, UIP_HTONS(UDP_CLIENT_PORT)); 

	/* separate threat is not needed??? */
	/* if mote==Z1, uart0_set_input, if mote==sky, uart1_set_input */
	uart0_init(BAUD2UBR(115200));
	uart0_set_input(serial_input_byte);

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
#if DIXONQ_PRINT_OFF
      ctimer_set(&backoff_timer, SEND_TIME, ping_only, NULL);
#endif      
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
