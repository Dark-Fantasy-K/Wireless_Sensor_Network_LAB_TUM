#define LOG_LEVEL LOG_LEVEL_INFO
#define LOG_MODULE "LEACH"


#include "contiki.h"
#include "net/nullnet/nullnet.h"
#include "net/packetbuf.h"
#include "net/netstack.h"
#include "dev/leds.h"
#include "net/linkaddr.h"
#include "sys/node-id.h"
#include "sys/log.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "net/linkaddr.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/saadc-sensor.h"		// Saadc to read Battery Sensor.
#include "common/temperature-sensor.h"
#include "my_sensor.h"
#include "my_functions.h"
#include "packet_structure.h"
#include "project-conf.h"

// used for recording the packet seq, prevent loop
static uint16_t last_seq_id = 0;
static linkaddr_t addr_master ={{0xf4, 0xce, 0x36, 0xb0, 0xdf, 0x60, 0xfd, 0x51}};

static short adjacency_matrix[MAX_NODES][MAX_NODES];  
static  linkaddr_t node_index_to_addr[MAX_NODES] = {
  {{0xf4, 0xce, 0x36, 0xb0, 0xdf, 0x60, 0xfd, 0x51}}, // node 0
  {{0xf4, 0xce, 0x36, 0x19, 0x94, 0x72, 0xc0, 0x67}}, // node 1
  {{0xf4, 0xce, 0x36, 0x7a, 0x82, 0x93, 0xea, 0xcd}}, // node 2
  {{0xf4, 0xce, 0x36, 0x77, 0x7b, 0x50, 0xa8, 0xc7}}, // node 3
  {{0xf4, 0xce, 0x36, 0x4a, 0x91, 0x02, 0x45, 0xb3}}, // node 4
  {{0xf4, 0xce, 0x36, 0x95, 0x48, 0x13, 0xf2, 0x29}}, // node 5
  {{0xf4, 0xce, 0x36, 0x64, 0x4e, 0x64, 0x2d, 0xae}}, // node 6
  {{0xf4, 0xce, 0x36, 0x53, 0x21, 0x0a, 0x51, 0x32}}, // node 7
};
static volatile uint8_t net_rejoin = 0;
static int num_known_nodes = MAX_NODES;

// sensor data transmission
static uint8_t trans_flag;
static int distance_buffer[BUFFER_SIZE], light_buffer[BUFFER_SIZE], temperature_buffer[BUFFER_SIZE];
static int global_index;
static int distance_av_0, light_av_0, temperature_av_0;
static int distance_av_1, light_av_1, temperature_av_1;
static sensor_data recv_message;
static float battery[MAX_NODES] ={1};

LIST(local_rt_table);
MEMB(rt_mem,rt_entry,MAX_NODES);

//LIST(permanent_rt_table);
//MEMB(permanent_rt_mem,rt_entry,MAX_NODES);

// heart beat
static volatile uint8_t Node_death;
static uint8_t heart_record[MAX_NODES];
//static linkaddr_t addr_ch;
//static uint8_t is_ch;


//sensor data 
void distance_av_cal(){
	distance_av_1 = distance_av_0;
	for(int i=0; i<BUFFER_SIZE; i++){
		distance_av_0 += distance_buffer[i];
	}
	distance_av_0 /= BUFFER_SIZE;
}

void light_av_cal(){
	light_av_1 = light_av_0;
	for(int i=0; i<BUFFER_SIZE; i++){
		light_av_0 += light_buffer[i];
	}
	light_av_0 /= BUFFER_SIZE;
}

void temperture_av_cal(){
	temperature_av_1 = temperature_av_0;
	for(int i=0; i<BUFFER_SIZE; i++){
		temperature_av_0 += temperature_buffer[i];
	}
	temperature_av_0 /= BUFFER_SIZE;
}

// routing discovery part 
rt_entry * check_local_rt(const linkaddr_t *addr)
{ 
  rt_entry *e= list_head(local_rt_table);
	for(; e != NULL; e = e->next)
	{
		if(linkaddr_cmp(&e->dest, addr))
		{
			return e;
		}
	}
	return e;
}

uint16_t get_node_id_from_linkaddr(const linkaddr_t *addr) {
  for(int i =0; i<=MAX_NODES;i++)
  {
    if(linkaddr_cmp(&node_index_to_addr[i], addr))
    {
      return i;
    }
  }
  return -1;
}

void print_adjacency_matrix()
{
  printf("Adjacency Matrix:\n   ");
  for (int j = 0; j < num_known_nodes; j++) {
    //print as node id 
    printf("%u     ",get_node_id_from_linkaddr(&node_index_to_addr[j]));
  }
  printf("\n");

  for (int i = 0; i < num_known_nodes; i++) {
    printf("%u",get_node_id_from_linkaddr(&node_index_to_addr[i]));
    for (int j = 0; j < num_known_nodes; j++) {
      if (adjacency_matrix[i][j] == -1)
        printf("  -  ");
      else
        printf("%3d  ", adjacency_matrix[i][j]);
    }
    printf("\n");
  }
}

int get_index_from_addr(const linkaddr_t *addr)
{
  for (int i = 0; i < num_known_nodes; i++) {
    if (linkaddr_cmp(addr, &node_index_to_addr[i])) {
      return i;
    }
  }

  return -1;
}

const linkaddr_t *get_next_hop_to(const linkaddr_t *dest, int is_permanent)
{
  if(is_permanent)
  {
    //for (rt_entry *e = list_head(permanent_rt_table); e != NULL; e = e->next) {
    //  if (linkaddr_cmp(&e->dest, dest)) {
    //    return &e->next_hop;
    //  }
    //}
  }
  else{
    for (rt_entry *e = list_head(local_rt_table); e != NULL; e = e->next) {
      if (linkaddr_cmp(&e->dest, dest)) {
        return &e->next_hop;
      }
    }
  }
  return NULL;  
}

void print_local_routing_table() {
  LOG_INFO("+------------------+ Local Routing Table: +--------------------+\n");
  int i = 0;
  for(rt_entry *e = list_head(local_rt_table); e != NULL; e = e->next) {
    uint16_t dest_id = get_node_id_from_linkaddr(&e->dest);
    uint16_t next_id = get_node_id_from_linkaddr(&e->next_hop);
    LOG_INFO("|No.%d | dest:%u | next:%u | tot_hop:%u | rssi:%d | seq:%u |\n",
            i++, dest_id, next_id,
            e->tot_hop, e->metric, e->seq_no);
  }
  LOG_INFO("+------------------+ ----------------------+--------------------+\n");
}

void forward_hello(struct dio_packet *pkt)
{
  nullnet_buf = (uint8_t *)pkt;
  nullnet_len = sizeof(pkt);
  NETSTACK_NETWORK.output(NULL);
}

void insert_entry_to_rt_table(const linkaddr_t *dst, const linkaddr_t *next_hop,uint8_t tot_hop, int16_t metric,uint16_t seq_no)
{
  rt_entry *e = memb_alloc(&rt_mem);
  if(e != NULL) {
    linkaddr_copy(&e->dest, dst);
    linkaddr_copy(&e->next_hop, next_hop);
    e->tot_hop = tot_hop;
    e->metric = metric;
    e->seq_no = seq_no;
    list_add(local_rt_table, e);  
  }
}

void patch_update_local_rt_table(const linkaddr_t *dst, const linkaddr_t *next,uint8_t tot_hop, int16_t metric,uint16_t seq_no)
{
  rt_entry* e = check_local_rt(dst);
    if (e == NULL)
    {
      insert_entry_to_rt_table(dst,dst,tot_hop,metric,seq_no);
      LOG_INFO("Discover new nodes adding to the rt table:\n");
    }
    else 
    {
      if(linkaddr_cmp(&e->dest, &linkaddr_node_addr))
      { 
        return;
      }
      else if(e->seq_no < seq_no)
      {
        if (abs(e->metric - metric) > 5)   
        {
          LOG_INFO("Update rt table with new metric:\n");
          e->metric = metric;
        }
      } 

    }



}

void print_rt_entries_pkt(const struct rt_entry_pkt *pkt)
{
  printf("DAO Packet from node ");
  uint16_t pkt_src_id = get_node_id_from_linkaddr(&pkt->src);
  printf("%u", pkt_src_id);
  printf("+-----------------------------------------------------------------------------------+\n");
  printf("|src   |  dest  | next_hop | hops | metric | seq_no |\n");
  printf("+-----------------------------------------------------------------------------------+\n");
    uint16_t src_id  = get_node_id_from_linkaddr(&pkt->rt_src);
    uint16_t dest_id = get_node_id_from_linkaddr(&pkt->rt_dest);
    uint16_t next_id = get_node_id_from_linkaddr(&pkt->rt_next_hop);
    printf("| %5u  |  %5u |   %5u  |  %3u |  %5d |  %5u |\n",
           src_id,
           dest_id,
           next_id,
           pkt->rt_tot_hop,
           pkt->rt_metric,
           pkt->rt_seq_no);

  printf("+-----------------------------------------------------------------------------------+\n");
}

bool parent_is_in_rt_table(const linkaddr_t *src)
{
  rt_entry *iter = list_head(local_rt_table);
  for(; iter != NULL; iter = iter->next) {
    if(linkaddr_cmp(&iter->dest, src))
    {
      return 1;
    }
    else;
  }
  return 0;
}

static void routing_report(const linkaddr_t *dest, uint8_t hop, int8_t rssi, uint16_t seq_id)
{
  static struct rt_entry_pkt pkt;
  //memset(&pkt, 0, sizeof(pkt));
  pkt.type = RT_REPORT_PACKET;
  linkaddr_copy(&pkt.src, &linkaddr_node_addr);
  pkt.hop_count = 0;
  pkt.seq_id = seq_id;
  pkt.battery = get_millivolts(saadc_sensor.value(BATTERY_SENSOR));
  
  rt_entry *iter = list_head(local_rt_table);
  iter=iter->next;
  linkaddr_copy(&pkt.rt_src, &iter->dest);
  for(; iter != NULL; iter = iter->next) {
    
    uint16_t dest_id = get_node_id_from_linkaddr(&iter->dest);
    uint16_t next_id = get_node_id_from_linkaddr(&iter->next_hop);
    printf("  dest: %i\n",dest_id );
    printf("  next_hop: %i\n", next_id);
    printf("  tot_hop: %d\n", iter->tot_hop);
    printf("  metric: %d\n", iter->metric);
    printf("  seq_no: %u\n", iter->seq_no);
    
    linkaddr_copy(&pkt.rt_dest,     &iter->dest);
    linkaddr_copy(&pkt.rt_next_hop, &iter->next_hop);
    pkt.rt_tot_hop = iter->tot_hop;
    pkt.rt_metric  = iter->metric;
    pkt.rt_seq_no  = iter->seq_no;

    //clock_wait(CLOCK_SECOND / 20);  // wait 50 ms
    nullnet_buf = (uint8_t *)&pkt;
    nullnet_len = sizeof(pkt);
    NETSTACK_NETWORK.output(dest); 

  }
  LOG_INFO("I have sent RT_REPORT_PACKET\n");
  uint16_t dest_id = get_node_id_from_linkaddr(dest);
  printf("Sending packet to dest: %i\n", dest_id);
}



// Receive hello packet callback
// 1.forward hello packet
// 2.reply to the src node
// 3,adding the hello packet info from the rt_table
static void DIO_PACKET_callback(const void *data, uint16_t len,
                           const linkaddr_t *src, const linkaddr_t *dest)
{
  int8_t rssi = (int8_t)packetbuf_attr(PACKETBUF_ATTR_RSSI);
  //LOG_INFO("Hello Packet Process begin\r\n");
  if(rssi <= RSSSI_TH)
  {
    LOG_WARN("low RSSI DAO, rejected\n\r");
    return;
  }

  // processing the hello packet info
  leds_single_on(LEDS_LED2);
  struct dio_packet *pkt = (struct dio_packet *)data;
  linkaddr_t report_src;
  linkaddr_copy(&addr_master, &pkt->src_master);
  linkaddr_copy(&report_src, &pkt->src);
  
  
  /*f(pkt->seq_id <=1)
  {
    for (int i = 0; i < MAX_NODES; i++) {
      for (int j = 0; j < MAX_NODES; j++) {
        adjacency_matrix[i][j] = (i == j) ? 255 : 0;
      }
    }
    memb_init(&rt_mem);
    list_init(local_rt_table);
    insert_entry_to_rt_table(&linkaddr_node_addr, &linkaddr_node_addr, 0, 0, 0);
    memb_init(&permanent_rt_mem);
    list_init(permanent_rt_table);
    last_seq_id = pkt->seq_id;
  }
  // Avoid loops: if already seen, drop
  else */
  /*if((pkt->seq_id <=last_seq_id) && parent_is_in_rt_table(&report_src)){
    leds_single_off(LEDS_LED2);
    LOG_INFO("The Packet has been Processed\r\n");
    return;
  }*/
  last_seq_id = pkt->seq_id;
  // processing the packet info
  
  //pkt->hop_count++;
  linkaddr_copy(&pkt->src, &linkaddr_node_addr);
  printf("--------------------My link-layer address: ");
  for(int i = 0; i < LINKADDR_SIZE; i++) {
    printf("%02x", linkaddr_node_addr.u8[i]);
  }
  printf("\n");
  printf("------------------------Pacekt link-layer address: ");
  for(int i = 0; i < LINKADDR_SIZE; i++) {
    printf("%02x",pkt->src.u8[i]);
  }
  printf("\n");
  // update_local_rt_table(master_node info + hello packet info);
  // flooding connectivity

  // adding the hello pkt_src to the routing table
  patch_update_local_rt_table(&report_src, &report_src,1,rssi,pkt->seq_id);

  // other nodes adding the master node hop to the routing table
  if (!linkaddr_cmp(&report_src, &pkt->src_master))
  {
    patch_update_local_rt_table(&pkt->src_master,&report_src,pkt->hop_count,rssi,pkt->seq_id);
  }
  // print the local_rt_table
  LOG_INFO("Local routing tabel is listed as follw: \r\n");
  //print_local_routing_table();
  
  // Forward the packet
  forward_hello(pkt);
  print_local_routing_table();
  // Reply the true source
  routing_report(&report_src, pkt->hop_count, rssi,pkt->seq_id);
  leds_single_off(LEDS_LED2);
}

static void DAO_PACKET_callback(const void *data, uint16_t len,
                           const linkaddr_t *src, const linkaddr_t *dest)
{
  leds_single_on(LEDS_LED2);
  LOG_INFO("Receiving RT_REPORT_PACEKT:\n");
  struct rt_entry_pkt *pkt = (struct rt_entry_pkt *)data;
  print_rt_entries_pkt(pkt);
  pkt->hop_count++;
  int8_t rssi = (int8_t)packetbuf_attr(PACKETBUF_ATTR_RSSI);
  // avoid unstalbe link hard decision
  if(rssi <= RSSSI_TH)
  {
    LOG_WARN("low RSSI DAO, rejected\n\r");
    return;
  }
  patch_update_local_rt_table(src,src,pkt->hop_count,rssi,pkt->seq_id);
  routing_report(&addr_master, pkt->hop_count, rssi,pkt->seq_id);
  LOG_INFO("Not the Master Node forwarding RT_REPORT_PACKET:\n");
  const linkaddr_t *next = get_next_hop_to(&addr_master,0);
  nullnet_buf = (uint8_t *)pkt;
  nullnet_len = sizeof(pkt);
  NETSTACK_NETWORK.output(next); 
}

static void SENSOR_PACKET_callback(const void *data, uint16_t len, 
                            const linkaddr_t *src, const linkaddr_t *dest){
  int8_t rssi = (int8_t)packetbuf_attr(PACKETBUF_ATTR_RSSI);
  //LOG_INFO("Hello Packet Process begin\r\n");
  if(rssi <= RSSSI_TH)
  {
    LOG_WARN("low RSSI DAO, rejected\n\r");
    return;
  }                    
	memcpy(&recv_message, (sensor_data*)data, sizeof(recv_message));
	if(recv_message.type == 3){
    uint16_t src = get_node_id_from_linkaddr(&recv_message.source);
		//LOG_INFO("Received data are from %d:\n\r", src);
		//LOG_INFO("batttery: [%d](mV)\n\r", recv_message.battery);
		//LOG_INFO("temperature: [%d](C)\n\r", recv_message.temperature);
		//LOG_INFO("light: [%d](lux)\n\r", recv_message.light_lux);
		//LOG_INFO("distance: [%d](cm)\n\r", recv_message.distance);
    battery[src] = (float)(recv_message.battery/3700);

    printf("Received data are from %d:\n\r", get_node_id_from_linkaddr(&recv_message.source));
		printf("batttery: [%d](mV):\n\r", recv_message.battery);
		printf("temperature: [%d](C):\n\r", recv_message.temperature);
		//printf("Node: %i SensorType: 1 Value: %d \n\r", src,recv_message.light_lux);
	  //printf("Node: %i SensorType: 2 Value: %d \n\r", src, recv_message.distance);
    printf("Node: %d SensorType: 1 Value: %d Battery: %d \n\r",
            src, recv_message.light_lux, recv_message.battery*100/3800);
    printf("Node: %d SensorType: 2 Value: %d Battery: %d \n\r",
            src, recv_message.distance, recv_message.battery*100/3800);


	}
}
 

static void ADVERTISE_PACKET_callback(const void *data, uint16_t len, 
                            const linkaddr_t *src, const linkaddr_t *dest) {
  if(len != sizeof(struct advertise_packet)) {
    LOG_WARN("Wrong packet size: %u\n", len);
    return;
  }

  struct advertise_packet *pkt = (struct advertise_packet *)data;
  int8_t rssi = (int8_t)packetbuf_attr(PACKETBUF_ATTR_RSSI);
  LOG_INFO("Geting ADVERTISE packet:\n");
  LOG_INFO("  Dest node:       %u\n", get_node_id_from_linkaddr(&(pkt->dest)));
  LOG_INFO("  CH node:    %u\n", get_node_id_from_linkaddr(&(pkt->advertise_ch)));
  LOG_INFO("  Seq ID:          %u\n", pkt->seq_id);
  //LOG_INFO("Hello Packet Process begin\r\n");
  if(rssi <= -75)
  {
    LOG_WARN("low RSSI DAO, rejected\n\r");
    return;
  }
  if(linkaddr_cmp(&(pkt->dest), &linkaddr_node_addr)) {
    // my dest 
    //linkaddr_cmp(&master_addr,&pkt->advertise_ch);
    //memb_init(&permanent_rt_mem);
    //list_init(permanent_rt_table);
    //rt_entry *e = memb_alloc(&permanent_rt_mem);
    /*if(e != NULL) {
      linkaddr_copy(&(e->dest), &addr_master);
      linkaddr_copy(&(e->next_hop), &(pkt->advertise_ch));
      e->tot_hop = pkt->tot_hop;
      e->metric = rssi;
      e->seq_no = 1;
      //list_add(permanent_rt_table, e);
      uint16_t dest_id = get_node_id_from_linkaddr(&(e->dest));
      uint16_t next_id = get_node_id_from_linkaddr(&(e->next_hop));
      LOG_INFO("+------------------+ Permanent Routing Table: +--------------------+\n");
      LOG_INFO("|No.0 | dest:%u | next:%u | tot_hop:%u | rssi:%d | tot_hop:%d |seq:%u |\n",
             dest_id, next_id,
             e->tot_hop, e->metric, e->tot_hop, e->seq_no);
      LOG_INFO("+------------------+ ------------------------ +--------------------+\n");

    }

  } else {
    // not my dest
    nullnet_buf = (uint8_t *)pkt;
    nullnet_len = sizeof(struct advertise_packet);
    NETSTACK_NETWORK.output((get_next_hop_to(&(pkt->dest),0)));
  }*/
}
                            }

static void HEARTBEAT_PACKET_callback(const void *data, uint16_t len, 
                            const linkaddr_t *src, const linkaddr_t *dest){
  heartbeat_packet* pkt = (heartbeat_packet*)data;
  linkaddr_t addr = pkt->src;
  int index = get_node_id_from_linkaddr(&addr);
  if(heart_record[index]>0){
    heart_record[index] --;;
  }
}


static int board_cast_rejoion = 0;

void NEWNODE_PACKET_callback(const void *data, uint16_t len,
                            const linkaddr_t *src, const linkaddr_t *dest)
{
  const newnode_packet *pkt = (const newnode_packet *)data;

  // Only process packets with the correct type
  if(node_id == MASTER_NODE_ID) {
  }
  else{
    if(linkaddr_cmp(&addr_master,NULL))
    {
      if(!board_cast_rejoion)
      {
        nullnet_buf = (uint8_t *)pkt;
        nullnet_len = sizeof(pkt);
        NETSTACK_NETWORK.output(NULL);
        board_cast_rejoion = 1;
      }
      else;
    }
    else{
      nullnet_buf = (uint8_t *)pkt;
      nullnet_len = sizeof(pkt);
      NETSTACK_NETWORK.output(get_next_hop_to(&addr_master, 0));
    }
  }
  
}

static void HELLO_Callback(const void *data, uint16_t len,
                           const linkaddr_t *src, const linkaddr_t *dest)
{
  leds_single_on(LEDS_LED2);
  uint8_t type = *((uint8_t *)data);
  LOG_INFO("<< Received packet, type = %d, len = %d\n", type, len);
  switch(type) {
    case HELLO_PACKET:
      DIO_PACKET_callback(data, len, src, dest);
      leds_single_off(LEDS_LED2);
      break;
    case RT_REPORT_PACKET:
      DAO_PACKET_callback(data, len, src, dest);
      leds_single_off(LEDS_LED2);
      break;
    case SENSOR_DATA_PACKET:
      SENSOR_PACKET_callback(data, len, src, dest);
      leds_single_off(LEDS_LED2);
      break;
    case ADVERTISE_PACKET:
      ADVERTISE_PACKET_callback(data, len, src, dest);
      leds_single_off(LEDS_LED2);
      break;
    case HEARTBEAT_PACKET:
      HEARTBEAT_PACKET_callback(data, len, src, dest);
      leds_single_off(LEDS_LED2);
      break;
    case NEWNODE_PACKET:
      NEWNODE_PACKET_callback(data, len, src, dest);
      leds_single_off(LEDS_LED2);
      break;

    default:
      LOG_WARN("Unknown packet type: %d\r\n", type);
  }
}



process_event_t rejoin_event;
static uint8_t hello_process_cnt = 0;

PROCESS(hello_process, "HELLO Flooding Process");
PROCESS(sensro_report_process, "Hello Dummy Process");
PROCESS(heart_beat_trans_process, "transmit heart beating signal");
PROCESS(heartbeat_hearing_process, "Master uses this process to monitor node lost");
// PROCESS(rejoin_process, "Rejoining the network");

AUTOSTART_PROCESSES(&hello_process, &sensro_report_process,
                      &heart_beat_trans_process, &heartbeat_hearing_process);

PROCESS_THREAD(hello_process, ev, data) {
  static struct etimer timer;
  PROCESS_BEGIN();
  LOG_INFO("HELLO PROCESS BEGIN\n");
  NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_CHANNEL,GROUP_CHANNEL);
  radio_value_t channel;
  NETSTACK_CONF_RADIO.get_value(RADIO_PARAM_CHANNEL, &channel);
  LOG_INFO("Radio channel set to %u\r\n", channel);
  if (NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_TXPOWER, TX_POWER) != RADIO_RESULT_OK) {
    LOG_WARN("Failed to set TX power to %d dBm\n", TX_POWER);
  } else {
    LOG_INFO("TX power set to %d dBm\n", TX_POWER);
  }

  for (int i = 0; i < MAX_NODES; i++) {
    for (int j = 0; j < MAX_NODES; j++) {
      adjacency_matrix[i][j] = (i == j) ? 255 : 0;
    }
  }
  memb_init(&rt_mem);
  list_init(local_rt_table);
  insert_entry_to_rt_table(&linkaddr_node_addr, &linkaddr_node_addr, 0, 0, 0);
  //memb_init(&permanent_rt_mem);
  //list_init(permanent_rt_table);
  nullnet_set_input_callback(HELLO_Callback);

  last_seq_id = 1;
  etimer_set(&timer, CLOCK_SECOND * 5);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
    hello_process_cnt++;
    LOG_INFO("hello process round%d\n\r",hello_process_cnt);
    if(hello_process_cnt > 2) {
      int guess_master_id = get_node_id_from_linkaddr(&addr_master);
      if(guess_master_id == 65535 && net_rejoin == 0)
      {
        LOG_WARN("Missed the hello process, integer rejion proceess\n\r");
        net_rejoin = 1;
        //process_post(&rejoin_process, rejoin_event, NULL);
      }
      if(net_rejoin == 1) {
        printf("BROADCAST LALALALALA\n");
        static newnode_packet pkt;
        pkt.type = NEWNODE_PACKET;
        linkaddr_copy(&(pkt.src), &linkaddr_node_addr);
        nullnet_buf = (uint8_t *)&pkt;
        nullnet_len = sizeof(pkt);
        NETSTACK_NETWORK.output(NULL);
        hello_process_cnt = 0;
        net_rejoin = 0;
      }
    }
    etimer_reset(&timer);
  }
  PROCESS_END();
}

PROCESS_THREAD(sensro_report_process, ev, data)
{
  static struct etimer sensor_reading_timer;
	static int light_raw, distance_raw;
	static int light_value, distance_value;
	static int voltage, temperature;
	static sensor_data packet;
  PROCESS_BEGIN();
  etimer_set(&sensor_reading_timer, CLOCK_SECOND*3);

  if(node_id == MASTER_NODE_ID) {
  }
  else{
    while(1) {
      
      PROCESS_WAIT_EVENT();
      // read raw data from ADC
      light_raw = saadc_sensor.value(P0_30);
      distance_raw = saadc_sensor.value(P0_31);
      
      // get sensor data
      light_value = get_light_lux(light_raw);
      distance_value = get_distance(distance_raw);
      voltage = get_millivolts(saadc_sensor.value(BATTERY_SENSOR));
      temperature = temperature_sensor.value(0)/4;

      distance_buffer[global_index] = distance_value;
      light_buffer[global_index] = light_value;
      temperature_buffer[global_index] = temperature;
      global_index ++;
      if(global_index>=BUFFER_SIZE){
        distance_av_cal();
        light_av_cal();
        temperture_av_cal();
        global_index = 0;
        trans_flag = 1;
      }
      if(ABS(distance_av_1-distance_av_0) >= DIS_THRES && ABS(light_av_1-light_av_0) >= LIGHT_THRES){
        trans_flag = 1;
      }
      // assign data to packet;
      if(trans_flag){
        packet.type = 3;
        linkaddr_copy(&packet.source, &linkaddr_node_addr);
        packet.light_lux = light_av_0;
        packet.distance = distance_av_0;
        packet.battery = voltage;
        packet.temperature = temperature;

        // transmit data to master
        const linkaddr_t *next_hop = get_next_hop_to(&addr_master,0);
        if(next_hop != NULL) {
          nullnet_buf = (uint8_t *)&packet;
          nullnet_len = sizeof(packet);
          NETSTACK_NETWORK.output(next_hop);
          LOG_INFO("Sent sensor data to master. Temp=%i, Distance=%i, Battery=%i, Light_Lux%i\n",
            packet.temperature,packet.distance,packet.battery,packet.light_lux );
        } else {
          LOG_WARN("No route to master!\n");
        }

        trans_flag = 0;
      }  
      etimer_reset(&sensor_reading_timer);
    }
   
  }
  

  PROCESS_END();
}

PROCESS_THREAD(heart_beat_trans_process, ev, data){
  PROCESS_BEGIN();
  static struct etimer et;

  etimer_set(&et, CLOCK_SECOND*5);
  heartbeat_packet heart = {
    .type = HEARTBEAT_PACKET,
    .src = linkaddr_node_addr
  };

  while (1)
  {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    nullnet_buf = (uint8_t*)&heart;
    nullnet_len = sizeof(heart);
    NETSTACK_NETWORK.output(NULL);
    etimer_reset(&et);
  }
  PROCESS_END();
}

// PROCESS_THREAD(rejoin_process, ev, data){
//   PROCESS_BEGIN();
//   static struct etimer timer;
//   etimer_set(&timer, CLOCK_SECOND*3);
//   while(1) {
//     PROCESS_WAIT_EVENT();
//     if(net_rejoin == 1) {
//         newnode_packet pkt;
//         pkt.type = NEWNODE_PACKET;
//         linkaddr_copy(&pkt.src, &linkaddr_node_addr);
//         nullnet_buf = (uint8_t *)&pkt;
//         nullnet_len = sizeof(pkt);
//         NETSTACK_NETWORK.output(NULL); 
//     }
//     etimer_reset(&timer);
//   }
//   PROCESS_END();
// }

PROCESS_THREAD(heartbeat_hearing_process, ev, data){
  PROCESS_BEGIN();
  static struct etimer et;
  etimer_set(&et, CLOCK_SECOND*5);
  while (1)
  {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    for(int i=0;i<MAX_NODES;i++){
      if(heart_record[i] >= TOLERANCE){
        memset(&heart_record, 0, MAX_NODES);
        Node_death = 1;
        break;
      }
      else{
        heart_record[i] ++ ;
      }
    }
    etimer_reset(&et);
  }
  PROCESS_END();
}