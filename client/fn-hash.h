/*
 * frontier hash table header
 * 
 * Author: Dave Dykstra
 *
 * $Id$
 *
 *  Copyright (C) 2008  Fermilab
 *
 *  This program is free software: you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation, either version 3 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */
#ifndef __HEADER_H_FN_HASH_H
#define __HEADER_H_FN_HASH_H

/* in order to avoid symbol clashes, re-define external symbols
   in this off-the-net hashtable package */

/* these may be used directly */
#define hashtable_destroy fn_hashtable_destroy
#define hashtable_count fn_hashtable_count
#define hashtable_stats fn_hashtable_stats
#define hashtable s_fn_hashtable
typedef struct s_fn_hashtable fn_hashtable;

/* don't use these directly in frontier, hint is they have double underscore */
#define create_hashtable fn__create_hashtable
#define hashtable_insert fn__hashtable_insert
#define hashtable_search fn__hashtable_search
#define hashtable_remove fn__hashtable_remove
#define hashtable_hash fn__hashtable_hash
#define hashtable_iterator fn__hashtable_iterator
#define hashtable_iterator_key fn__hashtable_iterator_key
#define hashtable_iterator_value fn__hashtable_iterator_value
#define hashtable_iterator_advance fn__hashtable_iterator_advance
#define hashtable_iterator_remove fn__hashtable_iterator_remove
#define hashtable_iterator_search fn__hashtable_iterator_search
#define hashtable_change fn__hashtable_change

#include "chashtable/hashtable.h"

struct s_fn_hashval
 {
  int len;
  char *data;
 };
typedef struct s_fn_hashval fn_hashval;

/* the functions these define also may be used directly */
int fn_hashtable_insert(fn_hashtable *h,char *key,fn_hashval *val);
fn_hashval *fn_hashtable_search(fn_hashtable *h,char *key);

extern fn_hashtable *fn_inithashtable();

#endif /*__HEADER_H_FN_HASH_H*/
