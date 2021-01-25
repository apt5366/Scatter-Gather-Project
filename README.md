# Scatter-Gather-Project
A virtual file managing system working along-side a cache to manage quick recovery of packets

This is a project created for my Systems Programming course at Penn State. 

The service creates a peer-to-peer system in which you place the data for your local filesystem on a bunch of external hosts (called Nodes). The ScatterGather service is a software 
simulation of an online storage system such as OneDrive or Box. It has a basic protocol by which you communicate with it through a series of messages sent across a network. 

Each file is broken down into nodes. These nodes are scattered thorughout the system in form of packets. Each packet has a well-defined structure of the follwoing form: 
1) Magic (4 bytes) – this is a special number placed in memory to detect when a packet has been corrupted. This value MUST be SG_MAGIC_VALUE (see sg_defs.h).
2) Sender Node ID (loc) (8 bytes) – this is an identifier for a sender of a packet. All non-zero values are valid.
3) Receiver Node ID (rem)  (8 bytes) – this is an identifier for a receiver of a packet. All non-zero values are valid.
4) Block ID (8 bytes) – this is an identifier for block being operated on. All non-zero values are valid.
5) Operation (4 bytes) – this is the kind of operation being attempted. See a SG_System_OP enumeration in sg_defs.h for information.
6) Sender sequence number (2 bytes) – this is a unique sequence number for the sender that should be always increasing (that is sent by the sender and incremented by one for each packet). For the purpose of validation in this assignment, you can just check to see if it is non-zero.
7) Receiver sequence number (2 bytes) – this is a unique sequence number for the receiver reflecting the number of packets sent from that sender to that receiver. For the purpose of validation in this assignment, you can just check to see if it is non-zero.
8) Data indicator (1 byte) – this indicates whether the packet contains a data block. 0 means that it does not, and 1 means that it does (you can assume this is always 0 or 1 for the unit test cases).
9) Data (SG_BLOCK_SIZE bytes) – this is the data block associated with the packet, if needed. This will be added to the packet of the data indicator=1 only.
10) Magic (4 bytes) – this is a special number placed in memory to detect when a packet has been corrupted. This value MUST be SG_MAGIC_VALUE (see sg_defs.h).

The application both sends and ereceives packets. Upon receiving the application also validates the packet's structure to know what data has been passed.

The cache implemented is an LRU (least recentl used) cache which works together with the scatter-gather drive.
