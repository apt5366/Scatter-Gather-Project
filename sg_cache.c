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
#include <stdlib.h>
#include <stdio.h>
#include <cmpsc311_log.h>
#include <time.h>
#include <string.h>
// Project Includes
#include <sg_cache.h>

// Defines
typedef struct cache_st{
    
	int cacheLine;
        SG_Node_ID nodeId;
	SG_Block_ID blockId;
	char data[1024];	

} Cache;

Cache *cache;

int queryCount, hitCount; // will store no.of queries and hits resp

int* LRUqueryCount; 

// Functional Prototypes

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : initSGCache
// Description  : Initialize the cache of block elements
//
// Inputs       : maxElements - maximum number of elements allowed
// Outputs      : 0 if successful, -1 if failure

int initSGCache( uint16_t maxElements ) {
	queryCount=0;
       	hitCount=0;	
	cache= (Cache*) malloc(maxElements*sizeof(Cache));
	
	if(cache == NULL){
               logMessage( LOG_ERROR_LEVEL, "cach malloc fail" );
               return( -1 );
        }
	
// 	int x[maxElements];
	
	LRUqueryCount=(int*) malloc(maxElements*sizeof(int));; // so now LRUqueryCount is an int array with size of cache
	int k;
        for(k=0;k<maxElements;k++)
                LRUqueryCount[k]=0;


	char temp[1024];
	int j;
	for(j=0;j<1024;j++)
		temp[j]='0';
/*		
	int i;
	for(i=0;i<maxElements;i++){
		cache[i]->cacheLine=i;
		cache[i]->nodeId=0;
		cache[i]->blockId=0;
		cache[i]->data=temp;
	}
*/
	int i;
        for(i=0;i<maxElements;i++){
                cache[i].cacheLine=i;
                cache[i].nodeId=0;
                cache[i].blockId=0;
               memcpy(&cache[i].data[0],&temp[0],1024); // making all entries 0 for cache data block
        }

// !!! Dynamically Allocate an array to track timestamps(querystamps) for LRU impl

	

    // Return successfully
    return( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : closeSGCache
// Description  : Close the cache of block elements, clean up remaining data
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int closeSGCache( void ) {
	
	free(cache);
	free(LRUqueryCount);
	// check if you need to free up qno as well
	//
	//
	logMessage(LOG_INFO_LEVEL,"[CACHE]No.of Queries: %d ",queryCount );
	
	float hitRatio= hitCount/(float)queryCount;
	logMessage(LOG_INFO_LEVEL,"[CACHE]Hit ratio: %f ",hitRatio );
	
	logMessage(LOG_INFO_LEVEL,"[CACHE]Hit count: %d ",hitCount );
    // Return successfully
    return( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : getSGDataBlock
// Description  : Get the data block from the block cache
//
// Inputs       : nde - node ID to find
//                blk - block ID to find
// Outputs      : pointer to block or NULL if not found

char * getSGDataBlock( SG_Node_ID nde, SG_Block_ID blk ) {
	
	queryCount++;
	int readData=-1; // flags if the Data has been read

	// check foll line ---->>>you are probably making same error else place as well !!!!!!!!!!!!!!!!!!
	char* data=(char*)malloc(1024*sizeof(char));
	
	int temp_indexes[sizeof(cache)]; 

	int k;
	for(k=0;k<sizeof(cache);k++)
		temp_indexes[k]=-1; // initialzinng it

	int i;
	for(i=0;i<sizeof(cache);i++){
		if(cache[i].nodeId == nde){
			temp_indexes[i]=i;// filling the index no. in index just to mark for reference		
		}
	}	


	int j;
	for(j=0;j<sizeof(cache);j++){
		if(temp_indexes[j]==j){ // now finding our reference marked indexes
			if(cache[j].blockId == blk){ // since these indexes have correct nodeId, then one of these will also match blkID
				  memcpy(&data[0],&cache[j].data[0],1024); // NodeId nd blkId both matched for this, so storing this data block
				  hitCount++; // as this represents a Hit
				  readData=1; // tells the prog that data has been read
				  LRUqueryCount[j]=queryCount; // update timestamp(querystamp) for this cacheline for LRU impl
			}
		}

	}

    if(readData==-1)
    	return NULL;	    
    else
    	return data ;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : putSGDataBlock
// Description  : Get the data block from the block cache
//
// Inputs       : nde - node ID to find
//                blk - block ID to find
//                block - block to insert into cache
// Outputs      : 0 if successful, -1 if failure

int putSGDataBlock( SG_Node_ID nde, SG_Block_ID blk, char *block ) {
	queryCount++;


	//
        // first check if block already present in cache

        int temp_indexes[sizeof(cache)];
        int k;
        for(k=0;k<sizeof(cache);k++)
             temp_indexes[k]=-1; // initialzinng it


        int i;
        for(i=0;i<sizeof(cache);i++){
               if(cache[i].nodeId == nde){
                      temp_indexes[i]=i;// filling the index no. in index just to mark for reference
                }
        }


        int j;
        for(j=0;j<sizeof(cache);j++){
              if(temp_indexes[j]==j){ // now finding our reference marked indexes
                      if(cache[j].blockId == blk){ // since these indexes have correct nodeId, then one of these will also match blkID
                                memcpy(&cache[j].data[0],&block[0],1024) ; // NodeId nd blkId both matched for this, so storing this data block
                                hitCount++; // as this represents a Hit
                                LRUqueryCount[j]=queryCount; // update timestamp(querystamp) for this cacheline for LRU impl
                                return 0;
                       }
               }

        }

	// If we reached here, this means that the block was NOT already present in cache -->> MISS


	int indexesEmpty[sizeof(cache)];// tells us whcih indexes are emoty in cache
	int temp=0; // stores how many places in cache are unoccupied
	
	//initializing indexesEmpty
	int p;
	for(p=0;p<sizeof(cache);p++)
		indexesEmpty[p]=0;
	
	int l;
	for(l=0;l<sizeof(cache);l++){
		if(cache[l].nodeId == 0){
			indexesEmpty[l]=1;
			temp++;	
		}
	}
// !!!MODIFY __ Need to check if block already present in cache first
//
 

	if(temp!=0){ // meaning we actually spotted empty places 
	
		//now picking a random spot to place the buffer from recorded empty spots in range(0, temp)
	
		srand(time(0));
	
		int x=(rand()%(temp))+1; // will pick the random index to be selected by pickingrandomly at what no. we spotted it 

		int xCheck=0; // to match the count upto x
	
		int n;
		for(n=0;n<sizeof(cache);n++){
			if(indexesEmpty[n]==1){
				xCheck++;
				if(x==xCheck)// so j is the required index of cache to be filled
				{
					cache[n].nodeId =nde;
					cache[n].blockId = blk;
				       	memcpy(&cache[n].data[0],&block[0],1024); 
					LRUqueryCount[n]=queryCount; // update timestamp for this cache line for LRU impl	
					return 0;
				}	

					
			}
		}


	}
	else{ // we skip the above and do LRU replacement policy
		/*
		//
		// first check if block already present in cache
		
		int temp_indexes[sizeof(cache)];
		int k;
	        for(k=0;k<sizeof(cache);k++)
        	        temp_indexes[k]=-1; // initialzinng it


	        int i;
        	for(i=0;i<sizeof(cache);i++){
                	if(cache[i].nodeId == nde){
                        	temp_indexes[i]=i;// filling the index no. in index just to mark for reference
                	}
        	}


        	int j;
        	for(j=0;j<sizeof(cache);j++){
                	if(temp_indexes[j]==j){ // now finding our reference marked indexes
                        	if(cache[j].blockId == blk){ // since these indexes have correct nodeId, then one of these will also match blkID
                                       memcpy(&cache[j].data[0],&block[0],1024) ; // NodeId nd blkId both matched for this, so storing this data block
                                  	hitCount++; // as this represents a Hit
                                  	LRUqueryCount[j]=queryCount; // update timestamp(querystamp) for this cacheline for LRU impl
                        		return 0;
				}
                	}

        	}
		*/

		// if Not already in cache then we move ahead and utilize LRU func of cache to place block inside
		//

		int LRUcacheline=0; // will detect least recently used cache line by storing least val of query count
		int minQueryCount=LRUqueryCount[0]; 
		int m;
		for(m=1;m<sizeof(cache);m++){
			if(LRUqueryCount[m]<minQueryCount){
				LRUcacheline= m;
				minQueryCount= LRUqueryCount[m];
			}	
		}

		cache[LRUcacheline].nodeId =nde;
                cache[LRUcacheline].blockId = blk;
                memcpy(&cache[LRUcacheline].data[0],&block[0],1024);
                LRUqueryCount[LRUcacheline]=queryCount; // update timestamp for this cache line for LRU impl       
                return 0;
	
	}
	
	




    // Return unsuccessfully
    return( -1 );
}
