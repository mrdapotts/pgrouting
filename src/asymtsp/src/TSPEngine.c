#include <stdio.h>
#include <stdlib.h>
#include <postgres.h>  /* postgres */

#include <search.h>    /* hash search */
#include <string.h>    /* memcpy */
#include <math.h>      /* exp    */

#include "tsp.h"

#ifdef DEBUG
#define DBG(format, arg...)                     \
    elog(NOTICE, format , ## arg)
#else
#define DBG(format, arg...) do { ; } while (0)
#endif

/* 
 * Taken from a paper by Aditya Nugroho
 * See 
 * http://www.scribd.com/doc/50049931/Final-Report-Solving-Traveling-Salesman-Problem-by-Dynamic-Programming-Approach-in-Java-Program-Aditya-Nugroho-Ht083276e
 *
 * REwritten in to C by muggins
 * Moved all globals in per call structure.
 * changed matrix from int to double
 */

static int 	tsp(    int , dataType ,tspPathAsmParamsType *);
static void 	getPath(int , dataType ,tspPathAsmParamsType  *) ;
//static void 	dumpMatrix(int  * matrix ,int size,int unitWidth);

tspPathAsmParamsType * computeTSP(double * inputArray, int size,char *errMesg) {
		tspPathAsmParamsType *data= palloc(sizeof( tspPathAsmParamsType ));
		int i,j;
		data->size = size;

#ifdef DEBUG
	{
		dataType dval;
    		char bufff[2048];
    		int nnn;
		DBG("Size of data item %lu",sizeof(dval));


    		DBG("Start ------ Matrix[%d][%d] ---------------------", size, size);
    		for (i=0; i<size; i++) {
        		sprintf(bufff, "%d:", i);
        		nnn = 0;
        		for (j=0; j<size; j++) {
            			nnn += sprintf(bufff+nnn, "\t%.4f", inputArray[i*size+j]);
        		}
        		DBG("%s", bufff);
    		}
	}
#endif

		data->retCosts= palloc(sizeof(int)*(size));
		data->outputArray= palloc(sizeof(int)*(size+1));
		data->outputCnt=0;

		for(i=0;i<size;i++){
			data->outputArray[i]= -1;
		}
		
		data->npow = (int) pow(2, data->size);
		DBG("pow is %d",data->npow);
		data->graph = palloc(sizeof(double)*data->size*data->npow);
		data->path = palloc(sizeof(int)*data->size*data->npow);
		data->inputData = inputArray;

		for (i = 0; i < data->size; i++) {
			for (j = 0; j < data->npow; j++) {
				data->graph[(i*data->npow)+j] = -1;
				data->path[(i*data->npow)+j] = -1;
			}
		} 

		//initialize based on distance matrix
		for (i = 0; i < data->size; i++) {
			data->graph[(i*data->npow)+0] =inputArray[(i*data->size)+0];
		}

		int result = tsp(0, data->npow - 2,data);

		// starting point always zero
		data->outputArray[0]=0;
		data->outputCnt=1;
		
		getPath(0, data->npow - 2,data);

		data->outputArray[data->outputCnt]=result;
		data->outputCnt++;
  
		
		for(i=0;i<data->outputCnt-1;i++){
			data->retCosts[i]= inputArray[ (data->outputArray[i]*size)+ data->outputArray[(i+1)%data->size]];
			DBG("%d Value %d [%d-> %d] %.4f",i,data->outputArray[i],
				data->outputArray[i],
				data->outputArray[(i+1)%data->size],
				data->retCosts[i]);
		}
		
		data->outputCnt--;
		
		return data;
}
	
static int tsp(int start, dataType set, tspPathAsmParamsType *  data ) {

	dataType masked, mask;
	int  result = -1, temp;
	int x;

	if (data->graph[(start*data->npow)+set] != -1) {

		return data->graph[(start*data->npow)+set];
	} else {
		for ( x = 0; x < data->size; x++) {
			mask = data->npow - 1 - (dataType) pow(2, x);
			masked = set & mask;
			if (masked != set) {

				int tmp = tsp(x, masked,data);

				temp = data->inputData[(start*data->size)+x] + tmp;

				if (result == -1 || result > temp) {
					result = temp;
					data->path[(start*data->npow)+set] = x;
				}
			}
		}
		data->graph[(start*data->npow)+set] = result;
		return result;
	}
}

static void getPath(int start, dataType set,  tspPathAsmParamsType *data) {

		DBG("getPath:Start %d set %llu ",start,set);

		if (data->path[(start*data->npow)+set] == -1) {
			return;
		}
		int x = data->path[(start*data->npow)+set];

		dataType mask = data->npow - 1 - (int) pow(2, x);
		int masked = set & mask;

		data->outputArray[data->outputCnt]=x;
		data->outputCnt++;
		getPath(x, masked,data);
}
/*
static void dumpMatrix(int  * matrix ,int size,int unitWidth){
#ifdef DEBUG
	{
		int i,j;		
    		char bufff[2048];
    		int nnn;

    		DBG("------ Matrix[%d][%d] ---------------------", size, unitWidth);
    		for (i=0; i<size; i++) {
        		sprintf(bufff, "%d:", i);
        		nnn = 0;
        		for (j=0; j<unitWidth; j++) {
            			nnn += sprintf(bufff+nnn, "\t%d", matrix[(i*unitWidth)+j]);
        		}
        		DBG("%s", bufff);
    		}
	}
#endif
}
*/
