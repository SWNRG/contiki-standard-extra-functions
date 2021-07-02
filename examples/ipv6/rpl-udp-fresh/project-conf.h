#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/* Print only the last part of the address (e.g. 02). Handy for debugging */
#ifndef printShortAddr
#define printShortAddr(addr) printf(" %02x ",((uint8_t *)addr)[15])
#endif
/* Printf full IPv6 address without DEBUG_FULL */
#define printLongAddr(addr) printf("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])

#ifndef WITH_NON_STORING
#define WITH_NON_STORING 0 /* Set this to run with non-storing mode */
#endif /* WITH_NON_STORING */

#undef NBR_TABLE_CONF_MAX_NEIGHBORS
#undef UIP_CONF_MAX_ROUTES

//#ifdef TEST_MORE_ROUTES
/* configure number of neighbors and routes */
#define NBR_TABLE_CONF_MAX_NEIGHBORS     10


//#define UIP_CONF_MAX_ROUTES   30
// George OCt 2020 is this better for big networks?
#define UIP_CONF_MAX_ROUTES   40





//#else
/* configure number of neighbors and routes */
//#define NBR_TABLE_CONF_MAX_NEIGHBORS     10
//#define UIP_CONF_MAX_ROUTES   10
//#endif /* TEST_MORE_ROUTES */

#define PERIOD 300
/* Dixon Q Test for how many previous ICMP counted */
#ifndef dixon_n_vals
#define dixon_n_vals 7 /* Change this within [3,21] */ 
#endif
/* Also for Dixon Q test */
#ifndef confidence_level
#define confidence_level 1 // 0 = q90, 1 = q95, 2 = q99
#endif
/* printouts in dixontestOut & dixontestQIn */
#ifndef PRINT_ON
#define PRINT_ON 0
#endif

/* Enable only when collecting statistics for overhead comparison */
#ifndef OVERHEAD_STATS
#define OVERHEAD_STATS 1
#endif


/* variable in contiki/core/net/rpl/rpl-mrhof.c 
 * It will print all details about chosen parent.
 * Good for monitoring MRHOF behaviour and nodes' rank
 */
#ifndef PRINT_CHOOSING_PARENT_DETAILS
#define PRINT_CHOOSING_PARENT_DETAILS 0
#endif
 
 
//#undef NETSTACK_CONF_MAC
//#define NETSTACK_CONF_MAC     csma_driver

#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC     nullrdc_driver
#undef NULLRDC_CONF_802154_AUTOACK
#define NULLRDC_CONF_802154_AUTOACK       1

/* Define as minutes */
#define RPL_CONF_DEFAULT_LIFETIME_UNIT   60

/* 10 minutes lifetime of routes */
#define RPL_CONF_DEFAULT_LIFETIME        10

#define RPL_CONF_DEFAULT_ROUTE_INFINITE_LIFETIME 1

#define UIP_CONF_STATISTICS 1   // stats ON
#define RPL_CONF_STATS 1 // enable counting of dio_timer resets 

#if WITH_NON_STORING
#undef RPL_NS_CONF_LINK_NUM
#define RPL_NS_CONF_LINK_NUM 40 /* Number of links maintained at the root. Can be set to 0 at non-root nodes. */
#undef UIP_CONF_MAX_ROUTES
#define UIP_CONF_MAX_ROUTES 0 /* No need for routes */
#undef RPL_CONF_MOP
#define RPL_CONF_MOP RPL_MOP_NON_STORING /* Mode of operation*/
#endif /* WITH_NON_STORING */

#endif
