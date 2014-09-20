
/* 
 * ATSP support functions
 */
#define _GNU_SOURCE    /* endable reentry version of hash tables */
#include <stdio.h>
#include <postgres.h>
#include <search.h>    /* hash search */
#include <string.h>    /* memcpy */
#include <math.h>      /* exp    */

#include "tsp.h"

#define LEN_DIGITS	8

#ifdef DEBUG
#define DBG(format, arg...)                     \
    elog(NOTICE, format , ## arg)
#else
#define DBG(format, arg...) do { ; } while (0)
#endif

static tsp_path_element_t * mkPathData( double * graph,int pathCount,int * returnPath,struct hsearch_data *,int squareSize);
static int keyLookup( struct hsearch_data *lookup,struct hsearch_data * revLookup,int value,int *keys);

static int getLookUp(struct hsearch_data *lookup, int value);
/**
 * Build a distance matrix from user supplied data
 *
 * Data is assume to be set of target/source cost[reverse_cost values]
 *
 * Data can be either a distance matrix with or without a leading zero diagonal 
 * or just active data.
 */
bool processATSPData(tsp_edge_t  * edges,int totalTuples,int  startVertex,bool hasReverse, tsp_path_element_t **path, int *pathCount, char ** errMesg){
	int i=0,j=0;
        int squareSize;
	int tmpSize;
	double * graph;
	
	int ktarget;
	int ksource;
        int keyValues=0;
	int kstartUp;

	struct hsearch_data lookup;
	memset(&lookup,0,sizeof(struct hsearch_data));

	struct hsearch_data revLookup;
	memset(&revLookup,0,sizeof(struct hsearch_data));
	for(i=0;i<totalTuples;i++){
		DBG("%d source %d target %d cost %.4f reverse_cost %.4f",i,
			edges[i].source,edges[i].target,
			edges[i].cost,edges[i].reverse_cost);
	}
	squareSize=(int)pow((float)totalTuples,0.5);


	if(squareSize * squareSize== totalTuples){
		DBG("REgulare sqaure %d",totalTuples);
        } else {
		if(hasReverse){
			tmpSize=totalTuples;
			DBG("Asym");	
		} else {
			tmpSize=totalTuples*2;
			DBG("TSP");	
		}
		squareSize=(int)pow(tmpSize,0.5)+1;

		if((squareSize*squareSize)!=(totalTuples+squareSize)){
			*errMesg="Invalid number of elements supplied";
			return false;
		}

	}
	/* received the correct number of data items, now 
           create distance matrix */
	graph=palloc(sizeof(double) * squareSize * squareSize);
	hcreate_r(squareSize*squareSize,&lookup);
	hcreate_r(squareSize*squareSize,&revLookup);
	
	for(i=0;i<squareSize;i++){
		for(j=0;j<squareSize;j++){
			if(i==j){
				graph[(i*squareSize)+j]=0.0;
			} else {
				graph[(i*squareSize)+j]= -1.0;
			}
		}
	}
	
	for(i=0;i<totalTuples;i++){
		ksource=keyLookup(&lookup,&revLookup,edges[i].source,&keyValues);
		ktarget=keyLookup(&lookup,&revLookup,edges[i].target,&keyValues);
		graph[(ksource*squareSize)+ktarget]= edges[i].cost;
		if(hasReverse){
			graph[(ktarget*squareSize)+ksource]= edges[i].reverse_cost;
		} else {
			graph[(ktarget*squareSize)+ksource]= edges[i].cost;
		}
	}

	DBG("Processing %d elements ",squareSize);

	
#ifdef DEBUG
	{
		
    		char bufff[2048];
    		int nnn;
    		DBG("---------- REVLOOKUP total keys [%d]\n", keyValues);

		for(i=0;i< keyValues;i++){
			int ret;
			char tmp[10];
			ENTRY search;
			
			ENTRY *result;
			sprintf(tmp,"%d",i);
			search.key=tmp;
			ret=hsearch_r(search,FIND,&result,&revLookup);
			if(ret== 0 ){
				DBG("Internal error can not find lookup for key %d",i);
			} else {
				DBG("Key %d Lookup %d",i,atoi(result->data));
			}

		}

    		DBG("---------- Matrix[%d][%d] ---------------------\n", squareSize, squareSize);
    		for (i=0; i<squareSize; i++) {
        		sprintf(bufff, "%d:", i);
        		nnn = 0;
        		for (j=0; j<squareSize; j++) {
            			nnn += sprintf(bufff+nnn, "\t%.4f", graph[i*squareSize+j]);
        		}
        		DBG("%s", bufff);
    		}
	}
#endif
	/* Convert startVertex */
	kstartUp= getLookUp(&lookup, startVertex);
DBG("Kstartup is %d",kstartUp);
	if(kstartUp == -1 ){
		DBG("Can not lookup start values %d ",startVertex);
		return false;
	}
	int *returnPath=computeTSP(graph,squareSize,kstartUp,pathCount,errMesg);
	DBG("Return length of path %d",*pathCount);
	/* Got a werid result */
	if(pathCount <=0 ){
		*errMesg="Unable to get a result";
		return false;
	}

	*path=mkPathData(graph,*pathCount,returnPath,&revLookup,squareSize);
	if(path == NULL ){
		*errMesg="Corrupt name value lookup";
		return false;
	}
	hdestroy_r(&lookup);
	hdestroy_r(&revLookup);
	return true;
}
/**
 *  change the return data from the TSP engine into something(?) useful
 */
static tsp_path_element_t * mkPathData( double * graph,int pathCount,
	int * returnPath,struct hsearch_data * revLookup,int squareSize){
	int i;
	tsp_path_element_t * thePath;
DBG("Path Count %d",pathCount);	
	thePath=(tsp_path_element_t*) palloc(pathCount * sizeof(tsp_path_element_t));
	for(i=0;i<pathCount;i++){
		if(returnPath[i] < 0 ){
			return NULL;
		}
		thePath[i].vertex_id=getLookUp(revLookup,returnPath[i]);
		if(i==(pathCount-1)){
			thePath[i].cost=0;
		} else {
			thePath[i].cost=graph[(squareSize*returnPath[i])+ returnPath[i+1]];
		}
        	DBG("%d Vertex %d cost %.4f", 
			i,thePath[i].vertex_id,thePath[i].cost);
	}
	return thePath;

}
/**
 * check to see if this key exists
 */
static int getLookUp(struct hsearch_data *lookup, int value){
	
	ENTRY search;
        ENTRY *result;
        int ret;
        char tmp[10];

        sprintf(tmp,"%d",value);
        search.key=tmp;
        ret=hsearch_r(search,FIND,&result,lookup);

	if(ret == 0 ){
		// Did not find it return an ivalid result
		return -1;
	}
	return atoi(result->data);
}

static int keyLookup( struct hsearch_data *lookup,struct hsearch_data *revLookup,int value,int *suppliedKey){
	ENTRY search;
	ENTRY *result;
	int ret;
	char tmp[10];
	DBG("key processing  VALUE %d",value);
	sprintf(tmp,"%d",value);
	search.key=tmp;
	ret=hsearch_r(search,FIND,&result,lookup);

	if(ret == 0 ){
		DBG("Adding data  VALUE %d key %d",value,*suppliedKey);
		// did'nt find it so add it
		char *key=palloc(LEN_DIGITS*sizeof (char ));
		sprintf(key,"%d",value);
		ENTRY insert;
		insert.key=key;
		char *data=palloc(LEN_DIGITS*sizeof (char ));
		sprintf(data,"%d",*suppliedKey);
		insert.data=data;
		hsearch_r(insert,ENTER,&result,lookup);

		ENTRY revInsert;
		ENTRY *dummy;
		revInsert.data=key;
		revInsert.key=data;
		hsearch_r(revInsert,ENTER,&dummy,revLookup);

		(*suppliedKey)++;
	}else {
		DBG("VALUE %d already inserted ",value);

	}

	return atoi(result->data);
}
