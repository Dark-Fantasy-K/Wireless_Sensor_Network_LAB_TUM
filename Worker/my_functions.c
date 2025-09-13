/**
 * @file    my_functions.c
 * @brief   realizations of additional used fucntions
 * @author  Chi Xia
 */

#include "my_functions.h"
#define DEBUG 0

void matrix_printer(const unsigned char* const matrix, const unsigned char dim) {
    for (int i = 0; i < dim; i++) {
        for (int j = 0; j < dim; j++) {
            printf("%03d ", matrix[i * dim + j]);
        }
        printf("\n");
    }
    printf("-----------------------------------\n\n");
}

unsigned char value_regularization(unsigned char data, const unsigned char hop_time) {
    const float value = (float)data/(float)hop_time;
    if (value == 0) return 0;
    if (value < 2) return 1;
    return (unsigned char)value;
}

void matrix_multiply(const unsigned char* matrix_A, const unsigned char* matrix_B, const unsigned char dim, unsigned char* matrix_out){
    unsigned char temp_matrix[dim*dim];
    // initialize matrx
    for(int i=0; i<dim; i++){
        for(int j=0; j<dim; j++){
            temp_matrix[i*dim+j] = 0;
        }
    }
    for(int i=0;i<dim;i++){
        for(int j=0;j<dim;j++){
            for(int k=0; k<dim; k++){
                temp_matrix[i*dim + j] += (matrix_A[i*dim+k] * matrix_B[k*dim+j]);
            }
        }
    }
    // assign value
    for(int i=0; i<dim; i++){
        for(int j=0; j<dim; j++){
            matrix_out[i*dim+j] = temp_matrix[i*dim+j];
        }
    }
}
void hop_matrix(const unsigned char* template, unsigned char* target, const unsigned char dim, const unsigned char hop_time){
    if(hop_time == 1){
        for(int i=0; i<dim; i++){
            for(int j=0; j<dim; j++){
                target[i*dim + j] = template[i*dim + j];
            }
        }
        return;
    }
    //calculate matrix based on hop_time
    const unsigned char* matrixA = template;
    const unsigned char* matrixB = template;
    for(int i=1;i<hop_time;i++){
        matrix_multiply(matrixA, matrixB, dim, target);
        matrixA = target;
    }
}

//static unsigned char combination2[10][2] = {{0, 1},{0, 2},{0, 3},{0, 4},{1, 2},{1, 3},{1,4},{2,3},{2,4},{3,4}};
//static unsigned char combination3[10][3] = {{0,1,2},{0,1,3},{0,1,4},{0,2,3},{0,2,4},{0,3,4},{1,2,3},{1,2,4},{1,3,4},{2,3,4}};

void ordering(const unsigned char* matrix, unsigned char* const ordered, const unsigned char dim, const float* battery, const float* rssi_criteria) {
    float operating_matrix[dim][2];
    for (int i = 0; i < dim; i++) {
        operating_matrix[i][0] = 0;
        operating_matrix[i][1] = (float)i;
        for (int j = 0; j < dim; j++) {
            if (i==j)
                operating_matrix[i][0] += 0;
            else
                operating_matrix[i][0] += (float)matrix[i*dim + j];
        }
        operating_matrix[i][0] *= battery[i] * rssi_criteria[i];
    }
    //for (int i=0; i<dim; i++) printf("%d", operating_matrix[i][0]);
    for (int i = 0; i < dim - 1; i++) {
        int swapped = 0;
        for (int j = 0; j < dim - 1 - i; j++) {
            if (operating_matrix[j][0] < operating_matrix[j + 1][0]) {
                float temp = operating_matrix[j][0];
                operating_matrix[j][0] = operating_matrix[j + 1][0];
                operating_matrix[j + 1][0] = temp;
                temp = operating_matrix[j][1];
                operating_matrix[j][1] = operating_matrix[j + 1][1];
                operating_matrix[j + 1][1] = temp;
                swapped = 1;
            }
        }
        if (!swapped) break;
    }
    for (int i = 0; i < dim; i++) {
        ordered[i] = (unsigned char)operating_matrix[i][1];
    }
}

unsigned char if_connect_master(const unsigned char* master_matrix, const unsigned char dim, const unsigned char* candidate_cluster, const unsigned char num) {
    unsigned char flag = 0;
    for(int i=0; i<num; i++) {
        if (candidate_cluster[i]<dim) {
            if (master_matrix[candidate_cluster[i]] == 1) {
                flag ++;
            }
        }
    }
    return flag;
}

unsigned char if_connect_each(const unsigned char* template, const unsigned char dim, const unsigned char* candidate_cluster, const unsigned char num) {
    const unsigned char candidate1 = candidate_cluster[0];
    const unsigned char candidate2 = candidate_cluster[1];
    const unsigned char candidate3 = candidate_cluster[2];
    unsigned char flag = 0;
    if (template[candidate1*dim + candidate2] == 1)flag ++;
    if (template[candidate2*dim + candidate3] == 1)flag ++;
    if (template[candidate3*dim + candidate1] == 1)flag ++;

    return flag;
}

void cluster_head_choose(const unsigned char* hop_template, const unsigned num_cluster, const unsigned char dim, const unsigned char* master, const float* battery, const short* rssi_matrix, unsigned char* const res) {
    unsigned char matrix_hop1[dim*dim], matrix_hop2[dim*dim], matrix_hop3[dim*dim];
    // hop1
    hop_matrix(hop_template, matrix_hop1, dim, 1);
    // hop2
    hop_matrix(hop_template, matrix_hop2, dim, 2);
    // hop3
    hop_matrix(hop_template, matrix_hop3, dim, 3);

    float rssi_criteria[dim];
    memset(rssi_criteria, 0, dim*sizeof(float));

    for(int i=0; i<dim; i++) {
        for(int j=0; j<dim; j++) {
            if (rssi_matrix[i*dim + j] == 255 || rssi_matrix[i*dim + j] == 0) {
                rssi_criteria[i] += -100;
            }else {
                rssi_criteria[i] += (float)rssi_matrix[i*dim + j];
            }
        }
        rssi_criteria[i] = (rssi_criteria[i]+100*(float)dim)/150;
    }
    /*
    matrix_printer(matrix_hop1, dim);
    matrix_printer(matrix_hop2, dim);
    matrix_printer(matrix_hop3, dim);
    */

    unsigned char final_value_matrix[dim*dim];
    memset(&final_value_matrix, 0, dim*dim*sizeof(unsigned char));
    for (int i=0;i<dim; i++) {
        for (int j=0;j<dim;j++) {
            const unsigned char weight[3] = {8, 4, 2};
            final_value_matrix[i*dim + j] = weight[0]*value_regularization(matrix_hop1[i*dim + j], 1) + weight[1]*value_regularization(matrix_hop2[i*dim + j],2 ) + weight[2]*value_regularization(matrix_hop3[i*dim + j], 3);
        }
    }
    unsigned char ordered[dim];
    ordering(final_value_matrix, ordered, dim, battery, rssi_criteria);
    unsigned char combination[10][num_cluster];
    for(int i=0; i<10; i++) {
        for(int j=0; j<num_cluster; j++) {
            combination[i][j] = ordered[combination3[i][j]];
        }
    }

    unsigned char chosen_cluster[num_cluster];
    unsigned char to_master_matrix[10] = {0};
    // only consider choosing 3 clusters
    for (int i=0;i<10;i++) {
        to_master_matrix[i] = if_connect_master(master, dim, combination[i], num_cluster);
    }

    unsigned char to_each_matrix[10] = {0};
    for (int i=0;i<10;i++) {
        to_each_matrix[i] = if_connect_each(hop_template, dim, combination[i], num_cluster);
    }

    unsigned char to_chosen_matrix[10] = {0};
    for (int i=0;i<10;i++) {
        to_chosen_matrix[i] = to_master_matrix[i] + to_each_matrix[i];
    }
    unsigned char target_grade = 6; //3+3 is the maximum value, means 3 nodes can connect to master as well as each
    while (target_grade) {
        for (int i=0;i<10;i++) {
            if (to_chosen_matrix[i] >= target_grade && to_master_matrix[i] >= 1) {
                for (int j=0;j<num_cluster;j++) {
                    chosen_cluster[j] = combination[i][j];
                }
                target_grade = 0;
                break;
            }
        }
        if (target_grade == 0) break;
        target_grade --;
        if (target_grade == 0) {
            for (int j=0;j<num_cluster;j++) {
                chosen_cluster[j] = combination[0][j];
            }
        }
    }
    for (int i=0;i<num_cluster;i++) {
        res[i] = chosen_cluster[i];
    }
}

float num_allocated(const unsigned char* allocated, const unsigned char dim) {
    float num = 0;
    for(int i=0; i<dim; i++) {
        num += (float)allocated[i];
    }
    return num;
}

int greatest_value_index(const float* matrix, const int length) {
    int index = 0;
    float temp = matrix[0];
    for(int i=1; i<length; i++) {
        if (matrix[i] >= temp) {
            temp = matrix[i];
            index = i;
        }
    }
    return index;
}

void group_selection(const unsigned char num_cluster, const unsigned char* cluster, const unsigned char dim, const unsigned char* hop_template, const unsigned char* master, const float* battery, unsigned char* const res_sub) {
    unsigned char matrix_hop1[dim*dim];
    unsigned char connection_summary[dim];
    memset(connection_summary, 0, dim*sizeof(unsigned char));
    unsigned char node_allocation[num_cluster][dim];
    memset(node_allocation, 0, num_cluster*dim*sizeof(unsigned char));

    // calculate the number of hops mainly for cluster's head
    unsigned char all_hop_template[(dim+1)*(dim+1)];
    memset(all_hop_template, 0, (dim+1)*(dim+1)*sizeof(unsigned char));
    for(int i=0; i<dim+1; i++) {
        for(int j=0; j<dim+1; j++) {
            if (i == 0) {
                if (j == 0) {
                    all_hop_template[i*(dim+1) + j] = 0;
                }else {
                    all_hop_template[i*(dim+1) + j] = master[j-1];
                }
            }else {
                if (j == 0) {
                    all_hop_template[i*(dim+1) + j] = master[i-1];
                }else {
                    all_hop_template[i*(dim+1) + j] = hop_template[(i-1)*dim + j-1];
                }
            }
        }
    }

    unsigned char head_2_master_cost[num_cluster];
    memset(head_2_master_cost, 0, num_cluster*sizeof(unsigned char));
    unsigned char all_hop_matrix[(dim+1)*(dim+1)];
    memset(all_hop_matrix, 0, num_cluster*sizeof(unsigned char));
    unsigned char flag = 1, hop_num = 1, max_hop = dim;
    while (flag) {
        flag = 0;
        hop_matrix(all_hop_template, all_hop_matrix, dim+1, hop_num);
        for (int i=0; i<num_cluster; i++) {
            if (all_hop_matrix[cluster[i]+1] > 0 && head_2_master_cost[i] == 0) {
                head_2_master_cost[i] = hop_num;
            }
        }
        for (int i=0; i<num_cluster; i++) {
            if (head_2_master_cost[i] == 0) {
                flag = 1;
            }
        }
        hop_num ++;
        if (hop_num > max_hop) break;
    }

    // hop1
    hop_matrix(hop_template, matrix_hop1, dim, 1);
    for(int i=0; i<dim; i++) {
        for(int j=0; j<num_cluster; j++) {
            connection_summary[i] += matrix_hop1[i + dim*cluster[j]];
        }
    }

    unsigned if_head[dim];
    memset(if_head, 0, dim*sizeof(unsigned));
    for(int i=0; i<dim; i++) {
        for (int j=0; j<num_cluster; j++) {
            if (i == cluster[j]) {
                if_head[i] = 1;
                break;
            }
        }
    }

    for(int k=1; k<=num_cluster; k++) {
        for(int i=0; i<dim; i++) {
            float head_value[num_cluster];
            memset(head_value, 0, sizeof(float)*num_cluster);
            for (int j=0; j<num_cluster; j++) {
                if (matrix_hop1[i + dim*cluster[j]]*k == connection_summary[i] && connection_summary[i] != 0 && if_head[i] == 0) {
                    head_value[j] = battery[cluster[j]]/(num_allocated(node_allocation[j], dim)+1)/(float)head_2_master_cost[j];
                }
            }
            if (connection_summary[i] == k && if_head[i] == 0) {
                node_allocation[greatest_value_index(head_value, num_cluster)][i] = 1;
            }
        }
    }
    // assign value to res
    for(int i=0; i<num_cluster; i++) {
        for(int j=0; j<dim; j++) {
            res_sub[i*dim + j] = node_allocation[i][j];
            for (int k=0; k<num_cluster; k++) {
                if (cluster[k] == j) {
                    res_sub[i*dim + j] = 0;
                }
            }
        }
    }
}

void from_D2matrix_to_D1matrix(const unsigned char* D2matrix, const unsigned char D2dim, unsigned char* const D1matrix) {
    for (int i = 0; i < D2dim; i++) {
        for (int j = 0; j < D2dim; j++) {
            D1matrix[i * D2dim + j] = D2matrix[i * D2dim + j];
        }
    }
}

void rssi_to_adjacent(const signed short* rssi_matrix, unsigned char* adjacent, const unsigned char dim){
    for (int i=0; i<dim; i++) {
        for (int j=0; j<dim; j++) {
            if (rssi_matrix[i*dim+j] != 255 && rssi_matrix[i*dim+j] >= -75 && rssi_matrix[i*dim+j] != 0) {
                adjacent[i*dim+j] = 1;
            }else {
                adjacent[i*dim+j] = 0;
            }
        }
    }
}

// new functions, mainly used for print message to be used by GUI

void print_link_stage(const unsigned char* head, const unsigned char num_head, const unsigned char* head_sub_node, const unsigned char dim, const unsigned char* adjacent, const unsigned char* master, const float* battery, unsigned char* const res) {
    unsigned char hop1[dim*dim];
    hop_matrix(adjacent, hop1, dim, 1);
    unsigned char connection_summary[dim];
    memset(connection_summary, 0, dim*sizeof(unsigned char));
    unsigned char can_2_master[dim];
    memset(can_2_master, 0, dim*sizeof(unsigned char));
    for (int i=0; i<dim; i++) {
        for (int j=0; j<num_head; j++) {
            connection_summary[i] += head_sub_node[i + dim*j];
        }
    }
    printf("REORGANIZATION\r\n");
    printf("ClusterHead: ");
    for (int i=0;i<3;i++) {
        printf("%d ",head[i]);
    }
    printf("\r\n");
    // first sent head id
    for (int i=0; i<num_head; i++) {
        if (master[head[i]] == 1) {
            printf("Newlink %d -> 255\r\n", head[i]);
            can_2_master[head[i]] = 1;
            //res[head[i]+1+0*(dim+1)] = 1;
            res[(head[i]+1)*(dim+1)+0] = 1;
        }
        else {
            float battery_temp = 0, hop_weight;
            unsigned char next = 254;
            for (int j=0; j<num_head; j++) {
                if (hop1[head[i]*dim+head[j]] != 0 && i != j) {
                    if (master[head[j]] == 1) hop_weight = 2;
                    else hop_weight = 0.5f;
                    if (battery[head[j]]  * hop_weight > battery_temp) {
                        battery_temp = battery[head[j]] * hop_weight;
                        next = head[j];
                    }
                }
            }
            if (next != 254) {
                printf("Newlink %d -> %d\r\n", head[i], next);
                can_2_master[head[i]] = 1;
                res[(head[i]+1)*(dim+1)+next+1] = 1;
                //res[(next+1)*(dim+1)+head[i]+1] = 1;
            }
        }
    }
    // second sent sub-node id
    for (int i=0; i<num_head; i++) {
        for (int j=0; j<dim; j++) {
            if (head_sub_node[i*dim + j] != 0) {
                printf("Newlink %d -> %d\r\n",  j, head[i]);
                can_2_master[j] = 1;
                res[(j+1)*(dim+1)+head[i]+1] = 1;
                //res[(head[i]+1)*(dim+1)+j+1] = 1;
            }
        }
    }
    unsigned char unconnected_index[dim][2], index=0, flag=0;
    memset(unconnected_index, 255, dim*2*sizeof(unsigned char));
    for (int i=0; i<dim; i++) {
        if(connection_summary[i] == 0){
            flag = 1;
            for (int j=0; j<num_head; j++) {
                if(head[j] == i) flag = 0;
            }
            if (flag) {
                // printf("i and head, %d, %d\n\r", i, head[j]);
                unconnected_index[index][0] = i;
                index += 1;
            }
        }
    }
    // allocate rest nodes
    for (int i=0; i<dim; i++) {
        if (unconnected_index[i][0] != 255) {
            float temp_criteria = 0, temp_weight;
            for (int j=0; j<dim; j++) {
                if (hop1[unconnected_index[i][0]*dim + j] != 0) {
                    if (can_2_master[j] == 1) {
                        temp_weight = 2;
                    }else {
                        temp_weight = 0.5f;
                    }
                    if (temp_criteria < battery[j] * temp_weight) {
                        temp_criteria = battery[j] * temp_weight;
                        unconnected_index[i][1] = j;
                        can_2_master[unconnected_index[i][0]] = 1;
                    }
                }
            }
        }
    }
    // third sent rest-node id
    for (int i=0; i<dim; i++) {
        if (unconnected_index[i][0] != 255 && unconnected_index[i][1] != 255) {
            //printf("------%d\n\r", unconnected_index[i][0]);
            printf("Newlink %d -> %d\r\n", unconnected_index[i][0], unconnected_index[i][1]);
            res[(unconnected_index[i][0]+1)*(dim+1)+unconnected_index[i][1]+1] = 1;
            //res[(unconnected_index[i][1]+1)*(dim+1)+unconnected_index[i][0]+1] = 1;
        }
    }
}

void death_printer(const unsigned char* adjacent, const unsigned char dim) {
    unsigned char connect_summary[dim];
    memset(connect_summary, 0, dim*sizeof(char));
    for (int i=0; i<dim; i++) {
        for (int j=0; j<dim; j++) {
            connect_summary[i] += adjacent[i*dim+j];
        }
    }
    for (int i=0; i<dim; i++) {
        if (connect_summary[i] == 0) {
            printf("LinkLost: %d\r\n", i);
        }
    }
}

void extract_matrix(const unsigned char* org_adjacent, const unsigned char dim, unsigned char* res_adjacent, unsigned char* master) {
    for (int i=1; i<dim; i++) {
        master[i-1] = org_adjacent[i];
    }
    for (int i=1; i<dim; i++) {
        for (int j=1; j<dim; j++) {
            res_adjacent[(i-1)*(dim-1)+(j-1)] = org_adjacent[i*dim+j];
        }
    }
}

void from_rssi_to_link(const short* rssi, const float* battery, const unsigned char dim, unsigned char* link_table, unsigned char* head_list) {
    // data transform
    const unsigned char low_dim = dim-1;
    unsigned char temp_adjacent[dim*dim];
    unsigned char adjacent[low_dim*low_dim];
    unsigned char master[low_dim];
    short used_rssi[low_dim*low_dim];
    memset(adjacent, 0, low_dim*low_dim*sizeof(unsigned char));
    memset(master, 0, low_dim*sizeof(unsigned char));

    rssi_to_adjacent(rssi, temp_adjacent, dim);
#if DEBUG
    printf("input rssi: \n");
    for (int i=0; i<dim; i++) {
        for (int j=0; j<dim; j++) {
            printf("%d ", rssi[i*dim+j]);
        }
        printf("\n");
    }
    printf("--------------------------------\n");
    printf("0_1 adjacent matrix: \n");
    for (int i=0; i<dim; i++) {
        for (int j=0; j<dim; j++) {
            printf("%d ", temp_adjacent[i*dim+j]);
        }
        printf("\n");
    }
    printf("--------------------------------\n");
#endif

    extract_matrix(temp_adjacent, dim, adjacent, master);
    death_printer(adjacent, low_dim);
#if DEBUG
    printf("extracted adjacent matrix: \n");
    for (int i=0; i<low_dim; i++) {
        for (int j=0; j<low_dim; j++) {
            printf("%d ", adjacent[i*low_dim+j]);
        }
        printf("\n");
    }
    printf("--------------------------------\n");
    printf("extracted master_connection array: \n");
    for (int i=0; i<low_dim; i++) {
        printf("%d ", master[i]);
    }
    printf("\n");
    printf("--------------------------------\n");
#endif
    // printf("extracted rssi matrix: \n");
    for (int i=1; i<dim; i++) {
        for (int j=1; j<dim; j++) {
            used_rssi[(i-1)*low_dim+j-1] = rssi[i*dim+j];
            // printf("%d ", used_rssi[(i-1)*low_dim+j-1]);
        }
        // printf("\n");
    }
    // printf("-------------------------------\n");
    // select head
    //unsigned char temp_head_index[3]={0};

    cluster_head_choose(adjacent, 3, low_dim, master, battery, used_rssi, head_list);
    unsigned char temp_head_allocate_node[3*low_dim];
    memset(temp_head_allocate_node, 0, 3*low_dim*sizeof(unsigned char));
    // allocate groups
    group_selection(3, head_list, low_dim, adjacent, master, battery, temp_head_allocate_node);
    print_link_stage(head_list, 3, temp_head_allocate_node, low_dim, adjacent, master, battery, link_table);
#if DEBUG
    printf("0_1 routing matrix: \n");
    for (int i=0; i<dim; i++) {
        for (int j=0; j<dim; j++) {
            printf("%d ", link_table[i*dim+j]);
        }
        printf("\n");
    }
    printf("--------------------------------\n");
#endif
}