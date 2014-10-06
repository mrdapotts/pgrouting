
/* 
 * ATSP support functions
 */

#include <stdio.h>
#include <postgres.h>

#include <string.h>    /* memcpy */
#include <math.h>      /* exp    */
#include <stdarg.h>
#include <map>
#include <stdexcept>
#include "tsp.h"
#include <functional>
#include <algorithm>


#define NO_PLACE  (-1)		// Failed to look a 
#define BAD_DATA  (-2.0)        // initial data value, 
static tspPathElementType * mkPathData( tspPathAsmParamsType *data ,std::map<int,int>  ,int squareSize,int start);

static int keyLookup(std::map<int,int> & lookup,std::map<int,int>  & revLookup, int value,int *suppliedKey);

static int getLookUp(std::map<int,int>  &lookup, int value);
static void releaseResources( tspPathAsmParamsType *data );

#define MAX_POINT_VAL 31
/**
 * Build a distance matrix from user supplied data
 *
 * Data is assume to be set of target/source cost[reverse_cost values]
 *
 * Data can be either a distance matrix with or without a leading zero diagonal 
 * or just active data.
 */
bool processATSPData(tspEdgeType  * edges,int totalTuples,int  startVertex,bool hasReverse, tspPathElementType **path, int *pathCount, char * errMesg){
	int i=0,j=0;
        int squareSize;
	int tmpSize;
	double * graph;
	
	int ktarget;
	int ksource;
        int keyValues=0;
	int kstartUp;

	std::map<int,int> stdLookup;
	std::map<int,int> stdRevLookup;
	dbgprintf("Processing %d  tuples",totalTuples);

	if(totalTuples < 3 ){
		sprintf(errMesg,"Asym requires 3 or more parmeters discovered %d ",totalTuples);
		return false;
	}

	for(i=0;i<totalTuples;i++){
		if(edges[i].source  < 0 ){
			sprintf(errMesg,"Source (%d) is negative   ", edges[i].source);
			return false;
		}
		if(edges[i].target  < 0 ){
			sprintf(errMesg,"Target (%d) is negative   ", edges[i].target);
			return false;
		}
		if(edges[i].cost  <=0 ){
			sprintf(errMesg,"Cost %4f for Source (%d) Target (%d) is invalid", 
				edges[i].cost,
				edges[i].source,
				edges[i].target);
			return false;
		}
		if(hasReverse && edges[i].reverseCost  <=0 ){
			sprintf(errMesg,"Cost %4f for Source (%d) Target (%d) is invalid", 
				edges[i].reverseCost,
				edges[i].source,
				edges[i].target);
			return false;
		}
		if(edges[i].source == edges[i].target ){
			sprintf(errMesg,"Source (%d) is the same as target (%d) cost %.4f ", 
			edges[i].source,edges[i].target, edges[i].cost);
			return false;
		}
		 dbgprintf("index %d source %d target %d cost %.4f reverse cost %.4f",i,
			edges[i].source,edges[i].target,
			edges[i].cost,edges[i].reverseCost);
	}
	squareSize=(int)pow((float)totalTuples,0.5);


	if(hasReverse){
		tmpSize=totalTuples*2;
		dbgprintf("Asym tmpSize %i",tmpSize);	
	} else {
		tmpSize=totalTuples*2;
		dbgprintf("TSP tmpSize %i",tmpSize);	
	}
	squareSize=(int)pow(tmpSize,0.5)+1;

	if((squareSize*squareSize)!=(2*totalTuples)+squareSize){

		sprintf(errMesg,"Invalid number of elements supplied");
		return false;
	}

	if(squareSize <3 ){
		sprintf(errMesg,"Invalid number of elements supplied must be at least 3");
		return false;
	}
	/* received the correct number of data items, now 
           create distance matrix */
	graph=(double *) getMem(sizeof(double) * squareSize * squareSize);
	
	for(i=0;i<squareSize;i++){
		for(j=0;j<squareSize;j++){
			if(i==j){
				graph[(i*squareSize)+j]=0.0;
			} else {
				graph[(i*squareSize)+j]= BAD_DATA;
			}
		}
	}
#ifdef DEBUG
	{
		
    		char bufff[2048];
    		int nnn;
	
    		dbgprintf("---------- Starting Matrix[%d][%d] ---------------------\n", squareSize, squareSize);
    		for (i=0; i<squareSize; i++) {
        		sprintf(bufff, "%d:", i);
        		nnn = 0;
        		for (j=0; j<squareSize; j++) {
            			nnn += sprintf(bufff+nnn, "\t%.4f", graph[i*squareSize+j]);
        		}
        		dbgprintf("%s", bufff);
    		}
	}
#endif
	for(i=0;i<totalTuples;i++){
		ksource=keyLookup(stdLookup,stdRevLookup,edges[i].source,&keyValues);
		if(keyValues >  MAX_POINT_VAL ){
			sprintf(errMesg,"Maximum support number of values %d user requested %d", MAX_POINT_VAL,keyValues);
			
			return false;
		}

		if(ksource == NO_PLACE ){
			sprintf(errMesg,"Failed to look up source %d", edges[i].source);
			return false;
		}
dbgprintf("Allocated key ---->>>%d size %d",keyValues,stdLookup.size());

		ktarget=keyLookup(stdLookup,stdRevLookup,edges[i].target,&keyValues);
		if(ktarget == NO_PLACE ){
			sprintf(errMesg,"Failed to look up source %d", edges[i].source);
			return false;
		}
dbgprintf("Target %d Source %d %.4f %.4f ",ksource,ktarget,edges[i].cost,edges[i].reverseCost);
		graph[(ksource*squareSize)+ktarget]= edges[i].cost;
		if(hasReverse){
			graph[(ktarget*squareSize)+ksource]= edges[i].reverseCost;
		} else {
			graph[(ktarget*squareSize)+ksource]= edges[i].cost;
		}
	}

	dbgprintf("Processing %d elements ",squareSize);

	
#ifdef DEBUG
	{
		
    		char bufff[2048];
    		int nnn;
    		dbgprintf("---------- REVLOOKUP total keys [%d]\n", keyValues);

		for(i=0;i< keyValues;i++){
			int ret;

			ret=getLookUp(stdRevLookup,  i);

			if(ret== NO_PLACE ){
				sprintf(errMesg,"Internal error can not find lookup for key %d",i);
				return false;
			} else {
				dbgprintf("Key %d Lookup %d",i,ret);
			}

		}

    		dbgprintf("---------- Matrix[%d][%d] ---------------------\n", squareSize, squareSize);
    		for (i=0; i<squareSize; i++) {
        		sprintf(bufff, "%d:", i);
        		nnn = 0;
        		for (j=0; j<squareSize; j++) {
            			nnn += sprintf(bufff+nnn, "\t%.4f", graph[i*squareSize+j]);
        		}
        		dbgprintf("%s", bufff);
    		}
	}
#endif
	/* check to see if the matrix was completed */
    	for (i=0; i<squareSize; i++) {
        	for (j=0; j<squareSize; j++) {
			if(graph[(i*squareSize)+j] == BAD_DATA){
				sprintf(errMesg,"Data not provided for point (%d,%d) ",i,j);
				return false;
			}
		}
	}
	/* Convert startVertex */
	kstartUp= getLookUp(stdLookup, startVertex);
dbgprintf("startVertex %d converted Kstartup is %d",startVertex,kstartUp);
	if(kstartUp == NO_PLACE ){
		sprintf(errMesg,"Can not lookup start values %d ",startVertex);
		return false;
	}

	tspPathAsmParamsType *data=computeTSP(graph,squareSize,errMesg);
	dbgprintf("Return length of path %d",data->outputCnt);
	/* Got a werid result */
	if(data->outputCnt <=0 ){
		sprintf(errMesg,"Unable to get a result");
		releaseResources(data);
		return false;
	}
	for(i=0;i<data->outputCnt;i++){
		// check to see if there are any bad results in the output
		// if so assume that the user supplied data was corrupt.
		if( data->outputArray[i] == BAD_DATA){
			sprintf(errMesg,"Corrupt data results ");
			releaseResources(data);
			return false;
		}
	}
	

	*path=mkPathData(data,stdRevLookup,squareSize,kstartUp);

	*pathCount=data->outputCnt;
        /* Free off all the allocate resources */
	releaseResources(data);

	if(path == NULL ){
		sprintf(errMesg,"Corrupt name value lookup");
		return false;
	}
	return true;
}
static void releaseResources( tspPathAsmParamsType *data ){
	releaseMem(data->graph);
        releaseMem(data->path);
	releaseMem(data->retCosts);
	releaseMem(data);
}
/**
 *  change the return data from the TSP engine into something(?) useful
 */
static tspPathElementType * mkPathData(tspPathAsmParamsType *data , 
	std::map<int,int> stdRevLookup,int squareSize,int kstartUp){
	int i;
	tspPathElementType * thePath;

	double * cost;
	int *vertexs;
	int startPlace NO_PLACE;
	cost=(double*) getMem(sizeof(double) * data->outputCnt);
	vertexs=(int*) getMem(sizeof(int) * data->outputCnt);

	thePath=(tspPathElementType*) getMem(data->outputCnt * sizeof(tspPathElementType));
	for(i=0;i<data->outputCnt;i++){

		if(kstartUp == data->outputArray[i] ){
			startPlace= i;
		}

		vertexs[i]=getLookUp(stdRevLookup,data->outputArray[i]);
		cost[i]=data->retCosts[i];

        	dbgprintf("%d Vertex %d cost %.4f", 
			i,thePath[i].vertexId,thePath[i].cost);
	}
        dbgprintf("StartPlace offset %d", startPlace);
	// reorder the output so that it appears to start from the requested 'vertex'
	for(i=0;i<data->outputCnt;i++){
		thePath[i].cost=cost[(startPlace+i)%data->outputCnt];
		thePath[i].vertexId=vertexs[(startPlace+i)%data->outputCnt];
        }
	releaseMem(cost);
	releaseMem(vertexs);
	return thePath;

}

static int keyLookup(std::map<int,int> & lookup,std::map<int,int> & revLookup, int value,int *suppliedKey){

	
	int ret=NO_PLACE;

	dbgprintf("user key value %d map value %d lookupSize %d",value,*suppliedKey,lookup.size());
	try {
		ret=lookup.at(value);

	} catch (const std:: out_of_range & oor){
		ret=NO_PLACE;
	}
	if(ret == NO_PLACE  ){
		dbgprintf("Mapping Value %d --> %d",value,*suppliedKey);
		revLookup[*suppliedKey]=value;
		lookup[value]= *suppliedKey;
		ret=  lookup[value];
		(*suppliedKey)++;

	}else {
		dbgprintf("Already mapped %d --> %d",value,ret);

	}

	return ret;
}

/**
 * check to see if this key exists
 */
static int getLookUp(std::map<int,int>  &lookup, int value){

	int ret =NO_PLACE;
	try {
		ret=lookup.at(value);
	} catch (const std:: out_of_range & oor){
		ret=NO_PLACE;
	}
	return ret;
}
