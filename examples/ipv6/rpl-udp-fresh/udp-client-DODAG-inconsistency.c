#include "contiki.h"
#include "lib/random.h"
#include "sys/ctimer.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ip/uip-udp-packet.h"

#include "net/rpl/rpl.h"      //coral

#include "sys/ctimer.h"
#include <stdio.h>
#include <string.h>

//#include "dixonQtest.h"
#include "dixonQtestIn.c"
#include "dixonQtestOut.c"

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678
#define UDP_EXAMPLE_ID  190

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

//#define DEBUG DEBUG_FULL
#if DEBUG
#include "net/ip/uip-debug.h"
#endif


#include "apps/powertrace/powertrace.h"
unsigned seconds=60*5;// warning: if this variable is changed, then the kinect variable the count the minutes should be changed
double fixed_perc_energy = 1;// 0 - 1
unsigned variation = 2;//0 - 99
/* The variable "fixed_perc_energy" correspods to the variable that will tell the PowertraceK the percentage of energy the node will start, considering the full battery capacity is 1000000 microAh. For example, setting fixed_perc_energy = 0.2 means that the nodes will be initiated with 20 % of 1000000 microAh = 200000 microAh. */


#ifndef PERIOD
#define PERIOD 500 /* increase it to 700 avoid flooding */
#endif

#define START_INTERVAL		(15 * CLOCK_SECOND)
#define SEND_INTERVAL		(PERIOD * CLOCK_SECOND)
#define SEND_TIME		(random_rand() % (SEND_INTERVAL))

#define MAX_PAYLOAD_LEN		60

static struct uip_udp_conn *client_conn;
static struct uip_udp_conn *server_conn;

static uip_ipaddr_t server_ipaddr;
static uip_ipaddr_t destination_ipaddr;

/* Get the preffered parent, and the current own IP of the node */
#include "net/rpl/rpl-icmp6.c"
extern   rpl_parent_t *dao_preffered_parent;
extern   uip_ipaddr_t *dao_preffered_parent_ip;
extern   uip_ipaddr_t dao_prefix_own_ip;

/* Monitor this var. When changed, the node has changed parent */
static rpl_parent_t *my_cur_parent;
static uip_ipaddr_t *my_cur_parent_ip;
static int counter=0; //counting rounds. Not really needed

/* When this variable is true, start sending UDP stats */
static uint8_t sendUDP = 0; 

/* When this variable is true, start sending ICMP stats */
static uint8_t sendICMP = 0; 

/* When true, the controller will start probing all nodes for detais */
enablePanicButton = 0;

/* When the controller detects version number attack, it orders to stop
 * resetting the tricle timer. The variables below lie in rpl-dag.c
 */
#include "net/rpl/rpl-dag.c"
extern uint8_t ignore_version_number_incos; //if == 1 DIO will not reset trickle
extern uint8_t dio_bigger_than_dag; // if version attack, this will be 1
extern uint8_t dio_smaller_than_dag; // if version attack, this will be 1
 
static int prevICMRecv = 0;
static int prevICMPSent = 0;

/*-----------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client process");
AUTOSTART_PROCESSES(&udp_client_process);
/*-----------------------------------------------------------------------*/
static int seq_id;
static int reply;
/*-----------------------------------------------------------------------*/
static void
send_msg_to_sink(char *inMsg, uip_ipaddr_t *addr)
{
  unsigned char buf[50]; //dont forget, 50 chars
  unsigned char msg[50];
  
  strcpy(msg, inMsg);
  
#define PRINT_PARENT 0
#if PRINT_PARENT
  printf("%c",msg);
  printLongAddr(addr);
  printf(", sending to %d\n", server_ipaddr.u8[sizeof(server_ipaddr.u8) - 1]);
#endif
  
  sprintf(buf, 
  	"[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", 
  		((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], 
  		((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], 
  		((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], 
  		((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], 
  		((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], 
  		((uint8_t *)addr)[15] 
  	);

	strcat(msg, buf);
	
   uip_udp_packet_sendto(client_conn, msg, strlen(msg),
  			&server_ipaddr, UIP_HTONS(UDP_SERVER_PORT));
}
/*---------------------------------------------------------------------------*/
static void 
send_all_neighbors(void)
{ 
	uip_ds6_nbr_t *nbr = nbr_table_head(ds6_neighbors);
	PRINTF("Counter %d: My neighbors only: \n",counter);    	
	
	while(nbr != NULL) {
		printf("My neighbor: ");
		printLongAddr(&nbr->ipaddr);
		printf("\n");
		nbr = nbr_table_next(ds6_neighbors, nbr);
		
		send_msg_to_sink("N1:", nbr);
		
	}
	PRINTF("End of neighbors\n"); 		
}
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
  char *str;

  if(uip_newdata()) {
    str = uip_appdata;
    str[uip_datalen()] = '\0';
    reply++;

    PRINTF("uip Message Received from SINK: %s\n",str);

	 if(str[0] == 'S' && str[1] == 'P'){
		 printf("Responding to sink's probe about my parent\n"); 
		 /* Send the parent again, after sink's request */
		 send_msg_to_sink("NP:", my_cur_parent_ip);  	 
	 }else if(str[0] == 'N' && str[1] == '1'){ 
			printf("CO-MSG: Controller is probing my neighbors\n");		
			send_all_neighbors();			
	 }else if(str[0] == 'U' && str[1] == '1'){ 
			printf("CO-MSG: Start sending UDP stats\n"); //sink asking for UDP sent/recv		
			
			sendUDP = 1;	
			
	 }else if(str[0] == 'U' && str[1] == '0'){ 
			printf("CO-MSG: Stop probing UDP stats\n"); 		
			
			sendUDP = 0;		
		
	 }else if(str[0] == 'I' && str[1] == '1'){ 
			printf("CO-MSG: Start sending ICMP stats\n"); //sink asking for UDP sent/recv		
				
			sendICMP = 1;	

	 }else if(str[0] == 'I' && str[1] == '0'){ 
			printf("CO-MSG: Stop probing ICMP stats\n"); 	
							
			sendICMP = 0;					
					
	 }else if(str[0] == 'N' && str[1] == '0'){ 
			printf("CO-MSG: Stop sending neighbors\n"); 	
	
	 }else if(str[0] == 'T' && str[1] == '1'){
	 			// sink orders to stop resetting trickle because version num attack
	 			printf("CO-MSG: Stop resetting trickle timer ON\n"); 	 	
				ignore_version_number_incos = 1;	

	 }else if(str[0] == 'T' && str[1] == '0'){
	 			// sink orders to stop resetting trickle because version num attack
	 			printf("CO-MSG: Stop resetting trickle timer OFF\n"); 	 	
				ignore_version_number_incos = 0;					

	 }else{	 
	 	PRINTF("DATA recv '%s' (s:%d, r:%d)\n", str, seq_id, reply);
	 }
  }
}
/*-----------------------------------------------------------------------*/
static void
send_packet(void *ptr)
{
  char buf[MAX_PAYLOAD_LEN];

  seq_id++; // TODO: change this with a random var

  PRINTF("DATA sending to %d 'Hello %d'\n",
         server_ipaddr.u8[sizeof(server_ipaddr.u8) - 1], seq_id);

  sprintf(buf, "Custom Data %d ", seq_id);
  uip_udp_packet_sendto(client_conn, buf, strlen(buf),
                        &server_ipaddr, UIP_HTONS(UDP_SERVER_PORT));
}
/*-----------------------------------------------------------------------*/
static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  PRINTF("Client IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      PRINTF("\n");
      /* hack to make address "final" */
      if (state == ADDR_TENTATIVE) {
			uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
      }
    }
  }
}
/*-----------------------------------------------------------------------*/
static void
set_global_address(void)
{
  uip_ipaddr_t ipaddr;

  uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
 
#if 0
/* Mode 1 - 64 bits inline */
   uip_ip6addr(&server_ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 1);
#elif 1
/* Mode 2 - 16 bits inline */
  uip_ip6addr(&server_ipaddr, UIP_DS6_DEFAULT_PREFIX, 
  			0, 0, 0, 0, 0x00ff, 0xfe00, 1);
#else
/* Mode 3 - derived from server link-local (MAC) address */
  uip_ip6addr(&server_ipaddr, UIP_DS6_DEFAULT_PREFIX, 
  			0, 0, 0, 0x0250, 0xc2ff, 0xfea8, 0xcd1a); //redbee-econotag
#endif
}
/*-----------------------------------------------------------------------*/
static void
monitor_ver_num(void) //TODO: Implement this method
{
	static int ver_num_attacks = 0; // be careful, it could increase for ever!
	char buf[MAX_PAYLOAD_LEN];
	
#define PRINT_DETAILS 0

	if(dio_bigger_than_dag == 1 || dio_smaller_than_dag == 1 ){
#if PRINT_DETAILS
		printf("DIO dio_bigger_than_dag: %d\n",dio_bigger_than_dag);
		printf("DIO dio_smaller_than_dag: %d\n",dio_smaller_than_dag);
		printf("R:%d, VA:%d\n",counter,ver_num_attacks);
#endif
		sprintf(buf, "[VA:%d]",++ver_num_attacks);
		uip_udp_packet_sendto(client_conn, buf, strlen(buf),
					  &server_ipaddr, UIP_HTONS(UDP_SERVER_PORT));
	}			     	
	while(ignore_version_number_incos==1){
#if PRINT_DETAILS
		printf("Client: dags per hour > 20, ignoring resets");
#endif
	}	
}

/*-----------------------------------------------------------------------*/
static void 
monitor_DAO(void)
{
/* dont forget: parent_ip = 
 * rpl_get_parent_ipaddr(parent->dag->preferred_parent)
 */
	//uip_ipaddr_t *addr; // is this needed ???
	
#define PRINT_CHANGES 0

	/* In contiki, you can directly compare if(parent == parent2) */
	if(my_cur_parent != dao_preffered_parent){
#if PRINT_CHANGES
		printf("Parent changed. Old parent->");
		printLongAddr(my_cur_parent_ip);
		printf(", new->");
		printLongAddr(dao_preffered_parent_ip);
		printf("\n");
#endif
		my_cur_parent = dao_preffered_parent;
		my_cur_parent_ip = dao_preffered_parent_ip;
		
#define PRINT_PARENT 0
#if PRINT_PARENT
	   printf("NP:");
	   printLongAddr(my_cur_parent_ip);
	   printf(", sending to %d\n", 
	   		server_ipaddr.u8[sizeof(server_ipaddr.u8) - 1]);
#endif
		send_msg_to_sink("NP:",my_cur_parent_ip);
	}
}
/************* STATISTICS REQUESTED (ENABLED) BY THE SINK **************/ 
static void
sendUDPStats(void)
{
   char buf[MAX_PAYLOAD_LEN];
#define PRINT_DET 0
#if PRINT_DET   
	printf("Sending UDP stats to sink\n");
	printf("R:%d, udp_sent:%d\n",counter,uip_stat.udp.sent);
	printf("R:%d, udp_recv:%d\n",counter,uip_stat.udp.recv);
#endif
	sprintf(buf, "[SU:%d %d]",uip_stat.udp.sent,uip_stat.udp.recv);
	uip_udp_packet_sendto(client_conn, buf, strlen(buf),
			     &server_ipaddr, UIP_HTONS(UDP_SERVER_PORT));		
}
/*-----------------------------------------------------------------------*/
sendICMPStats(void)
{
   char buf[MAX_PAYLOAD_LEN];
#define PRINT_DET 0
#if PRINT_DET      
		printf("Sending ICMP stats to sink\n");
		printf("R:%d, icmp_sent:%d\n",counter,uip_stat.icmp.sent);
		printf("R:%d, icmp_recv:%d\n",counter,uip_stat.icmp.recv);
#endif
		sprintf(buf, "[SI:%d %d]",uip_stat.icmp.sent,uip_stat.icmp.recv);
		uip_udp_packet_sendto(client_conn, buf, strlen(buf),
				     &server_ipaddr, UIP_HTONS(UDP_SERVER_PORT));				
}
/*-----------------------------------------------------------------------*/


PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer periodic;
  static struct ctimer backoff_timer;

  PROCESS_BEGIN();
  PROCESS_PAUSE();


//powertrace_start(CLOCK_SECOND * 60);


//George original power trace was replaced with powertraceK
// there are more printouts, they are commented out in apps/powertrace/powertrace.c
powertrace_start(CLOCK_SECOND * seconds, seconds, fixed_perc_energy, variation);


  set_global_address();

	printf("PERIOD defined: %d\n",PERIOD);
	
  /* The data sink runs with a 100% duty cycle in order to ensure high 
     packet reception rates. */
  NETSTACK_MAC.off(1);

  PRINTF("UDP client process started nbr:%d routes:%d\n",
         NBR_TABLE_CONF_MAX_NEIGHBORS, UIP_CONF_MAX_ROUTES);

  print_local_addresses();

  /* new connection with remote host */
  client_conn = udp_new(NULL, UIP_HTONS(UDP_SERVER_PORT), NULL); 
  if(client_conn == NULL) {
    printf("No UDP connection available, exiting the process!\n");
    PROCESS_EXIT();
  }
  udp_bind(client_conn, UIP_HTONS(UDP_CLIENT_PORT)); 

  // Destination PORT
  server_conn = udp_new(NULL, UIP_HTONS(UDP_CLIENT_PORT), NULL);
  if(server_conn == NULL) {
    printf("No UDP connection available, exiting the process!\n");
    PROCESS_EXIT();
  }
  udp_bind(server_conn, UIP_HTONS(UDP_SERVER_PORT));

  PRINTF("Created a connection with the server ");
  PRINT6ADDR(&client_conn->ripaddr);
  PRINTF(" local/remote port %u/%u\n",
  		UIP_HTONS(client_conn->lport), UIP_HTONS(client_conn->rport));

  uint8_t dixonQCounter = 0; /* when icmp indicates possible attack, use it */
  
  printf("DixonQ active n value: %d\n",dixon_n_vals);
  printf("DixonQ confidence_level: %d\n",confidence_level);
  	 
  etimer_set(&periodic, SEND_INTERVAL);
  while(1) {
    PROCESS_YIELD();

    monitor_DAO();
    
    monitor_ver_num();
	 
    if(ev == tcpip_event) {
      tcpip_handler();
    }

    if(etimer_expired(&periodic)) {
      etimer_reset(&periodic);

      counter++;
      
      uint8_t *ipLast = ((uint8_t *)my_cur_parent_ip)[15];
#if PRINT_IP_ON      
      printf("my_cur_parent_ip: "); 
      printShortAddr(my_cur_parent_ip);
      printf("\n");
      printf("My parent last oct: %d\n",ipLast);
#endif
      if (counter%10 == 0 & ipLast == 1){ // sink only???
      	PRINTF("Cur Round: %d\n",counter);
      }
      
      /* sending periodic UDP data to sink (e.g. temperature measurements) */
      ctimer_set(&backoff_timer, SEND_TIME, send_packet, NULL);   
	
if (counter > 30){	 // too many messages      
      printf("R: %d, trickle resets number: %d\n",counter,rpl_stats.resets);
      printf("R: %d, global repairs: %d\n",counter,rpl_stats.global_repairs);
      printf("R: %d, local repairs: %d\n",counter,rpl_stats.local_repairs);
      
		printf("R:%d, icmp_sent_TOTAL:%d\n",counter,uip_stat.icmp.sent);
		printf("R:%d, icmp_recv_TOTAL:%d\n",counter,uip_stat.icmp.recv);
}

/***********************************************************************/
/* Hybrid Security Mechanism: This is the node-part.
 * it is lightweight, and it works in conjuction with the controller.
 * DixonQ outlier holds only a few values in an array (usually within [3..10]
 * and checks every newcoming ICMP value (in & out) for outliers.	
 *
 * How to enable it: 
 * Either the node(s) are continiously running it, or the controller asks for
 * it by sending a message to turn it on.
 */	
		int ICMPSent = uip_stat.icmp.sent - prevICMPSent;
		prevICMPSent = uip_stat.icmp.sent;
		int ICMPRecv = uip_stat.icmp.recv - prevICMRecv;
		prevICMRecv = uip_stat.icmp.recv;
				
		//printf("R:%d, CURRENT_icmp_sent:%d\n",counter,ICMPSent);
		//printf("R:%d, CURRENT_icmp_recv:%d\n",counter,ICMPRecv);
		
		/* Try these for total number of packets sent/received until now */		
		//int dixonQAnswerSent = addDixonQOut(uip_stat.icmp.sent);
		//int dixonQAnswerRecv = addDixonQIn(uip_stat.icmp.recv);

		int dixonQAnswerSent = addDixonQOut(ICMPSent);
		int dixonQAnswerRecv = addDixonQIn(ICMPRecv);
						
		/* Continiously monitoring fo abnormalities in icmp (Dixon q test outliers */
		if (counter > dixon_n_vals + 3){ /* On bootstrap network is still forming */
				/* both ICMP outliers, definitely under attack. Look for another parent */
				if( dixonQAnswerSent > 0 && dixonQAnswerRecv > 0)
				{
					/* put current parent in black list and choose a new one? */
					printf("R: %d, PANIC both icmps outliers, choose a new parent maybe?\n",counter);
				}
				else{			
					//TODO: Differenciate actions for small/big outlier
					if( dixonQAnswerSent > 0 ){ /* dixonQAnswerSent = 1 or 2 */
						printf("R:%d, Sent icmps out of bounds (outlier)\n",counter);
						sendICMP = 1 ;
						enablePanicButton = 1; /* Central Management should ask all nodes to sendICMP */
						dixonQCounter=1;
					}
				
					if( dixonQAnswerRecv == 1 || dixonQAnswerRecv == 2){
						printf("R:%d, Recv icmps out of bounds (outlier)\n",counter);
						sendICMP = 1 ;
						enablePanicButton = 1; /* Central Management should ask all nodes to sendICMP */
						dixonQCounter=1;
					}
				}	
		} //if counter > dixon_n_vals + 3 at the beggining the network is still forming
    }
/********** End of hybrid security node part implementing dixonQ outlier ******/

    if (sendUDP != 0){
   	sendUDPStats();   	
   	//ctimer_set(&backoff_timer, SEND_TIME, sendUDPStats, NULL); 
    }
   
	 if (sendICMP != 0){
		sendICMPStats();
   	//ctimer_set(&backoff_timer, SEND_TIME, sendICMPStats, NULL); 
    }
    //ctimer_set(&backoff_timer, SEND_TIME, send_all_neighbors, NULL);     
  }
  PROCESS_END();
}
/*-----------------------------------------------------------------------*/
