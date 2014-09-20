--
-- Copyright (c) 2013 Stephen Woodbridge
--
-- This files is released under an MIT-X license.
--


-----------------------------------------------------------------------
-- Core function for TSP
-----------------------------------------------------------------------
/*
 * select seq, id from pgr_sql_asm_tsp(sql text, start int,has_reverse_cost)
 *
 *            sql text returns a list of source, target, cost and reverse_cost
 *            start   starting point
 *            has_reverse_cost set the link has a reverse_cost
 *         
 * Error is throw is source--> target and target-->source link is not 
 * not provided when reverse_cost is set
 * if reverse_cost is set an error is throw in source--> target is not the same
 * as target-->source, if this value is provided.
 * Error is throw source is the same as target
 */
CREATE OR REPLACE FUNCTION pgr_sql_asm_tsp(sql text , startpt integer, has_reverse_cost boolean)
    RETURNS SETOF pgr_costResult
    AS '$libdir/librouting_atsp', 'sql_asm_tsp'
    LANGUAGE c IMMUTABLE STRICT;

