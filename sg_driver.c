////////////////////////////////////////////////////////////////////////////////
//
//  File           : sg_driver.c
//  Description    : This file contains the driver code to be developed by
//                   the students of the 311 class.  See assignment details
//                   for additional information.
//
//   Author        : Ayush Tiwari 
//   Last Modified : 
//

// Include Files
#include <string.h>
#include <assert.h>

// Project Includes
#include <sg_driver.h>
#include <sg_service.h>
#include <sg_cache.h>
#include <string.h>

// Defines

//
// Global Data

// Driver file entry

// Global data
int sgDriverInitialized = 0; // The flag indicating the driver initialized
SG_Block_ID sgLocalNodeId;   // The local node identifier
SG_SeqNum sgLocalSeqno;      // The local sequence number
uint32_t Magic=  0x0000fefe; //for serialize and deserialize

typedef struct{
        //int isOpen=0; // to check if the file is open ::: 0 --> closed, 1 --> Open

        int isOpen; // to check if the file is open ::: 0 --> closed, 1 --> Open
        char* filename; //contains the path of the file
        int size; // has the size of the file
        int filePointer; //tells usthe postion where we are at
        SG_Node_ID nodes[20]; // will store the node IDs for the blocks of this file 
        SG_Block_ID blocks[20]; // will store the block IDs for the blocks of this file  
        SgFHandle fileHandle;
}File;

File file[20];// this array of structs stores all the files // the indexes here correspond to the filehandles

int fileHandleCounter=0; // This variable will be incremented whenever a new file is created 





// Driver support functions
int sgInitEndpoint( void ); // Initialize the endpoint

//
// Functions

//
// File system interface implementation

////////////////////////////////////////////////////////////////////////////////
//
// Function     : sgopen
// Description  : Open the file for for reading and writing
//
// Inputs       : path - the path/filename of the file to be read
// Outputs      : file handle if successful test, -1 if failure

SgFHandle sgopen(const char *path) {

    // First check to see if we have been initialized
    if (!sgDriverInitialized) {

        // Call the endpoint initialization 
        if ( sgInitEndpoint() ) {
            logMessage( LOG_ERROR_LEVEL, "sgopen: Scatter/Gather endpoint initialization failed." );
            return( -1 );
        }

        // Set to initialized
        sgDriverInitialized = 1;
    }

// FILL IN THE REST OF THE CODE	
	

    	// so the file handles go frm 0 to 19 -->> for 20 possible files
        int curFile=fileHandleCounter;

        file[curFile].isOpen=1;
        file[curFile].filename= path;
        file[curFile].size=0;
        file[curFile].filePointer=0;

        for(int i=0;i<20;i++)
                file[curFile].nodes[i]=0;

        for(int i=0;i<20;i++)
                file[curFile].blocks[i]=0;

        file[curFile].fileHandle= fileHandleCounter; // assigns a new number fileHandleCounter to the current files fileHandle


        fileHandleCounter++; // set for the next file we are about to add


	initSGCache(SG_MAX_CACHE_ELEMENTS);

    // Return the file handle 
	return(fileHandleCounter);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : sgread
// Description  : Read data from the file
//
// Inputs       : fh - file handle for the file to read from
//                buf - place to put the data
//                len - the length of the read
// Outputs      : number of bytes read, -1 if failure

int sgread(SgFHandle fh, char *buf, size_t len) {
	
	 fh--; // so that if want to access fh=1, will use file[fh=0]'th file as that is how the array works

        if((fh<0)||(fh>=21)||(file[fh].isOpen==0)) // incase file handle is bad or file is actually closed
                return (-1);

        if(file[fh].filePointer==file[fh].size) // means we are at end of file
        {
                logMessage( LOG_ERROR_LEVEL, "Error : reading beyond end of file with filehandle [%d].", fh );
                return( -1 );
        }

        int currIndex= file[fh].filePointer/1024 ; // will help to get which block id and node id to use

	char read_buf[1024]; // will store the complete block we are reading
	
	char* cache_buf= getSGDataBlock(file[fh].nodes[currIndex],file[fh].blocks[currIndex]);

	if(cache_buf==NULL){ // the req block was NOT found in cache 

        	char initPacket[SG_BASE_PACKET_SIZE], recvPacket[SG_DATA_PACKET_SIZE];
        	size_t pktlen, rpktlen;
        	SG_Node_ID loc;
        	SG_Node_ID rem=file[fh].nodes[currIndex]; // this will store the req NODE_ID
        	SG_Block_ID blkid= file[fh].blocks[currIndex]; // this will store the req BLOCK_ID
      	 	SG_SeqNum sloc, srem;
        	SG_System_OP op;
        	SG_Packet_Status ret;

		pktlen = SG_BASE_PACKET_SIZE;
        	if ( (ret = serialize_sg_packet( sgLocalNodeId , // Local ID gotten from the initialization step
                	                                rem,   // required Remote ID to retireive the desired data
                        	                      blkid,  // required Block ID to retireive the desired data
                                	    SG_OBTAIN_BLOCK,  // Operation to receive the block to be read
                                    	   SG_SEQNO_UNKNOWN,    // Sender sequence number
                                    	   SG_SEQNO_UNKNOWN,  // Receiver sequence number
                                    	   NULL, initPacket, &pktlen)) != SG_PACKT_OK ) {
                	logMessage( LOG_ERROR_LEVEL, "READ: failed serialization of packet [%d].", ret );
                	return( -1 );
        	}


        	// Send the packet
            	rpktlen = SG_DATA_PACKET_SIZE;
         	if ( sgServicePost(initPacket, &pktlen, recvPacket, &rpktlen) ) {
                	logMessage( LOG_ERROR_LEVEL, "READ: failed packet post" );
                	return( -1 );
        	}


        	// Unpack the recieived data
            	if ( (ret = deserialize_sg_packet(&loc, &rem, &blkid, &op, &sloc,
                	                    &srem, read_buf, recvPacket, rpktlen)) != SG_PACKT_OK ) {
                	logMessage( LOG_ERROR_LEVEL, "READ: failed deserialization of packet [%d]", ret );
                	return( -1 );
              	}

        	// Sanity check the return value
        	if ( loc != sgLocalNodeId ) {
                	logMessage( LOG_ERROR_LEVEL, "READ: bad local ID returned [%ul]", loc );
                	return( -1 );
          	}

		//Update Cache 
		putSGDataBlock(file[fh].nodes[currIndex],file[fh].blocks[currIndex], read_buf);
	 }
	 else{// req block was found in cache
		 memcpy(&read_buf[0],&cache_buf[0],1024);
		 //Update Cache's timestamp only, the data, nodeId,blkid  remain the same
                putSGDataBlock(file[fh].nodes[currIndex],file[fh].blocks[currIndex], read_buf);
	 }


	// in both foll cases, we dont read the other untocuched half of the block                        
         if((file[fh].filePointer % 1024)==0){ // we are at beginning of block -->> plain read of 1st half of block
                 memcpy(&buf[0],&read_buf[0],512); // Reading the 1st half
            }
         else if((file[fh].filePointer % 1024)==512){ // we are at half point of block -->>read 2nd half of currBlock 
                 memcpy(&buf[0],&read_buf[512],512); // reading the 2nd half          
            }	
	// Have read the desired half of the block

        file[fh].filePointer += len;


    // Return the bytes processed
    return( len );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : sgwrite
// Description  : write data to the file
//
// Inputs       : fh - file handle for the file to write to
//                buf - pointer to data to write
//                len - the length of the write
// Outputs      : number of bytes written if successful test, -1 if failure

int sgwrite(SgFHandle fh, char *buf, size_t len) {
	
	 fh--; // so that if want to access fh=1, will use file[fh=0]'th file as that is how the array works
/*
                                                                                        // incase the filePointer is NOT at end of file
        if((file[fh].filePointer!=file[fh].size)||(fh<0)||(fh>=21)||(file[fh].isOpen==0)) // incase file handle is bad or file is actually closed
                return (-1);
*/
                                                                                        // incase the filePointer is NOT at end of file
        if((fh<0)||(fh>=21)||(file[fh].isOpen==0)) // incase file handle is bad or file is actually closed
                return (-1);

	if((file[fh].filePointer==file[fh].size)&&(file[fh].size % 1024 == 0)){ // means that we need to create a new block 

        	char initPacket[SG_DATA_PACKET_SIZE], recvPacket[SG_BASE_PACKET_SIZE];
        	size_t pktlen, rpktlen;
        	SG_Node_ID loc;
        	// SG_Block_ID blkid;
        	SG_SeqNum sloc, srem;
        	SG_System_OP op;
        	SG_Packet_Status ret;
		char temp_buf [1024]; // since serialize/deserialize need a 1024 sized buffer
		memcpy(&temp_buf[0],&buf[0],512); // copying req data to first half

        	pktlen = SG_DATA_PACKET_SIZE;
        	if ( (ret = serialize_sg_packet( sgLocalNodeId, // Local ID received from initialization
                	                    SG_NODE_UNKNOWN,   // Remote ID
                        	            SG_BLOCK_UNKNOWN,  // Block ID
                                	    SG_CREATE_BLOCK,  // Operation to create block
                                            SG_SEQNO_UNKNOWN,    // Sender sequence number
                                     	    SG_SEQNO_UNKNOWN,  // Receiver sequence number
                            	//potential error
                                	    temp_buf,               // data gotten from fn param to put inside packet
                                    	initPacket, &pktlen)) != SG_PACKT_OK ) {
                	logMessage( LOG_ERROR_LEVEL, "sgInitEndpoint: failed serialization of packet [%d].", ret );
                	return( -1 );
        	}


		// Send the packet
    		rpktlen = SG_DATA_PACKET_SIZE;
    		if ( sgServicePost(initPacket, &pktlen, recvPacket, &rpktlen) ) {
        		logMessage( LOG_ERROR_LEVEL, "sgInitEndpoint: failed packet post" );
        		return( -1 );
   		 }
// CHANGE FOLL LINE !!!!!! method to get currIndex will change 
	// can also append here
        	int currIndex= file[fh].filePointer/1024 ; // dividing the integer file pointer by 1024 to get the index of block id 
                                                // and node id to store


    		// Unpack the recieived data
    		// assigns the node Id to the currIndex'th element of the resp array, and does the same for block ID received
    		if ( (ret = deserialize_sg_packet(&loc, &file[fh].nodes[currIndex], &file[fh].blocks[currIndex], &op, &sloc,
                		                    &srem, NULL, recvPacket, rpktlen)) != SG_PACKT_OK ) {
        		logMessage( LOG_ERROR_LEVEL, "sgInitEndpoint: failed deserialization of packet [%d]", ret );
        		return( -1 );
    		   }

    		// Sanity check the return value
    		if ( loc != sgLocalNodeId) {   // in case the loc sent in wasnt the same as received
        		logMessage( LOG_ERROR_LEVEL, "sgInitEndpoint: bad local ID returned [%ul]", loc );
        		return( -1 );
    		  }
		
		// created a new block with 1st half data filled by buf
        	file[fh].size += len; // size increases in this case as we have created new block
		
	}
	else if((file[fh].filePointer==file[fh].size)&&(file[fh].size % 1024 !=0)){ // complete block at end of file
				
		int currIndex= file[fh].filePointer/1024 ; // dividing the integer file pointer by 1024 to get the index of block id 
                                                // and node id to store
	
		
		char obtain_buf [1024]; // will recieve the original block 
		
		// obtaining original block
		
		//first checking the cache
		char* cache_buf= getSGDataBlock(file[fh].nodes[currIndex],file[fh].blocks[currIndex]);
		
		if(cache_buf==NULL){ // the req block was NOT found in cache
		
			//obtaining from SGservice then
				// Local variables
		    	char initPacket[SG_BASE_PACKET_SIZE], recvPacket[SG_DATA_PACKET_SIZE];
		    	size_t pktlen, rpktlen;
		    	SG_Node_ID loc;
		    	SG_Node_ID rem=file[fh].nodes[currIndex]; // this will store the req NODE_ID
	            	SG_Block_ID blkid= file[fh].blocks[currIndex]; // this will store the req BLOCK_ID
		    	SG_SeqNum sloc, srem;
		    	SG_System_OP op;
		    	SG_Packet_Status ret;


		    	// Setup the packet
		    	pktlen = SG_BASE_PACKET_SIZE;
		    	if ( (ret = serialize_sg_packet( sgLocalNodeId, // Local ID
                        	        		     	   rem,   // Remote ID
                			                    	 blkid,  // Block ID
		                        	       SG_OBTAIN_BLOCK,  // Operation
                                		      SG_SEQNO_UNKNOWN,    // Sender sequence number
                		                      SG_SEQNO_UNKNOWN,  // Receiver sequence number
		                                      NULL, initPacket, &pktlen)) != SG_PACKT_OK ) {
		        	logMessage( LOG_ERROR_LEVEL, "sgObtainBlock: failed serialization of packet [%d].", ret );
		        	return( -1 );
		    	}

			// Send the packet
		    	rpktlen = SG_DATA_PACKET_SIZE;
		    	if ( sgServicePost(initPacket, &pktlen, recvPacket, &rpktlen) ) {
		        	logMessage( LOG_ERROR_LEVEL, "sgObtainBlock: failed packet post" );
        			return( -1 );
		    	}

		   	 // Unpack the recieived data
		    	if ( (ret = deserialize_sg_packet(&loc, &rem, &blkid, &op, &sloc,
        	                	            &srem, obtain_buf, recvPacket, rpktlen)) != SG_PACKT_OK ) {
        			logMessage( LOG_ERROR_LEVEL, "sgObtainBlock: failed deserialization of packet [%d]", ret );
        			return( -1 );
    			}

    			// Sanity check the return value
    			if ( loc == SG_NODE_UNKNOWN ) {
        			logMessage( LOG_ERROR_LEVEL, "sgObtainBlock: bad local ID returned [%ul]", loc );
        			return( -1 );
	    		}
			//Update Cache 
        	        //putSGDataBlock(file[fh].nodes[currIndex],file[fh].blocks[currIndex], obtain_buf);
         	}
         	else{// req block was found in cache
                	 memcpy(&obtain_buf[0],&cache_buf[0],1024);
         	}



		// origninal block obtained in obtain_buf
		// Now need to modify it's second half
		

			// Local variables
		    char initPacket_1[SG_DATA_PACKET_SIZE], recvPacket_1[SG_BASE_PACKET_SIZE];
		    size_t pktlen, rpktlen;
                    SG_Node_ID loc;
                    SG_Node_ID rem=file[fh].nodes[currIndex]; // this will store the req NODE_ID
                    SG_Block_ID blkid= file[fh].blocks[currIndex]; // this will store the req BLOCK_ID
                    SG_SeqNum sloc, srem;
                    SG_System_OP op;
                    SG_Packet_Status ret;

		    //size_t pktlen, rpktlen;
		    char update_buf[1024];

		   memcpy(&update_buf[0],&obtain_buf[0],512); // copying original data into first half of update)buf
		   memcpy(&update_buf[512],&buf[0],512); // copying new data into second half of update_buf 



		    // Setup the packet
		    pktlen = SG_BASE_PACKET_SIZE;
		    if ( (ret = serialize_sg_packet(sgLocalNodeId, // Local ID
                                		    rem,   // Remote ID
                		                    blkid,  // Block ID
		                                    SG_UPDATE_BLOCK,  // Operation
                                		    SG_SEQNO_UNKNOWN,    // Sender sequence number
                		                    SG_SEQNO_UNKNOWN,  // Receiver sequence number
		                                    update_buf, // final full data block being sent to originial block position
						    initPacket_1, &pktlen)) != SG_PACKT_OK ) {
		        logMessage( LOG_ERROR_LEVEL, "sgUpdateBlock: failed serialization of packet [%d].", ret );
		        return( -1 );
		    }

		// Send the packet
		    rpktlen = SG_BASE_PACKET_SIZE;
		    if ( sgServicePost(initPacket_1, &pktlen, recvPacket_1, &rpktlen) ) {
		        logMessage( LOG_ERROR_LEVEL, "sgUpdateBlock: failed packet post" );
        		return( -1 );
		    }

		    // Unpack the recieived data
		    if ( (ret = deserialize_sg_packet(&loc, &rem, &blkid, &op, &sloc,
        	                            &srem, NULL, recvPacket_1, rpktlen)) != SG_PACKT_OK ) {
        		logMessage( LOG_ERROR_LEVEL, "sgUpdateBlock: failed deserialization of packet [%d]", ret );
        		return( -1 );
	            }

    	       // Sanity check the return value

    		if ( loc == SG_NODE_UNKNOWN ) {
        		logMessage( LOG_ERROR_LEVEL, "sgUpdateBlock: bad local ID returned [%ul]", loc );
        		return( -1 );
	           }

		
		//Update Cache
                    putSGDataBlock(file[fh].nodes[currIndex],file[fh].blocks[currIndex],update_buf);


		// Required block(at end of file) updated(both in cache and SGservice) with new data in second half

        	file[fh].size += len; // size increases in this case as we have added a new half to the last block

	}
	else if(file[fh].filePointer<file[fh].size){ // we are supposed to update a pre-existing block
	
		int currIndex= file[fh].filePointer/1024 ; // dividing the integer file pointer by 1024 to get the index of block id 
                                                // and node id to store

                char obtain_buf [1024]; // will recieve the original block 
		char update_buf[1024]; // will store the final data to be sent back to SGsystem

		// obtaining original block, since need this for both if and else-if cases ahead

		//first checking the cache
                char* cache_buf= getSGDataBlock(file[fh].nodes[currIndex],file[fh].blocks[currIndex]);

                if(cache_buf==NULL){ // the req block was NOT found in cache

                                // Local variables
         	        char initPacket[SG_BASE_PACKET_SIZE], recvPacket[SG_DATA_PACKET_SIZE];
                	size_t pktlen, rpktlen;
                    	SG_Node_ID loc;
                    	SG_Node_ID rem=file[fh].nodes[currIndex]; // this will store the req NODE_ID
                    	SG_Block_ID blkid= file[fh].blocks[currIndex]; // this will store the req BLOCK_ID
                    	SG_SeqNum sloc, srem;
                    	SG_System_OP op;
                    	SG_Packet_Status ret;

                    	// Setup the packet
                    	pktlen = SG_BASE_PACKET_SIZE;
                    	if ( (ret = serialize_sg_packet( sgLocalNodeId, // Local ID
                        	                                   rem,   // Remote ID
                                                     		 blkid,  // Block ID
                                                       SG_OBTAIN_BLOCK,  // Operation
                                                      SG_SEQNO_UNKNOWN,    // Sender sequence number
                                                      SG_SEQNO_UNKNOWN,  // Receiver sequence number
                                                      NULL, initPacket, &pktlen)) != SG_PACKT_OK ) {
                        	logMessage( LOG_ERROR_LEVEL, "sgObtainBlock: failed serialization of packet [%d].", ret );
                        	return( -1 );
                    	   }

			// Send the packet
                    	rpktlen = SG_DATA_PACKET_SIZE;
                    	if ( sgServicePost(initPacket, &pktlen, recvPacket, &rpktlen) ) {
                        	logMessage( LOG_ERROR_LEVEL, "sgObtainBlock: failed packet post" );
                        	return( -1 );
                    	}

                    	// Unpack the recieived data
                    	if ( (ret = deserialize_sg_packet(&loc, &rem, &blkid, &op, &sloc,
                        	                    &srem, obtain_buf, recvPacket, rpktlen)) != SG_PACKT_OK ) {
                        	logMessage( LOG_ERROR_LEVEL, "sgObtainBlock: failed deserialization of packet [%d]", ret );
                        	return( -1 );
                	}

                	// Sanity check the return value
                	if ( loc == SG_NODE_UNKNOWN ) {
                        	logMessage( LOG_ERROR_LEVEL, "sgObtainBlock: bad local ID returned [%ul]", loc );
                        	return( -1 );
                	}
 	
		     //Update Cache 
                      // putSGDataBlock(file[fh].nodes[currIndex],file[fh].blocks[currIndex], obtain_buf);
                }
                else{// req block was found in cache
                         memcpy(&obtain_buf[0],&cache_buf[0],1024);
                }



                // origninal block obtained in obtain_buf


	      // in both foll cases, we preserve the other untocuched half of the block and resend it back with new half			
		if((file[fh].filePointer % 1024)==0){ // we are at beginning of block -->> plain update of 1st half of block
			memcpy(&update_buf[0],&buf[0],512); // updating new data in 1st half
			memcpy(&update_buf[512],&obtain_buf[512],512);	// preserving other hlf	(2nd one)	
				
		}
		else if((file[fh].filePointer % 1024)==512){ // we are at half point of block -->> update 2nd half of currBlock	
			 memcpy(&update_buf[0],&obtain_buf[0],512); // preserving 1st half 
		 	 memcpy(&update_buf[512],&buf[0],512); //updating new data in 2nd half	 	
		}
		
	      // Now updating the Block 

		// Local variables
                    char initPacket_1[SG_DATA_PACKET_SIZE], recvPacket_1[SG_BASE_PACKET_SIZE];
		    size_t pktlen, rpktlen;
                    SG_Node_ID loc;
                    SG_Node_ID rem=file[fh].nodes[currIndex]; // this will store the req NODE_ID
                    SG_Block_ID blkid= file[fh].blocks[currIndex]; // this will store the req BLOCK_ID
                    SG_SeqNum sloc, srem;
                    SG_System_OP op;
                    SG_Packet_Status ret;

		    // Setup the packet
                    pktlen = SG_BASE_PACKET_SIZE;
                    if ( (ret = serialize_sg_packet(sgLocalNodeId, // Local ID
                                                    rem,   // Remote ID
                                                    blkid,  // Block ID
                                                    SG_UPDATE_BLOCK,  // Operation
                                                    SG_SEQNO_UNKNOWN,    // Sender sequence number
                                                    SG_SEQNO_UNKNOWN,  // Receiver sequence number
                                                    update_buf, // final full data block being sent to originial block position
                                                    initPacket_1, &pktlen)) != SG_PACKT_OK ) {
                        logMessage( LOG_ERROR_LEVEL, "sgUpdateBlock: failed serialization of packet [%d].", ret );
                        return( -1 );
                    }

                // Send the packet
                    rpktlen = SG_BASE_PACKET_SIZE;
                    if ( sgServicePost(initPacket_1, &pktlen, recvPacket_1, &rpktlen) ) {
                        logMessage( LOG_ERROR_LEVEL, "sgUpdateBlock: failed packet post" );
                        return( -1 );
                    }

                    // Unpack the recieived data
                    if ( (ret = deserialize_sg_packet(&loc, &rem, &blkid, &op, &sloc,
                                            &srem, NULL, recvPacket_1, rpktlen)) != SG_PACKT_OK ) {
                        logMessage( LOG_ERROR_LEVEL, "sgUpdateBlock: failed deserialization of packet [%d]", ret );
                        return( -1 );
                }

		 // Sanity check the return value
                if ( loc == SG_NODE_UNKNOWN ) {
                        logMessage( LOG_ERROR_LEVEL, "sgUpdateBlock: bad local ID returned [%ul]", loc );
                        return( -1 );
                }
		
		//Update Cache
                  putSGDataBlock(file[fh].nodes[currIndex],file[fh].blocks[currIndex], update_buf);

		// Updated the desired half of the block(both in cache and SGservice) with new data(buf) and preserved other half

	}	

        	file[fh].filePointer += len;


    // Log the write, return bytes written
    return( len );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : sgseek
// Description  : Seek to a specific place in the file
//
// Inputs       : fh - the file handle of the file to seek in
//                off - offset within the file to seek to
// Outputs      : new position if successful, -1 if failure

int sgseek(SgFHandle fh, size_t off) {
	fh--; // so that if want to access fh=1, will use file[fh=0]'th file as that is how the array works


                                                                //  if location is beyond file, or
        if((file[fh].size<off)||(fh<0)||(fh>=21)||(file[fh].isOpen==0)) // incase file handle is bad or file is actually closed
                return (-1);

        file[fh].filePointer= off;

    // Return new position
    return( off );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : sgclose
// Description  : Close the file
//
// Inputs       : fh - the file handle of the file to close
// Outputs      : 0 if successful test, -1 if failure

int sgclose(SgFHandle fh) {
	 fh--; // so that if want to access fh=1, will use file[fh=0]'th file as that is how the array works

        if((fh<0)||(fh>=21)||(file[fh].isOpen==0)) // incase file handle is bad or file is actually closed
                return (-1);


        file[fh].isOpen=0; // this lets us know that file with fileHandle=fh has been closed


    // Return successfully
    return( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : sgshutdown
// Description  : Shut down the filesystem
//
// Inputs       : none
// Outputs      : 0 if successful test, -1 if failure

int sgshutdown(void) {
        
	fileHandleCounter=0;
//	closeSGCache();
//      for(int i=0;i<20;i++)
//              file[i]=NULL;

// shutting down system 
        // Local variables
    char initPacket[SG_BASE_PACKET_SIZE], recvPacket[SG_BASE_PACKET_SIZE];
    size_t pktlen, rpktlen;
    SG_Node_ID loc, rem;
    SG_Block_ID blkid;
    SG_SeqNum sloc, srem;
    SG_System_OP op;
    SG_Packet_Status ret;

    // Local and do some initial setup
    logMessage( LOG_INFO_LEVEL, "Initializing shutdown ..." );
    sgLocalSeqno = SG_INITIAL_SEQNO;

    // Setup the packet
    pktlen = SG_BASE_PACKET_SIZE;
    if ( (ret = serialize_sg_packet( sgLocalNodeId, // Local ID
                                    SG_NODE_UNKNOWN,   // Remote ID
                                    SG_BLOCK_UNKNOWN,  // Block ID
                                    SG_STOP_ENDPOINT,  // Operation to tell the system to stop and shutdown
                                    sgLocalSeqno++,    // Sender sequence number
                                    SG_SEQNO_UNKNOWN,  // Receiver sequence number
                                    NULL, initPacket, &pktlen)) != SG_PACKT_OK ) {
        logMessage( LOG_ERROR_LEVEL, "SG_STOP_ENDPOINT,: failed serialization of packet [%d].", ret );
        return( -1 );
    }

    // Send the packet
    rpktlen = SG_BASE_PACKET_SIZE;
    if ( sgServicePost(initPacket, &pktlen, recvPacket, &rpktlen) ) {
        logMessage( LOG_ERROR_LEVEL, "SG_STOP_ENDPOINT,: failed packet post" );
        return( -1 );
    }

    // Unpack the recieived data
    if ( (ret = deserialize_sg_packet(&loc, &rem, &blkid, &op, &sloc,
                                    &srem, NULL, recvPacket, rpktlen)) != SG_PACKT_OK ) {
        logMessage( LOG_ERROR_LEVEL, "SG_STOP_ENDPOINT,: failed deserialization of packet [%d]", ret );
        return( -1 );
    }

    // Sanity check the return value
    if ( loc == SG_NODE_UNKNOWN ) {
        logMessage( LOG_ERROR_LEVEL, "SG_STOP_ENDPOINT,: bad local ID returned [%ul]", loc );
        return( -1 );
    }

    
    closeSGCache();

    // Log, return successfully
    logMessage( LOG_INFO_LEVEL, "Shut down Scatter/Gather driver." );
    return( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : serialize_sg_packet
// Description  : Serialize a ScatterGather packet (create packet)
//
// Inputs       : loc - the local node identifier
//                rem - the remote node identifier
//                blk - the block identifier
//                op - the operation performed/to be performed on block
//                sseq - the sender sequence number
//                rseq - the receiver sequence number
//                data - the data block (of size SG_BLOCK_SIZE) or NULL
//                packet - the buffer to place the data
//                plen - the packet length (int bytes)
// Outputs      : 0 if successfully created, -1 if failure

SG_Packet_Status serialize_sg_packet(SG_Node_ID loc, SG_Node_ID rem, SG_Block_ID blk,
                                     SG_System_OP op, SG_SeqNum sseq, SG_SeqNum rseq, char *data,
                                     char *packet, size_t *plen) {

// REPLACE THIS CODE WITH YOU ASSIGNMENT #2



	uint8_t data_ind;

        if(data==NULL)
                data_ind= 0x00;
        else
                data_ind=0x01;

// validation
        uint64_t temp= 0x00000000;

        if(loc == temp)
                return 1;

        if(rem== temp)
                return 2;

        if(blk== temp)
                return 3;

        if( (op<0)||(op>6))
                return 4;

        if( sseq == 0x00)
                return 5;

        if(rseq== 0x00)
                return 6;

        if((data_ind)&&(data == NULL))  // if data_ind = 0x01 and data points to NULL, i.e, there is no data, then dataBlokBADD
                return 7;

         if (packet== NULL)
                 return 9;

	 // validation done

        if(plen==NULL)
                logMessage(LOG_INFO_LEVEL, "plen pointer is NULL");

// Creating buffer

        *plen=0;

        memcpy(&packet[0],&Magic,4 );
        *plen+= sizeof(Magic);

        memcpy(&packet[sizeof(Magic)],&loc,8 );
        *plen+=sizeof(loc);

        memcpy(&packet[sizeof(Magic)+sizeof(loc)],&rem,8 );
        *plen+= sizeof(rem);

        memcpy(&packet[sizeof(Magic)+sizeof(loc)+sizeof(rem)],&blk,8 );
        *plen+= sizeof(blk);

        memcpy(&packet[sizeof(Magic)+sizeof(loc)+sizeof(rem)+sizeof(blk)],&op,4 );
        *plen +=sizeof(op);

        memcpy(&packet[sizeof(Magic)+sizeof(loc)+sizeof(rem)+sizeof(blk)+ sizeof(op)],&sseq,2 );
        *plen +=sizeof(sseq);

        memcpy(&packet[sizeof(Magic)+sizeof(loc)+sizeof(rem)+sizeof(blk)+ sizeof(op)+sizeof(sseq)],&rseq,2 );
        *plen += sizeof(rseq);

        memcpy(&packet[sizeof(Magic)+sizeof(loc)+sizeof(rem)+sizeof(blk)+ sizeof(op)+sizeof(sseq)+sizeof(rseq)],&data_ind, 1 );
        *plen += sizeof(data_ind);

	
	if(data_ind)
        {
               memcpy(&packet[sizeof(Magic)+sizeof(loc)+sizeof(rem)+sizeof(blk)+ sizeof(op)+sizeof(sseq)+sizeof(rseq)+sizeof(data_ind)],&data[0], 1024);

                *plen+=1024 ;

                //memcpy(&packet[sizeof(Magic)+sizeof(loc)+sizeof(rem)+sizeof(blk)+ sizeof(op)+sizeof(sseq)+sizeof(rseq)+sizeof(data_ind)],&data, 8);
                //*plen+=8 ;


   //memcpy(&packet[sizeof(Magic)+sizeof(loc)+sizeof(rem)+sizeof(blk)+ sizeof(op)+sizeof(sseq)+sizeof(rseq)+sizeof(data_ind)+sizeof(data)],&Magic, 4);
     memcpy(&packet[sizeof(Magic)+sizeof(loc)+sizeof(rem)+sizeof(blk)+ sizeof(op)+sizeof(sseq)+sizeof(rseq)+sizeof(data_ind)+1024],&Magic, 4);
        }
        else
        {
                //memcpy(&packet[sizeof(Magic)+sizeof(loc)+sizeof(rem)+sizeof(blk)+ sizeof(op)+sizeof(sseq)+sizeof(rseq)+sizeof(data_ind)],0x00, 1);

                packet[sizeof(Magic)+sizeof(loc)+sizeof(rem)+sizeof(blk)+ sizeof(op)+sizeof(sseq)+sizeof(rseq)+sizeof(data_ind)]=0x00;
                *plen+=1 ;

                memcpy(&packet[sizeof(Magic)+sizeof(loc)+sizeof(rem)+sizeof(blk)+ sizeof(op)+sizeof(sseq)+sizeof(rseq)+sizeof(data_ind)+1],&Magic,4);

        }

     *plen += sizeof(Magic);



    // Return the system function return value
    return( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : deserialize_sg_packet
// Description  : De-serialize a ScatterGather packet (unpack packet)
//
// Inputs       : loc - the local node identifier
//                rem - the remote node identifier
//                blk - the block identifier
//                op - the operation performed/to be performed on block
//                sseq - the sender sequence number
//                rseq - the receiver sequence number
//                data - the data block (of size SG_BLOCK_SIZE) or NULL
//                packet - the buffer to place the data
//                plen - the packet length (int bytes)
// Outputs      : 0 if successfully created, -1 if failure

SG_Packet_Status deserialize_sg_packet(SG_Node_ID *loc, SG_Node_ID *rem, SG_Block_ID *blk,
                                       SG_System_OP *op, SG_SeqNum *sseq, SG_SeqNum *rseq, char *data,
                                       char *packet, size_t plen) {

// REPLACE THIS CODE WITH YOU ASSIGNMENT #2
	

	enableLogLevels(LOG_INFO_LEVEL);

        size_t temp_plen=0;

        uint8_t data_ind;

/*
        if(data==NULL)
                data_ind= 0x00;
        else
                data_ind=0x01;
*/

//      assert(loc!=NULL);
//      assert(rem!=NULL);
//      assert(blk!=NULL);
//      assert(op!=NULL);
//copying data from pinter into variabls

        uint32_t Magic1;
        uint32_t Magic2;

        memcpy(&Magic1, &packet[0],4 );

        memcpy(&loc[0],&packet[4],8 );

        memcpy(&rem[0], &packet[12],8 );

        memcpy(&blk[0], &packet[20],8 );

        memcpy(&op[0], &packet[28],4 );

        memcpy(&sseq[0] ,&packet[32],2 );

        memcpy(&rseq[0], &packet[34],2 );
	
	memcpy(&data_ind, &packet[36],1);

        if(data_ind)
        {
                memcpy(&data[0], &packet[37],1024);
                memcpy(&Magic2,&packet[1061], 4);

                // possible point of error


              //memcpy(&data[0], &packet[37],8);
                //memcpy(&Magic2,&packet[45], 4);
        }
        else
        {
              //  memcpy(&data[0], &packet[37], 1);

                if(data!=NULL){
                        data[0]= 0x00;

                        memcpy(&Magic2,&packet[38], 4);
                }
                else memcpy(&Magic2,&packet[37], 4);
        }



//validation
//logMessage(LOG_INFO_LEVEL,"test 1");

        if(Magic1 != 0xfefe)
                return SG_PACKT_PDATA_BAD;

        temp_plen+= sizeof(Magic1);

        uint64_t tmp= 0x00000000;

        if(*loc == tmp)
                return 1;
        temp_plen+=8;

        if(*rem== tmp)
                return 2;
        temp_plen+= 8;
        if(*blk== tmp)
                return 3;
        temp_plen+=8;

//logMessage(LOG_INFO_LEVEL,"test 2");

        if( (*op<0)||(*op>6))
                return 4;
        temp_plen+= 4;

        if( *sseq == 0x00)
                return 5;
        temp_plen+=2;

        if(*rseq== 0x00)
                return 6;
        temp_plen+=2;

	 if((data_ind)&&(data== NULL))  // if data_ind = 0x01 and data points to NULL, i.e, there is no data, then dataBlokBADD
                return 7;

        temp_plen+=1; // for data indicator

        if(data_ind)
                temp_plen+=1024; // when there is data then its size if 1024 bytes
                //temp_plen+=8; // when there is data then its size if 1024 bytes
        else
                temp_plen+=0; // when there is no data thensize of data is 1 byte

        if (packet== NULL)
                 return 9;

         if(Magic2 != SG_MAGIC_VALUE)
                return SG_PACKT_PDATA_BAD;
        temp_plen+=sizeof(Magic2);

        if (temp_plen!= plen)
                 return 8;

    // Return the system function return value
    return( 0 );
}

//
// Driver support functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : sgInitEndpoint
// Description  : Initialize the endpoint
//
// Inputs       : none
// Outputs      : 0 if successfull, -1 if failure

int sgInitEndpoint( void ) {

    // Local variables
    char initPacket[SG_BASE_PACKET_SIZE], recvPacket[SG_BASE_PACKET_SIZE];
    size_t pktlen, rpktlen;
    SG_Node_ID loc, rem;
    SG_Block_ID blkid;
    SG_SeqNum sloc, srem;
    SG_System_OP op;
    SG_Packet_Status ret;

    // Local and do some initial setup
    logMessage( LOG_INFO_LEVEL, "Initializing local endpoint ..." );
    sgLocalSeqno = SG_INITIAL_SEQNO;

    // Setup the packet
    pktlen = SG_BASE_PACKET_SIZE;
    if ( (ret = serialize_sg_packet( SG_NODE_UNKNOWN, // Local ID
                                    SG_NODE_UNKNOWN,   // Remote ID
                                    SG_BLOCK_UNKNOWN,  // Block ID
                                    SG_INIT_ENDPOINT,  // Operation
                                    sgLocalSeqno++,    // Sender sequence number
                                    SG_SEQNO_UNKNOWN,  // Receiver sequence number
                                    NULL, initPacket, &pktlen)) != SG_PACKT_OK ) {
        logMessage( LOG_ERROR_LEVEL, "sgInitEndpoint: failed serialization of packet [%d].", ret );
        return( -1 );
    }

    // Send the packet
    rpktlen = SG_BASE_PACKET_SIZE;
    if ( sgServicePost(initPacket, &pktlen, recvPacket, &rpktlen) ) {
        logMessage( LOG_ERROR_LEVEL, "sgInitEndpoint: failed packet post" );
        return( -1 );
    }

    // Unpack the recieived data
    if ( (ret = deserialize_sg_packet(&loc, &rem, &blkid, &op, &sloc, 
                                    &srem, NULL, recvPacket, rpktlen)) != SG_PACKT_OK ) {
        logMessage( LOG_ERROR_LEVEL, "sgInitEndpoint: failed deserialization of packet [%d]", ret );
        return( -1 );
    }

    // Sanity check the return value
    if ( loc == SG_NODE_UNKNOWN ) {
        logMessage( LOG_ERROR_LEVEL, "sgInitEndpoint: bad local ID returned [%ul]", loc );
        return( -1 );
    }

    // Set the local node ID, log and return successfully
    sgLocalNodeId = loc;
    logMessage( LOG_INFO_LEVEL, "Completed initialization of node (local node ID %lu", sgLocalNodeId );
    return( 0 );
}
