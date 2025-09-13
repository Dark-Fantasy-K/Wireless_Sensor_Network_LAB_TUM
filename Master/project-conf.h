#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

// PHY LAYER PARAMETERS
#define GROUP_CHANNEL 12
#define MAX_NODES 8
// Use the lowest value to create multi-hop network.
#define TX_POWER 0
#define TX_POWER_MAX 7

// MAC LAYER PARAMETERS   default: checkrate are csma, contikimac and 8
#define NETSTACK_CONF_MAC csma_driver        //*nullmac_driver, or csma_driver
#define NETSTACK_CONF_RDC nullrdc_driver	 // *nullrdc_driver, or contikimac_driver
#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE 64 // *16 the checkrate in Hz. It should be a power of 2!

// Max number of nodes in the network.
#define MASTER_NODE_ID 64849
#define RSSSI_TH     -80



// heart beat
#define TOLERANCE 5




// Sensor 
#define BUFFER_SIZE	3
#define DIS_THRES	20
#define	LIGHT_THRES	500
#define	TEM_THRES	10

// Hello Process Parameters for system 
#define HELLO_INTERVAL 5
#define HELLO_SEQ_ID   100  



/************PACKET TYPES *****************/
/*TYPE MAPPING
 * 0- set  
 * 1- Broadcast Hello packet
 
 * */
/* ==== BROADCAST MESSAGE TYPES ==== */
#define HELLO_PACKET       1  // Initial broadcast to build neighbor and route table.


/* ==== UNICAST MESSAGE TYPES  ====*/
#define RT_REPORT_PACKET   2
#define SENSOR_DATA_PACKET 3
#define ADVERTISE_PACKET   4
#define HEARTBEAT_PACKET   5
#define NEWNODE_PACKET     6

#endif /* PROJECT_CONF_H_ */
