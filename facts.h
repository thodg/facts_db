/*
 *  facts_db - in-memory graph database
 *  Copyright 2020 Thomas de Grivel <thoxdg@gmail.com>
 *
 *  Permission to use, copy, modify, and distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef FACTS_H
#define FACTS_H

#include <stdio.h>
#include "binding.h"
#include "fact.h"
#include "set.h"
#include "skiplist.h"
#include "spec.h"

#define FACTS_SET_INITIAL_SIZE 1000000
#define FACTS_SKIPLIST_SPACING 2.0
#define FACTS_LOAD_BUFSZ (1024 * 1024)

typedef struct facts
{
        s_set set;
        s_skiplist *index_spo;
        s_skiplist *index_pos;
        s_skiplist *index_osp;
} s_facts;

void              facts_init (s_facts *facts, unsigned long max);

void              facts_destroy (s_facts *facts);

s_facts *     new_facts (unsigned long max);

void       delete_facts (s_facts *facts);

const char *      facts_intern (s_facts *facts,
                                const char *string);

s_fact *          facts_add_fact (s_facts *facts,
                                  s_fact *f);

s_fact *          facts_add_spo (s_facts *facts,
                                 const char *s,
                                 const char *p,
                                 const char *o);

int               facts_remove_fact (s_facts *facts,
                                     s_fact *f);

int               facts_remove_spo (s_facts *facts,
                                    const char *s,
                                    const char *p,
                                    const char *o);

s_fact *          facts_get_fact (s_facts *facts,
                                  s_fact *f);

unsigned long     facts_count (s_facts *facts);

typedef struct facts_cursor {
        s_skiplist *tree;
        s_skiplist_node *node;
        s_fact start;
        s_fact end;
        const char **var_s;
        const char **var_p;
        const char **var_o;
} s_facts_cursor;

void              facts_cursor_init (s_facts *facts,
                                     s_facts_cursor *c,
                                     s_skiplist *tree,
                                     s_fact *start,
                                     s_fact *end);

s_fact *          facts_cursor_next (s_facts_cursor *c);

void              facts_with_3 (s_facts *facts,
                                s_facts_cursor *c,
                                const char *s,
                                const char *p,
                                const char *o);

void              facts_with_0 (s_facts *facts,
                                s_facts_cursor *c,
                                const char **var_s,
                                const char **var_p,
                                const char **var_o);

void              facts_with_1_2 (s_facts *facts,
                                  s_facts_cursor *c,
                                  const char *s,
                                  const char *p,
                                  const char *o,
                                  const char **var_s,
                                  const char **var_p,
                                  const char **var_o);

void              facts_with_spo (s_facts *facts,
                                  s_binding *bindings,
                                  s_facts_cursor *c,
                                  const char *s,
                                  const char *p,
                                  const char *o);

int               facts_write (s_facts *facts,
                               FILE *fp);

int               facts_read_fact (s_facts *facts,
                                   s_fact *f,
                                   FILE *fp);

int               facts_load (s_facts *facts,
                              FILE *fp);

int               facts_write_log (const char *operation,
                                   s_fact *f,
                                   FILE *fp);

int               facts_load_log (s_facts *facts,
                                  FILE *fp);

#endif
