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

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "facts.h"
#include "rw.h"

void facts_init (s_facts *facts, unsigned long max)
{
        unsigned long height;
        assert(facts);
        set_init(&facts->set, max);
        height = log(max) / log(FACTS_SKIPLIST_SPACING);
        facts->index_spo = new_skiplist(height, FACTS_SKIPLIST_SPACING);
        assert(facts->index_spo);
        facts->index_spo->compare = fact_compare_spo;
        facts->index_pos = new_skiplist(height, FACTS_SKIPLIST_SPACING);
        assert(facts->index_pos);
        facts->index_pos->compare = fact_compare_pos;
        facts->index_osp = new_skiplist(height, FACTS_SKIPLIST_SPACING);
        assert(facts->index_osp);
        facts->index_osp->compare = fact_compare_osp;
        facts->log = NULL;
}

void facts_destroy (s_facts *facts)
{
        delete_skiplist(facts->index_spo);
        delete_skiplist(facts->index_pos);
        delete_skiplist(facts->index_osp);
        set_destroy(&facts->set);
}

s_facts * new_facts (unsigned long max)
{
        s_facts *facts = malloc(sizeof(s_facts));
        if (facts)
                facts_init(facts, max);
        return facts;
}

void delete_facts (s_facts *facts)
{
        facts_destroy(facts);
        free(facts);
}

const char * facts_find_symbol (s_facts *facts, const char *string)
{
        s_set_item *i;
        size_t len;
        assert(facts);
        assert(string);
        len = strlen(string);
        i = set_get(&facts->set, string, len);
        if (i)
                return i->data;
        return NULL;
}

const char * facts_intern (s_facts *facts, const char *string)
{
        s_set_item *i;
        size_t len;
        assert(facts);
        assert(string);
        len = strlen(string);
        i = set_get(&facts->set, string, len);
        if (!i) {
                char *data = malloc(len + 1);
                memcpy(data, string, len + 1);
                i = set_add(&facts->set, data, len);
        }
        assert(i);
        i->usage++;
        return i->data;
}

void facts_unintern (s_facts *facts, const char *string)
{
        s_set_item *i;
        size_t len;
        assert(facts);
        assert(string);
        len = strlen(string);
        i = set_get(&facts->set, string, len);
        if (i) {
                i->usage--;
                if (!i->usage)
                        set_remove(&facts->set, i);
        }
}

s_fact * facts_add_fact (s_facts *facts, s_fact *f)
{
        s_fact intern;
        s_fact *found;
        s_fact *new;
        assert(facts);
        assert(f);
        intern.s = facts_intern(facts, f->s);
        intern.p = facts_intern(facts, f->p);
        intern.o = facts_intern(facts, f->o);
        if ((found = facts_get_fact(facts, &intern)))
                return found;
        if (facts->log)
                write_fact_log("add", &intern, facts->log);
        new = new_fact(intern.s, intern.p, intern.o);
        assert(new);
        skiplist_insert(facts->index_spo, new);
        skiplist_insert(facts->index_pos, new);
        skiplist_insert(facts->index_osp, new);
        return new;
}

s_fact * facts_add_spo (s_facts *facts, const char *s,
                        const char *p, const char *o)
{
        s_fact f;
        assert(facts);
        assert(s);
        assert(p);
        assert(o);
        f.s = s;
        f.p = p;
        f.o = o;
        return facts_add_fact(facts, &f);
}

int facts_remove_fact (s_facts *facts, s_fact *f)
{
        s_fact *found;
        assert(facts);
        found = skiplist_remove(facts->index_spo, f);
        if (found) {
                skiplist_remove(facts->index_pos, found);
                skiplist_remove(facts->index_osp, found);
                if (facts->log)
                        write_fact_log("remove", found, facts->log);
                facts_unintern(facts, found->s);
                facts_unintern(facts, found->p);
                facts_unintern(facts, found->o);
                delete_fact(found);
                return 1;
        }
        return 0;
}

int facts_remove_spo (s_facts *facts, const char *s,
                      const char *p, const char *o)
{
        s_fact f;
        assert(facts);
        assert(s);
        assert(p);
        assert(o);
        f.s = s;
        f.p = p;
        f.o = o;
        return facts_remove_fact(facts, &f);
}

s_fact * facts_get_fact (s_facts *facts, s_fact *f)
{
        s_skiplist_node *node;
        assert(facts);
        assert(f);
        node = skiplist_find(facts->index_spo, f);
        if (node)
                return (s_fact*) node->value;
        return NULL;
}

s_fact * facts_get_spo (s_facts *facts, const char *s, const char *p,
                        const char *o)
{
        s_fact f;
        f.s = s;
        f.p = p;
        f.o = o;
        return facts_get_fact(facts, &f);
}

unsigned long facts_count (s_facts *facts)
{
        assert(facts);
        return facts->index_spo->length;
}

void facts_cursor_init (s_facts *facts,
                        s_facts_cursor *c,
                        s_skiplist *tree,
                        s_fact *start,
                        s_fact *end)
{
        s_skiplist_node *pred;
        assert(facts);
        assert(c);
        assert(tree);
        pred = skiplist_pred(tree, start);
        assert(pred);
        c->tree = tree;
        c->node = skiplist_node_next(pred, 0);
        if (start)
                c->start = *start;
        else {
                c->start.s = P_FIRST;
                c->start.p = P_FIRST;
                c->start.o = P_FIRST;
        }
        if (end)
                c->end = *end;
        else {
                c->end.s = P_LAST;
                c->end.p = P_LAST;
                c->end.o = P_LAST;
        }
        delete_skiplist_node(pred);
        c->var_s = NULL;
        c->var_p = NULL;
        c->var_o = NULL;
}

s_fact * facts_cursor_next (s_facts_cursor *c)
{
        assert(c);
        if (c->node) {
                c->node = skiplist_node_next(c->node, 0);
                if (c->node &&
                    c->tree->compare(c->node->value, &c->end) > 0)
                        c->node = NULL;
        }
        if (c->node) {
                s_fact *f = (s_fact*) c->node->value;
                if (c->var_s)
                        *c->var_s = f->s;
                if (c->var_p)
                        *c->var_p = f->p;
                if (c->var_o)
                        *c->var_o = f->o;
                return f;
        }
        return NULL;
}

void facts_with_3 (s_facts *facts, s_facts_cursor *c, const char *s,
                   const char *p, const char *o)
{
        s_fact f;
        assert(facts);
        assert(c);
        assert(s);
        assert(p);
        assert(o);
        f.s = s;
        f.p = p;
        f.o = o;
        facts_cursor_init(facts, c, facts->index_spo, &f, &f);
}

void facts_with_0 (s_facts *facts, s_facts_cursor *c,
                   const char **var_s, const char **var_p,
                   const char **var_o)
{
        assert(facts);
        assert(c);
        facts_cursor_init(facts, c, facts->index_spo, NULL, NULL);
        c->var_s = var_s;
        c->var_p = var_p;
        c->var_o = var_o;
}

void facts_with_1_2 (s_facts *facts, s_facts_cursor *c, const char *s,
                     const char *p, const char *o, const char **var_s,
                     const char **var_p, const char **var_o)
{
        s_fact start;
        s_fact end;
        s_skiplist *tree;
        assert(facts);
        assert(c);
        assert(s);
        assert(p);
        assert(o);
        assert(var_s || var_p || var_o);
        start.s = var_s ? P_FIRST : s;
        start.p = var_p ? P_FIRST : p;
        start.o = var_o ? P_FIRST : o;
        end.s = var_s ? P_LAST : s;
        end.p = var_p ? P_LAST : p;
        end.o = var_o ? P_LAST : o;
        tree = (!var_s && var_o) ? facts->index_spo :
                !var_p ? facts->index_pos :
                facts->index_osp;
        facts_cursor_init(facts, c, tree, &start, &end);
        c->var_s = var_s;
        c->var_p = var_p;
        c->var_o = var_o;
}

void facts_with_spo (s_facts *facts,
                     s_binding *bindings,
                     s_facts_cursor *c,
                     const char *s,
                     const char *p,
                     const char *o)
{
        const char **var_s;
        const char **var_p;
        const char **var_o;
        assert(facts);
        assert(c);
        assert(s);
        assert(p);
        assert(o);
        var_s = (s[0] == '?') ? bindings_get(bindings, s) : NULL;
        var_p = (p[0] == '?') ? bindings_get(bindings, p) : NULL;
        var_o = (o[0] == '?') ? bindings_get(bindings, o) : NULL;
        if (var_s && var_p && var_o)
                facts_with_0(facts, c, var_s, var_p, var_o);
        else if (!(var_s || var_p || var_o))
                facts_with_3(facts, c, s, p, o);
        else
                facts_with_1_2(facts, c, s, p, o, var_s, var_p, var_o);
}
