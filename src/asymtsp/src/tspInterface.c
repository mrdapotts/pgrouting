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

static bool fetchEdgeTspColumns(SPITupleTable *tuptable, tspEdgeType *edgeColumns,bool reverseCost) ;


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
  	char *out = (char *)getMem(VARSIZE(in));

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
 * Gets the raw data from the database and populates the edgeColumns 
 */
static bool fetchEdgeTspColumns(SPITupleTable *tuptable, tspEdgeType *edgeColumns,bool reverseCost) {

	edgeColumns->source = SPI_fnumber(SPI_tuptable->tupdesc, "source");
  	edgeColumns->target = SPI_fnumber(SPI_tuptable->tupdesc, "target");
  	edgeColumns->cost = SPI_fnumber(SPI_tuptable->tupdesc, "cost");
	if(reverseCost){
  		edgeColumns->reverseCost = SPI_fnumber(SPI_tuptable->tupdesc, "reverse_cost");

	}

  	if ( edgeColumns->source == SPI_ERROR_NOATTRIBUTE ){
      		elog(ERROR, "Error, query must include 'source' column ");
		return false;
	}
      	if( edgeColumns->target == SPI_ERROR_NOATTRIBUTE ){
      		elog(ERROR, "Error, query must include 'target' column ");
		return false;
	}

      	if( edgeColumns->cost == SPI_ERROR_NOATTRIBUTE) {
      		elog(ERROR, "Error, query must include 'cost' column ");
		return false;
	}

      	if( reverseCost && edgeColumns->reverseCost == SPI_ERROR_NOATTRIBUTE) {

      		elog(ERROR, "Error, query must include  'reverse_cost' column");
      		return false;
  	}

  	if (SPI_gettypeid(SPI_tuptable->tupdesc, edgeColumns->source) != INT4OID ||
      		SPI_gettypeid(SPI_tuptable->tupdesc, edgeColumns->target) != INT4OID ){

      		elog(ERROR, "Error, columns 'source', 'target' must be of type int4  ");
      		return false;
  	}
  	int type=  SPI_gettypeid(SPI_tuptable->tupdesc, edgeColumns->cost);
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
	
	if(reverseCost ){
  		int type=  SPI_gettypeid(SPI_tuptable->tupdesc, edgeColumns->cost);
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
	if(reverseCost){
  		DBG("columns:  source %i target %i cost %.4f reverse_cost %.4f",
      			edgeColumns->source,
      			edgeColumns->target, edgeColumns->cost,edgeColumns->reverseCost);
	} else {
  		DBG("columns:  source %i target %i cost %.4f",
      			edgeColumns->source,
      			edgeColumns->target, edgeColumns->cost);
	}

  	return true;
} 
/**
 * Get the data from the tuple
 */
static void fetchEdgeTsp(HeapTuple *tuple, TupleDesc *tupdesc,
         tspEdgeType *edgeColumns,
         tspEdgeType *target_edge,bool reverseCost) {
	Datum binval;
  	bool isnull;

  	binval = SPI_getbinval(*tuple, *tupdesc, edgeColumns->id, &isnull);
  	if (isnull) {
		elog(ERROR, "id contains a null value");
	}
  	target_edge->id = DatumGetInt32(binval);

  	binval = SPI_getbinval(*tuple, *tupdesc, edgeColumns->source, &isnull);
  	if (isnull) {
		elog(ERROR, "source contains a null value");
	}
  	target_edge->source = DatumGetInt32(binval);

  	binval = SPI_getbinval(*tuple, *tupdesc, edgeColumns->target, &isnull);
  	if (isnull) {
		elog(ERROR, "target contains a null value");
	}
  	target_edge->target = DatumGetInt32(binval);

  	binval = SPI_getbinval(*tuple, *tupdesc, edgeColumns->cost, &isnull);
  	
	if (isnull) {
		elog(ERROR, "cost contains a null value");
	}
  	target_edge->cost = DatumGetFloat8(binval);

  	if(reverseCost ) {
		binval = SPI_getbinval(*tuple, *tupdesc, edgeColumns->reverseCost, &isnull);
  	
		if (isnull) {
			elog(ERROR, "reverse_cost contains a null value");
		}
  		target_edge->reverseCost = DatumGetFloat8(binval);
	}

}

static int compute_sql_asm_tsp(char* sql, int sourceVertexId,
                       bool reverseCost, 
                       tspPathElementType **path, int *pathCount) {
	int SPIcode;
  	void *SPIplan;
  	Portal SPIportal;
  	bool moredata = TRUE;
  	int ntuples;
  	tspEdgeType *edges = NULL;
  	int totalTuples = 0;

	DBG("Sql %s source %d reverse %s",sql,sourceVertexId,reverseCost==true?"true":"false");

  	tspEdgeType edgeColumns = {.id= -1, .source= -1, .target= -1, .cost= -1 };
  	char *errMesg;
  	int ret = -1;
  	errMesg=palloc(sizeof(char) * 300);	

  	DBG("start compute_sql_asm_tsp %i",*pathCount);

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

      		if (edgeColumns.id == -1) {
          		if (!fetchEdgeTspColumns(SPI_tuptable, &edgeColumns,reverseCost))
            			return finish(SPIcode, ret);
      		}

      		ntuples = SPI_processed;
      		totalTuples += ntuples;
      		if (!edges){
        		edges = palloc(totalTuples * sizeof(tspEdgeType));
      		} else {
        		edges = repalloc(edges, totalTuples * sizeof(tspEdgeType));
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
              			fetchEdgeTsp(&tuple, &tupdesc, &edgeColumns, &edges[totalTuples - ntuples + t],reverseCost);
          		}
          		SPI_freetuptable(tuptable);
      		} else {
          		moredata = FALSE;
      		}
  	}


  	DBG("Total %i tuples", totalTuples);
  	DBG("Calling tsp functions total tuples <%i> initial path count <%i>", totalTuples,*pathCount);
	
	ret=processATSPData(edges,totalTuples,sourceVertexId,reverseCost, path, pathCount,errMesg);

  	DBG("SIZE %i elements to process",*pathCount);

  	if (!ret ) {
      		elog(ERROR, "Error computing path: %s", errMesg);
  	}
  	return finish(SPIcode, ret);
}


PG_FUNCTION_INFO_V1(sql_asm_tsp);
Datum sql_asm_tsp(PG_FUNCTION_ARGS) {
  	FuncCallContext     *funcctx;
  	int                  callCntr;
  	int                  maxCalls;
  	TupleDesc            tupleDesc;
  	tspPathElementType   *path;

  	/* stuff done only on the first call of the function */
  	if (SRF_IS_FIRSTCALL()) {
      		MemoryContext   oldcontext;
      		int pathCount = 0;
      		int ret;

      		/* create a function context for cross-call persistence */
      		funcctx = SRF_FIRSTCALL_INIT();

      		/* switch to memory context appropriate for multiple function calls */
      		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

      		ret = compute_sql_asm_tsp(text2char(PG_GETARG_TEXT_P(0)),
                	PG_GETARG_INT32(1), PG_GETARG_BOOL(2), &path, &pathCount);


#ifdef DEBUG
      		if (ret >= 0) {
          		int i;
          		for (i = 0; i < pathCount; i++) {
              			DBG("Step # %i vertexId  %i cost %.4f", i, path[i].vertexId,path[i].cost);
          		}
      		}
#endif

      		/* total number of tuples to be returned */
      		funcctx->max_calls = pathCount;
      		funcctx->user_fctx = path;

      		DBG("Path count %i", pathCount);

      		funcctx->tuple_desc =
            		BlessTupleDesc(RelationNameGetTupleDesc("pgr_costResult"));

      		MemoryContextSwitchTo(oldcontext);
  	}

  	funcctx = SRF_PERCALL_SETUP();
  	callCntr = funcctx->call_cntr;
  	maxCalls = funcctx->max_calls;
  	tupleDesc = funcctx->tuple_desc;
  	path = (tspPathElementType*) funcctx->user_fctx;

  	if (callCntr < maxCalls) {   /* do when there is more left to send */
      		HeapTuple    tuple;
      		Datum        result;
      		Datum *values;
      		char* nulls;

      		values = palloc(4 * sizeof(Datum));
      		nulls = palloc(4 * sizeof(char));

      		values[0] = Int32GetDatum(callCntr);
      		nulls[0] = ' ';
      		values[1] = Int32GetDatum(path[callCntr].vertexId);
      		nulls[1] = ' ';
      		values[2] = Float8GetDatum(0); // edge id not supplied by this method
      		nulls[2] = ' ';
      		values[3] = Float8GetDatum(path[callCntr].cost);
      		nulls[3] = ' ';

      		tuple = heap_formtuple(tupleDesc, values, nulls);

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
