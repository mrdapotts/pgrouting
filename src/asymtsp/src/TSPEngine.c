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

struct t_asm_params{
	int * outputArray;
	int outputCnt;
	int *g;
	int  *p;
	int npow;
	int N; 
	double *d;
} ;


static int 	tsp(int , int ,struct t_asm_params *);
static void 	getPath(int , int ,struct t_asm_params *) ;
#ifdef BOLAX
int main(int argc, char * argv[]) {
	double graph[]={0.0,1.0,2.0,6.0,0.0,3.0,5.0,4.0,0.0};
	int i;

	

	int * data =computeTSP(graph, 3) ;

  	for(i=0;i<4;i++){
		fprintf(stderr,"D %d\n",data[i]);
	}   
}
#endif // BOLAX
int *	computeTSP(double * inputArray, int n,int startVertex,int *pathCount,char **errMsg) {
		struct t_asm_params *data= palloc(sizeof(struct t_asm_params));
		int * outputArray;
		int   k, l, m, s;
		int i,j;
		int * out;
#ifdef DEBUG
	{
		
    		char bufff[2048];
    		int nnn;

    		DBG("Start Vertex [%d]------ Matrix[%d][%d] ---------------------\n", startVertex,n, n);
    		for (i=0; i<n; i++) {
        		sprintf(bufff, "%d:", i);
        		nnn = 0;
        		for (j=0; j<n; j++) {
            			nnn += sprintf(bufff+nnn, "\t%.4f", inputArray[i*n+j]);
        		}
        		DBG("%s", bufff);
    		}
	}
#endif
		out= palloc(sizeof(int)*(n+1));
		data->outputArray= palloc(sizeof(int)*(n+1));
		data->outputCnt=0;



		DBG("Dealing with %d",n);

		for(i=0;i<n;i++){
			data->outputArray[i]= -1;
		}
		data->N = n;
		
		data->npow = (int) pow(2, n);

		data->g = palloc(sizeof(int)*n*data->npow);
		data->p = palloc(sizeof(int)*n*data->npow);
		data->d = inputArray;

		for (i = 0; i < n; i++) {
			for (j = 0; j < data->npow; j++) {
				data->g[(i*data->npow)+j] = -1;
				data->p[(i*data->npow)+j] = -1;
			}
		} 

		//initialize based on distance matrix
		for (i = 0; i < n; i++) {
			data->g[(i*data->npow)+0] = inputArray[(i*data->N)+0];
		}

 
		int result = tsp(0, data->npow - 2,data);
		// starting point always zero
		data->outputArray[0]=0;
		data->outputCnt=1;
		
		getPath(0, data->npow - 2,data);
/*
		data->outputArray[data->outputCnt]=result;
		data->outputCnt++;
*/
		outputArray=data->outputArray;
		for(i=0;i<n;i++){
			if(outputArray[i]== startVertex){
				DBG("breaking on %d",startVertex);
				break;
			}
		}
		for(j=0;j<n;j++){
			DBG("Rewritting on  %d %d ",j,(i+j)%n);
			out[j]=outputArray[(i+j)%n];
		}
		*pathCount=data->outputCnt;
		pfree(data->g);
		pfree(data->p);
		pfree(data);
		return out;
}
	
int tsp(int start, int set, struct t_asm_params * data ) {

	int masked, mask, result = -1, temp;
	int x;
printf("TSP:Start %d set %d \n",start,set);
	if (data->g[(start*data->npow)+set] != -1) {
printf("TSP:Not -1  %d %d %d \n",start,set,(start*data->npow)+set);
		return data->g[(start*data->npow)+set];
	} else {
		for ( x = 0; x < data->N; x++) {
			mask = data->npow - 1 - (int) pow(2, x);
			masked = set & mask;
			if (masked != set) {
				temp = data->d[(start*data->N)+x] + tsp(x, masked,data);
				if (result == -1 || result > temp) {
					result = temp;
					data->p[(start*data->npow)+set] = x;
				}
			}
		}
		data->g[(start*data->npow)+set] = result;
		return result;
	}
}

void getPath(int start, int set,struct t_asm_params  *data) {
printf("getPath:Start %d set %d \n",start,set);
		if (data->p[(start*data->npow)+set] == -1) {
			return;
		}
		int x = data->p[(start*data->npow)+set];
		int mask = data->npow - 1 - (int) pow(2, x);
		int masked = set & mask;

		data->outputArray[data->outputCnt]=x;
		data->outputCnt++;
		getPath(x, masked,data);
}
