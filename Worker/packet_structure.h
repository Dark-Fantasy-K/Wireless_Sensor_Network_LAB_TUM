#ifndef PACKET_STRUCTURES_H
#define PACKET_STRUCTURES_H
/*******************STRUCTURES**********************/
// This structure is used routing table entries.
// Total byte = 8 byte.
typedef struct rt_entry{
	struct rt_entry *next;
	//uint8_t type;// Standard C includes:
	linkaddr_t dest;
	linkaddr_t next_hop;
	uint8_t tot_hop;		// Total hop number for this destination.
	int16_t metric;
	uint16_t seq_no;
	// used for constructiong the local routing table
}rt_entry;


typedef struct heartbeat_packet
{
  uint8_t type;  
  linkaddr_t src;
}heartbeat_packet;


typedef struct newnode_packet
{
  uint8_t type;  
  linkaddr_t src;
}newnode_packet;




typedef struct{

	uint8_t type;// Standard C includes:
	uint8_t no_entries;
	rt_entry table[MAX_NODES];

}routing_table;


struct rt_entry_pkt{
	uint8_t type;// Standard C includes:
	linkaddr_t src;
	uint8_t hop_count;
	uint16_t seq_id;
	int battery;  
	linkaddr_t rt_src;
	linkaddr_t rt_dest;
	linkaddr_t rt_next_hop;
	uint8_t rt_tot_hop;		// Total hop number for this destination.
	int16_t rt_metric;
	uint16_t rt_seq_no;
	// used for constructiong the local routing table
};


//the packet used for intial the set-up process
struct dio_packet {
	uint8_t type;
	linkaddr_t src;
	linkaddr_t src_master;                 // original sender (Master node)
	uint8_t hop_count;             // current hop count from master
	uint16_t seq_id;               // sequence ID to prevent loops
  };


/*
struct dao_packet {
  	uint8_t type;// Standard C includes:
	linkaddr_t src;
	//uint8_t hop_count; 
	uint16_t seq_id; 
	uint8_t no_entries;
	rt_entry table; // battery status
};
*/

struct advertise_packet{
	uint8_t type;
	linkaddr_t dest;
	linkaddr_t advertise_ch;      // current hop count from master
	uint16_t tot_hop;
	uint16_t seq_id;               // sequence ID to prevent loop
};


typedef struct sensor_data
{
    uint8_t type;
    linkaddr_t  source;
    int light_lux;
    int distance;
    int battery;
    int temperature;
    /* data */
}sensor_data;
/********************ROUTING LIST*************************/


#endif
