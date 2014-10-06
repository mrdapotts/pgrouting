/*
 * Traveling Salesman Problem solution algorithm for PostgreSQL
 *
 * Copyright (c) 2006 Anton A. Patrushev, Orkney, Inc.
 * Copyright (c) 2013 Stephen Woodbridge, iMaptools.com.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#define _TSP_H

#ifdef __cplusplus
extern "C"
{
#endif
// input
typedef struct tspEdge {
            int id;
            int source;
            int target;
            double cost;
	    double reverseCost;
} tspEdgeType;
// output    
typedef struct tspPathElement
{
            int seq;
            int vertexId;
            double cost;
} tspPathElementType;

typedef struct tAsmParams{
	int * outputArray;
	int outputCnt;
	double *graph;
	int  *path;
	int npow;
	int size; 
	double *inputData;
	double *retVetex;
	double *retCosts;
} tspPathAsmParamsType;

bool processATSPData(tspEdgeType  * edges,int totalTuples,int  startVertex,bool has_reverse, tspPathElementType **path, int *pathCount, char * errMesg);

tspPathAsmParamsType * computeTSP(double * inputArray, int n,char *errMesg) ;

void * getMem(size_t request );
void releaseMem(void *mem );
void dbgprintf(const char *str,...);

typedef unsigned  long long dataType;
#ifdef __cplusplus
}
#endif
