/**
 * @file    my_fucntions.h
 * @brief   header file of vital functions
 * @details declare functions and assign important variable
 * @author  Chi Xia
***/

#ifndef MY_FUNCTIONS_H
#define MY_FUNCTIONS_H

#include<string.h>
#include<stdio.h>

#define RSSI_LIMITATION (-60)

typedef struct head_sub_info
{
    unsigned char   pkt_type;
    unsigned char   head_info[3];
    unsigned char   connect_info[3*10];
    unsigned char   dim;
    /* data */
}head_sub;

/// pre-define possible combinations of chosen heads, only consider 2 and 3 heads
static const unsigned char combination2[10][2] = {{0, 1},{0, 2},{0, 3},{0, 4},{1, 2},{1, 3},{1,4},{2,3},{2,4},{3,4}};
static const unsigned char combination3[10][3] = {{0,1,2},{0,1,3},{0,1,4},{0,2,3},{0,2,4},{0,3,4},{1,2,3},{1,2,4},{1,3,4},{2,3,4}};

void matrix_multiply(const unsigned char* matrix_A, const unsigned char* matrix_B, const unsigned char dim, unsigned char* matrix_out);
void hop_matrix(const unsigned char* template, unsigned char* target, const unsigned char dim, const unsigned char hop_time);
void ordering(const unsigned char* matrix, unsigned char* const ordered, const unsigned char dim, const float* battery, const float* rssi_criteria);
unsigned char if_connect_master(const unsigned char* master_matrix, const unsigned char dim, const unsigned char* candidate_cluster, const unsigned char num);
unsigned char if_connect_each(const unsigned char* template, const unsigned char dim, const unsigned char* candidate_cluster, const unsigned char num);
void cluster_head_choose(const unsigned char* hop_template, const unsigned num_cluster, const unsigned char dim, const unsigned char* master, const float* battery, const short* rssi_matrix, unsigned char* const res);
float num_allocated(const unsigned char* allocated, const unsigned char dim);
int greatest_value_index(const float* matrix, const int length);
void group_selection(const unsigned char num_cluster, const unsigned char* cluster, const unsigned char dim, const unsigned char* hop_template, const unsigned char* master, const float* battery, unsigned char* const res);
void from_D2matrix_to_D1matrix(const unsigned char* D2matrix, const unsigned char D2dim, unsigned char* const D1matrix);
void rssi_to_adjacent(const signed short* rssi_matrix, unsigned char* adjacent, const unsigned char dim);
void print_link_stage(const unsigned char* head, const unsigned char num_head, const unsigned char* head_sub_node, const unsigned char dim, const unsigned char* adjacent, const unsigned char* master, const float* battery, unsigned char* const res);
void death_printer(const unsigned char* adjacent, const unsigned char dim);
void matrix_printer(const unsigned char* const matrix, const unsigned char dim);
unsigned char value_regularization(unsigned char data, const unsigned char hop_time);
void extract_matrix(const unsigned char* org_adjacent, const unsigned char dim, unsigned char* res_adjacent, unsigned char* master);
void from_rssi_to_link(const short* rssi, const float* battery, const unsigned char dim, unsigned char* link_table, unsigned char* head_list);

#endif
