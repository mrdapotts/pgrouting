/*
 * TSP with an via an sql string
 *
 * Copyright (c) Dave Potts
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "postgres.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "catalog/pg_type.h"
#if PGSQL_VERSION > 92
#include "access/htup_details.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <search.h>
#include "tsp.h"


Datum sql_asm_tsp(PG_FUNCTION_ARGS);

static bool fetch_edge_tsp_columns(SPITupleTable *tuptable, tsp_edge_t *edge_columns,bool reverse_cost) ;


#ifdef DEBUG
#define DBG(format, arg...)                     \
    elog(NOTICE, format , ## arg)
#else
#define DBG(format, arg...) do { ; } while (0)
#endif

// The number of tuples to fetch from the SPI cursor at each iteration
#define TUPLIMIT 1000

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

static char * text2char(text *in) {
  	char *out = palloc(VARSIZE(in));

  	memcpy(out, VARDATA(in), VARSIZE(in) - VARHDRSZ);
  	out[VARSIZE(in) - VARHDRSZ] = '\0';
  	return out;
}

static int finish(int code, int ret) {
  	code = SPI_finish();
  	if (code  != SPI_OK_FINISH ) {
      		elog(ERROR,"couldn't disconnect from SPI");
      		return -1 ;
  	}

  	return ret;
}
/**
 * Gets the raw data from the database and populates the edge_columns 
 */
static bool fetch_edge_tsp_columns(SPITupleTable *tuptable, tsp_edge_t *edge_columns,bool reverse_cost) {

	edge_columns->source = SPI_fnumber(SPI_tuptable->tupdesc, "source");
  	edge_columns->target = SPI_fnumber(SPI_tuptable->tupdesc, "target");
  	edge_columns->cost = SPI_fnumber(SPI_tuptable->tupdesc, "cost");
	if(reverse_cost){
  		edge_columns->reverse_cost = SPI_fnumber(SPI_tuptable->tupdesc, "reverse_cost");

	}

  	if ( edge_columns->source == SPI_ERROR_NOATTRIBUTE ){
      		elog(ERROR, "Error, query must include 'source' column ");
		return false;
	}
      	if( edge_columns->target == SPI_ERROR_NOATTRIBUTE ){
      		elog(ERROR, "Error, query must include 'target' column ");
		return false;
	}

      	if( edge_columns->cost == SPI_ERROR_NOATTRIBUTE) {
      		elog(ERROR, "Error, query must include 'cost' column ");
		return false;
	}

      	if( reverse_cost && edge_columns->reverse_cost == SPI_ERROR_NOATTRIBUTE) {

      		elog(ERROR, "Error, query must include  'reverse_cost' column");
      		return false;
  	}

  	if (SPI_gettypeid(SPI_tuptable->tupdesc, edge_columns->source) != INT4OID ||
      		SPI_gettypeid(SPI_tuptable->tupdesc, edge_columns->target) != INT4OID ){

      		elog(ERROR, "Error, columns 'source', 'target' must be of type int4  ");
      		return false;
  	}
  	int type=  SPI_gettypeid(SPI_tuptable->tupdesc, edge_columns->cost);
  	switch ( type){
  		case INT2OID:
  		case INT4OID:
  		case FLOAT4OID:
  		case FLOAT8OID:
  		case TIDOID:
       			break;
  		default:
      			elog(ERROR, "Unknown type for 'cost' column %d",type);
      			return false;
  	}
	
	if(reverse_cost ){
  		int type=  SPI_gettypeid(SPI_tuptable->tupdesc, edge_columns->cost);
  		switch ( type){
  			case INT2OID:
  			case INT4OID:
  			case FLOAT4OID:
  			case FLOAT8OID:
  			case TIDOID:
       				break;
  			default:
      				elog(ERROR, "Unknown type for 'reverse_cost' column %d",type);
      			return false;
		}
  	}
	if(reverse_cost){
  		DBG("columns:  source %i target %i cost %.4f reverse_cost %.4f",
      			edge_columns->source,
      			edge_columns->target, edge_columns->cost,edge_columns->reverse_cost);
	} else {
  		DBG("columns:  source %i target %i cost %.4f",
      			edge_columns->source,
      			edge_columns->target, edge_columns->cost);
	}

  	return true;
} 
/**
 * Get the data from the tuple
 */
static void fetch_edge_tsp(HeapTuple *tuple, TupleDesc *tupdesc,
         tsp_edge_t *edge_columns,
         tsp_edge_t *target_edge,bool reverse_cost) {
	Datum binval;
  	bool isnull;

  	binval = SPI_getbinval(*tuple, *tupdesc, edge_columns->id, &isnull);
  	if (isnull) {
		elog(ERROR, "id contains a null value");
	}
  	target_edge->id = DatumGetInt32(binval);

  	binval = SPI_getbinval(*tuple, *tupdesc, edge_columns->source, &isnull);
  	if (isnull) {
		elog(ERROR, "source contains a null value");
	}
  	target_edge->source = DatumGetInt32(binval);

  	binval = SPI_getbinval(*tuple, *tupdesc, edge_columns->target, &isnull);
  	if (isnull) {
		elog(ERROR, "target contains a null value");
	}
  	target_edge->target = DatumGetInt32(binval);

  	binval = SPI_getbinval(*tuple, *tupdesc, edge_columns->cost, &isnull);
  	
	if (isnull) {
		elog(ERROR, "cost contains a null value");
	}
  	target_edge->cost = DatumGetFloat8(binval);

  	if(reverse_cost ) {
		binval = SPI_getbinval(*tuple, *tupdesc, edge_columns->reverse_cost, &isnull);
  	
		if (isnull) {
			elog(ERROR, "reverse_cost contains a null value");
		}
  		target_edge->reverse_cost = DatumGetFloat8(binval);
	}

}

static int compute_sql_asm_tsp(char* sql, int source_vertex_id,
                       bool reverse_cost, 
                       tsp_path_element_t **path, int *path_count) {
	int SPIcode;
  	void *SPIplan;
  	Portal SPIportal;
  	bool moredata = TRUE;
  	int ntuples;
  	tsp_edge_t *edges = NULL;
  	int total_tuples = 0;

	DBG("Sql %s source %d reverse %s",sql,source_vertex_id,reverse_cost==true?"true":"false");

  	tsp_edge_t edge_columns = {.id= -1, .source= -1, .target= -1, .cost= -1 };
  	char *err_msg;
  	int ret = -1;
  	int z;

  	DBG("start compute_sql_asm_tsp %i",*path_count);

  	SPIcode = SPI_connect();
  	if (SPIcode  != SPI_OK_CONNECT) {
      		elog(ERROR, "compute_sql_asm_tsp: couldn't open a connection to SPI");
      		return -1;
  	}

  	SPIplan = SPI_prepare(sql, 0, NULL);
  	if (SPIplan  == NULL) {
      		elog(ERROR, "compute_sql_asm_tsp: couldn't create query plan via SPI");
      		return -1;
  	}

  	if ((SPIportal = SPI_cursor_open(NULL, SPIplan, NULL, NULL, true)) == NULL) {
      		elog(ERROR, "compute_sql_asm_tsp: SPI_cursor_open('%s') returns NULL", sql);
      		return -1;
  	}

  	while (moredata == TRUE) {
      		SPI_cursor_fetch(SPIportal, TRUE, TUPLIMIT);

      		if (edge_columns.id == -1) {
          		if (!fetch_edge_tsp_columns(SPI_tuptable, &edge_columns,reverse_cost))
            			return finish(SPIcode, ret);
      		}

      		ntuples = SPI_processed;
      		total_tuples += ntuples;
      		if (!edges){
        		edges = palloc(total_tuples * sizeof(tsp_edge_t));
      		} else {
        		edges = repalloc(edges, total_tuples * sizeof(tsp_edge_t));
		}
      		if (edges == NULL) {
          		elog(ERROR, "Out of memory");
          		return finish(SPIcode, ret);
      		}

      		if (ntuples > 0) {
          		int t;
          		SPITupleTable *tuptable = SPI_tuptable;
          		TupleDesc tupdesc = SPI_tuptable->tupdesc;

          		for (t = 0; t < ntuples; t++) {
              			HeapTuple tuple = tuptable->vals[t];
              			fetch_edge_tsp(&tuple, &tupdesc, &edge_columns, &edges[total_tuples - ntuples + t],reverse_cost);
          		}
          		SPI_freetuptable(tuptable);
      		} else {
          		moredata = FALSE;
      		}
  	}


  	DBG("Total %i tuples", total_tuples);
  	DBG("Calling tsp functions <%i><%i>", total_tuples,*path_count);

	ret=processATSPData(edges,total_tuples,source_vertex_id,reverse_cost, path, path_count,&err_msg);

  	DBG("SIZE %i elements to process",*path_count);

  	if (ret < 0) {
      		elog(ERROR, "Error computing path: %s", err_msg);
  	}
  	return finish(SPIcode, ret);
}


PG_FUNCTION_INFO_V1(sql_asm_tsp);
Datum sql_asm_tsp(PG_FUNCTION_ARGS) {
  	FuncCallContext     *funcctx;
  	int                  call_cntr;
  	int                  max_calls;
  	TupleDesc            tuple_desc;
  	tsp_path_element_t      *path;

  	/* stuff done only on the first call of the function */
  	if (SRF_IS_FIRSTCALL()) {
      		MemoryContext   oldcontext;
      		int path_count = 0;
      		int ret;

      		/* create a function context for cross-call persistence */
      		funcctx = SRF_FIRSTCALL_INIT();

      		/* switch to memory context appropriate for multiple function calls */
      		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

      		ret = compute_sql_asm_tsp(text2char(PG_GETARG_TEXT_P(0)),
                	PG_GETARG_INT32(1), PG_GETARG_BOOL(2), &path, &path_count);


#ifdef DEBUG
      		if (ret >= 0) {
          		int i;
          		for (i = 0; i < path_count; i++) {
              			DBG("Step # %i vertex_id  %i ", i, path[i].vertex_id);
              			DBG("        cost       %f ", path[i].cost);
          		}
      		}
#endif

      		/* total number of tuples to be returned */
      		DBG("Conting tuples number");
      		funcctx->max_calls = path_count;
      		funcctx->user_fctx = path;

      		DBG("Path count %i", path_count);

      		funcctx->tuple_desc =
            		BlessTupleDesc(RelationNameGetTupleDesc("pgr_costResult"));

      		MemoryContextSwitchTo(oldcontext);
  	}

  	funcctx = SRF_PERCALL_SETUP();
  	call_cntr = funcctx->call_cntr;
  	max_calls = funcctx->max_calls;
  	tuple_desc = funcctx->tuple_desc;
  	path = (tsp_path_element_t*) funcctx->user_fctx;

  	if (call_cntr < max_calls) {   /* do when there is more left to send */
      		HeapTuple    tuple;
      		Datum        result;
      		Datum *values;
      		char* nulls;

      		values = palloc(4 * sizeof(Datum));
      		nulls = palloc(4 * sizeof(char));

      		values[0] = Int32GetDatum(call_cntr);
      		nulls[0] = ' ';
      		values[1] = Int32GetDatum(path[call_cntr].vertex_id);
      		nulls[1] = ' ';
      		values[2] = Float8GetDatum(0); // edge id not supplied by this method
      		nulls[2] = ' ';
      		values[3] = Float8GetDatum(path[call_cntr].cost);
      		nulls[3] = ' ';

      		tuple = heap_formtuple(tuple_desc, values, nulls);

      		/* make the tuple into a datum */
      		result = HeapTupleGetDatum(tuple);

      		/* clean up (this is not really necessary) */
      		pfree(values);
      		pfree(nulls);

      		SRF_RETURN_NEXT(funcctx, result);
  	} else {   /* do when there is no more left */
      		SRF_RETURN_DONE(funcctx);
  	}
}
/**
 * Wrapper functions for memory allocation.  It seems its impossible to access
 * palloc/pfree without running in to problems with the name managler 
 */
void * getMem(size_t request ){
	return palloc(request);
}
void releaseMem(void *mem ){
	pfree (mem);
}
