# LEACH based wireless sensor network project
## Overview  
This repository provides a C-based implementation for experimenting with **Wireless Sensor Networks (WSNs)**.  

It extends the **LEACH (Low-Energy Adaptive Clustering Hierarchy)** protocol by incorporating **energy awareness and re-joining mechanisms**. In particular, the project builds a hierarchical network structure where cluster-heads are selected based on **current battery level** and **link quality**, ensuring more efficient and reliable network operation. Also, a QT-programm is installed in the master node as the dashboard of the project.   

This project builds upon and extends the repositories and tools developed by the **TUM LSN (Chair of Communication Networks, TUM)** group.  

## Contents  
- `simple_packet.c` – Basic packet structures and utilities.  
- `my_functions.c` – Core helper functions for node communication and networking.  
- `master.c` – Main simulation control and integration of protocol logic.  

## Getting Started  

### Prerequisites  
- GCC (or any standard C compiler)  
- Make (optional, if you set up a Makefile)  

### Compilation  
You can compile the project with:  
```bash
gcc master.c my_functions.c simple_packet.c -o wsn_sim




