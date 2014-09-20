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

// define the type of object for the distance matrix


#ifdef __cplusplus
extern "C"
{
#endif
// input
typedef struct tsp_edge {
            int id;
            int source;
            int target;
            double cost;
	    double reverse_cost;
} tsp_edge_t;
// output    
typedef struct tsp_path_element
{
            int seq;
            int vertex_id;
            double cost;
} tsp_path_element_t;

bool processATSPData(tsp_edge_t  * edges,int total_tuples,int  start_vertex,bool has_reverse, tsp_path_element_t **path, int *path_count, char ** err_msg);

int *  computeTSP(double * inputArray, int n,int startVertex,int *pathCount,char **errMesg) ;
#ifdef __cplusplus
}
#endif
