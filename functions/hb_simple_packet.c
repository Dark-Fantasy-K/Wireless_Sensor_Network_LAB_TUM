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
static linkaddr_t addr_master;
static short adjacency_matrix[MAX_NODES][MAX_NODES];  
static linkaddr_t node_index_to_addr[MAX_NODES]; 
static int num_known_nodes = 0;

// sensor data transmission
static uint8_t trans_flag;
static int distance_buffer[BUFFER_SIZE], light_buffer[BUFFER_SIZE], temperature_buffer[BUFFER_SIZE];
static int global_index;
static int distance_av_0, light_av_0, temperature_av_0;
static int distance_av_1, light_av_1, temperature_av_1;
static sensor_data recv_message;

// heart beatt
#define TOLERANCE 5
static volatile uint8_t Node_death;
static uint8_t heart_record[MAX_NODES];
// static volatile uint8_t queue_index[MAX_NODES];
typedef struct heartbeat_s
{
  pkt_type  type;
  linkaddr_t src;
}heartbeat_s;

LIST(local_rt_table);
MEMB(rt_mem,rt_entry,MAX_NODES);

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
  return ((uint16_t)addr->u8[LINKADDR_SIZE - 2] << 8) | addr->u8[LINKADDR_SIZE - 1];
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
  // 不存在，尝试添加
  if (num_known_nodes < MAX_NODES) {
    linkaddr_copy(&node_index_to_addr[num_known_nodes], addr);
    return num_known_nodes++;
  }
  // 超出最大节点数量
  return -1;
}

const linkaddr_t *get_next_hop_to(const linkaddr_t *dest)
{
  for (rt_entry *e = list_head(local_rt_table); e != NULL; e = e->next) {
    if (linkaddr_cmp(&e->dest, dest)) {
      return &e->next_hop;
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
  nullnet_len = sizeof(*pkt);
  NETSTACK_NETWORK.output(NULL);
}

void update_local_rt_table(const linkaddr_t *dst, const linkaddr_t *next,uint8_t tot_hop, int16_t metric,uint16_t seq_no)
{
  rt_entry *e = memb_alloc(&rt_mem);
  if(e != NULL) {
    linkaddr_copy(&e->dest, dst);
    linkaddr_copy(&e->next_hop, next);
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
      update_local_rt_table(dst,dst,tot_hop,metric,seq_no);
      LOG_INFO("Discover new nodes adding to the rt table:\n");
    }
    else if (abs(e->metric -metric) > 5)   
    {
      LOG_INFO("Update rt table with new metric:\n");
      e->metric =metric ;
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
  rt_entry *iter = list_head(local_rt_table);
  linkaddr_copy(&pkt.rt_src,     &iter->dest);
  for(; iter != NULL; iter = iter->next) {
    
    printf("  type: %d\n", iter->type);
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


    /*printf("Entry %d: dest=%i next_hop=%i hop=%d metric=%d\n",
      dest_id,
      next_id,
      pkt.tot_hop,
      pkt.metric);*/
    clock_wait(CLOCK_SECOND / 20);  // 等待 50ms
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
  //LOG_INFO("Hello Packet Process begin\r\n");
  leds_single_on(LEDS_LED2);
  // processing the hello packet info
  struct dio_packet *pkt = (struct dio_packet *)data;
  linkaddr_t report_src;
  linkaddr_copy(&addr_master, &pkt->src_master);
  linkaddr_copy(&report_src, &pkt->src);
  last_seq_id = pkt->seq_id;
  if(node_id == MASTER_NODE_ID) 
  {
    leds_single_off(LEDS_LED2);
    return;
  }

  // Avoid loops: if already seen, drop
  //if((pkt->seq_id <=last_seq_id) && !parent_is_in_rt_table(&report_src)){
  //  leds_single_off(LEDS_LED2);
  //  LOG_INFO("The Packet has been Processed\r\n");
  //  return;
  //}

  // processing the packet info
  int8_t rssi = (int8_t)packetbuf_attr(PACKETBUF_ATTR_RSSI);
  pkt->hop_count++;
  linkaddr_copy(&pkt->src, &linkaddr_node_addr);

  // initialization ip_table, and add flooding info
  // update_local_rt_table(master_node info + hello packet info);
  // flooding connectivity

  // adding the hello pkt_src to the routing table
  patch_update_local_rt_table(&report_src,&report_src,pkt->hop_count,rssi,pkt->seq_id);

  // other nodes adding the master node hop to the routing table
  if (!linkaddr_cmp(&report_src, &pkt->src_master))
  {
    patch_update_local_rt_table(&pkt->src_master,&report_src,pkt->hop_count,rssi,pkt->seq_id);
  }

  // print the local_rt_table
  LOG_INFO("Local routing tabel is listed as follw: \r\n");
  print_local_routing_table();
  // Forward the packet
  forward_hello(pkt);
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
  //struct dao_packet *pkt = (struct dao_packet *)data;
  //print_dao_table_entries(pkt);
  print_rt_entries_pkt(pkt);
  LOG_INFO("-------dao packet size-%u--------------",sizeof(struct rt_entry_pkt));
  
  int8_t rssi = (int8_t)packetbuf_attr(PACKETBUF_ATTR_RSSI);
  patch_update_local_rt_table(src,src,pkt->hop_count++,rssi,pkt->seq_id);


  //update_local_rt_table(src,src,pkt->hop_count++,rssi,pkt->seq_id);

  
  if(node_id == MASTER_NODE_ID)
  {  
    LOG_INFO("Master Node get RT_REPORT_PACKET:\n");
    int src_index = get_index_from_addr(&pkt->src);
    if (src_index == -1) 
    { 
      LOG_WARN("Can't find the correct idx\n");
      return;
    }
    // update the adjacency matrix
    int dst_index = get_index_from_addr(&pkt->rt_dest);
    if (src_index == dst_index) 
    {
      adjacency_matrix[src_index][dst_index] = 255;
    }
    else
    {
      adjacency_matrix[src_index][dst_index] = pkt->rt_metric;
      adjacency_matrix[dst_index][src_index] = pkt->rt_metric;
    }
    // go through the routing report rt_table, update the local rt table
    // note that next hop would be the packet src 
    patch_update_local_rt_table(&pkt->rt_dest,src,pkt->rt_tot_hop,pkt->rt_metric,pkt->rt_seq_no);
    print_local_routing_table();
    print_adjacency_matrix();
    leds_single_off(LEDS_LED2);
  }
  else
  {
    // update my own local routing table
    // since local routing table update, routing report
    routing_report(&addr_master, pkt->hop_count, rssi,pkt->seq_id);
    LOG_INFO("Not the Master Node forwarding RT_REPORT_PACKET:\n");
    const linkaddr_t *dest = get_next_hop_to(&addr_master);
    nullnet_buf = (uint8_t *)&pkt;
    nullnet_len = sizeof(pkt);
    //nullnet_len = offsetof(struct dao_packet, table) + pkt->no_entries * sizeof(rt_entry);
    NETSTACK_NETWORK.output(dest); 
  }


}

static void SENSOR_PACKET_callback(const void *data, uint16_t len, 
                            const linkaddr_t *src, const linkaddr_t *dest){

	memcpy(&recv_message, (sensor_data*)data, sizeof(recv_message));
	if(recv_message.type == 3){
		LOG_INFO("Received data are from %d:\n\r", get_node_id_from_linkaddr(&recv_message.source));
		LOG_INFO("batttery: [%d](mV)", recv_message.battery);
		LOG_INFO("temperature: [%d](C)", recv_message.temperature);
		LOG_INFO("light: [%d](lux)", recv_message.light_lux);
		LOG_INFO("distance: [%d](cm)", recv_message.distance);
	}
}

int from_linkaddr_to_index(linkaddr_t addr){
  return 0;
}

void heartbeat_hearing(const void* data){
  heartbeat_s* pointer = (heartbeat_s*)data;
  linkaddr_t addr = pointer->src;
  int index = from_linkaddr_to_index(addr);
  if(heart_record[index]>0){
    heart_record[index] --;;
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
    case PKT_HEART_BEAT+1:
      heartbeat_hearing(data);
    default:
      LOG_WARN("Unknown packet type: %d\r\n", type);
     
  }
}



PROCESS(hello_process, "HELLO Flooding Process");
PROCESS(sensro_report_process, "Hello Dummy Process");
PROCESS(choose_ch_process, "choosing CH Process");
PROCESS(heart_beat_trans_process, "transmit heart beating signal");
PROCESS(heartbeat_hearing_process, "Master uses this process to monitor node lost");

AUTOSTART_PROCESSES(&hello_process, &sensro_report_process,&choose_ch_process, &heart_beat_trans_process, &heartbeat_hearing_process);
PROCESS_THREAD(hello_process, ev, data) {
  PROCESS_BEGIN();
  LOG_INFO("HELLP PROCESS BEGIN\n");
  NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_CHANNEL,GROUP_CHANNEL);
  radio_value_t channel;
  NETSTACK_CONF_RADIO.get_value(RADIO_PARAM_CHANNEL, &channel);
  LOG_INFO("Radio channel set to %u\r\n", channel);
  get_index_from_addr(&linkaddr_node_addr);
  //initiation the adjacency_matrix
  for (int i = 0; i < MAX_NODES; i++)
  {
    for (int j = 0; j < MAX_NODES; j++)
    {
      if (i==j)
      {
        adjacency_matrix[i][j] = 255;  
      }
      else
      {
        adjacency_matrix[i][j] = 0;  
      }
    }
  }
  memb_init(&rt_mem);
  list_init(local_rt_table);
  update_local_rt_table(&linkaddr_node_addr, &linkaddr_node_addr,0,0,0);
  
  nullnet_set_input_callback(HELLO_Callback);
  if(node_id == MASTER_NODE_ID) 
  {
    static struct dio_packet my_hello_pkt;
    static struct etimer timer;
    // periodically send hello packet
    etimer_set(&timer, CLOCK_SECOND * HELLO_INTERVAL);
    while(1) {
      last_seq_id = HELLO_SEQ_ID;
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
      leds_single_on(LEDS_LED1);
      my_hello_pkt.type = HELLO_PACKET;
      linkaddr_copy(&my_hello_pkt.src, &linkaddr_node_addr);
      linkaddr_copy(&my_hello_pkt.src_master, &linkaddr_node_addr);
      my_hello_pkt.hop_count = 0;
      my_hello_pkt.seq_id = HELLO_SEQ_ID;
      forward_hello(&my_hello_pkt);
      LOG_INFO("MASTER broadcasted HELLO\r\n");
      leds_single_off(LEDS_LED1);
      etimer_reset(&timer);
    }
  }
  else{
    
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
        const linkaddr_t *next_hop = get_next_hop_to(&addr_master);
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
const float battery[MASTER_NODE_ID] = {0.8f,0.9f, 0.7f, 1.0f, 0.9f, 0.8f, 0.6f, 0.8f,0.3f,0.6f};
PROCESS_THREAD(choose_ch_process, ev, data){
	static struct etimer choose_timer;
  PROCESS_BEGIN();
  etimer_set(&choose_timer, CLOCK_SECOND*3);
	while(1){
		PROCESS_WAIT_EVENT();
		// code to test head chosen algorithm
		/*const short rssi[] = {
			0,-60,0,0,-30,-40,0,0,
			-60,0,0,-50,-30,0,-73,0,
			0,0,0,0,-50,-50,0,-50,
			0,-50,0,0,0,-40,-47,0,
			-30,-30,-50,0,0,0,-30,-47,
			-40,0,-50,-30,0,0,0,-45,
			0,-73,0,-47,-39,0,0,0,
			0,0,-50,0,-47,-45,0,0};*/

    unsigned char head_list[3] ={0};
    short* rssi = (short*)adjacency_matrix;
		unsigned char link_table[MAX_NODES][MAX_NODES] = {0};
		from_rssi_to_link(rssi, battery, MAX_NODES, (uint8_t*)link_table,head_list);
		for (int i=0;i<MAX_NODES;i++) {
			for (int j=0;j<MAX_NODES;j++) {
				printf("%d ",link_table[i][j]);
			}
			printf("\n");
		}
		printf("===================================================\n\r");
		etimer_reset(&choose_timer);
		// route_ready = READY;

	}

	PROCESS_END();
}


/*
Heart beat processes,
There is a global variable Heart_Record,
callback function, heartbeat_hearing
sturcture, heartbeat_s
*/


PROCESS_THREAD(heart_beat_trans_process, ev, data){
  PROCESS_BEGIN();
  static struct etimer et;

  etimer_set(&et, CLOCK_SECOND*5);
  heartbeat_s heart = {
    .type = PKT_HEART_BEAT,
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
      }else{
        heart_record[i] ++ ;
      }
    }
    etimer_reset(&et);
  }
  PROCESS_END();
}