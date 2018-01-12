/*
 *  Copyright 2007 by Tomas Mandys <tomas.mandys at 2p dot cz>
 */

/*  This file is part of abnfc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  It is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Ragel; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _ABNF_H_
#define _ABNF_H_ 1

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct abnf_str {
	char *s;
	unsigned int len;
	enum {ABNF_STR_MALLOC=0x01} flags;
};

enum abnf_element_type {ABNF_ET_NONE=0, ABNF_ET_RULE, ABNF_ET_GROUP, ABNF_ET_STRING, ABNF_ET_TOKEN, ABNF_ET_RANGE, ABNF_ET_ACTION};
struct abnf_rule;

typedef void (*abnf_action_t)(void);

struct abnf_element {
	enum abnf_element_type type;
	union {
		struct {
			struct abnf_str name;
			struct abnf_rule *resolved;

		} rule;
		struct abnf_alternation *group;

		struct {
			unsigned char lo;
			unsigned char hi;
		} range;

		/* terminals */
		struct abnf_str string;  /* case sensitive */
		struct abnf_str token;   /* case insensitive */

		/* used action */
		struct {
			abnf_action_t action;
			void *user_data;
		} action;
	} u;

};

struct abnf_repetition {
	unsigned int min, max;
	struct abnf_element element;
};

struct abnf_concatenation {
	struct abnf_repetition repetition;

	struct abnf_concatenation *prev, *next;
};

struct abnf_alternation {
	struct abnf_concatenation *concatenation;

	struct abnf_alternation *prev, *next;

};

struct abnf_rule {
	struct abnf_str name;
	struct abnf_alternation *alternation;
	struct abnf_str origin;
	struct {  /* private fields */
		unsigned int flags;
	} internal;
	struct abnf_rule *prev, *next;
};

struct abnf_print_info {
	unsigned int in_file_count;
	struct abnf_str *in_files;
	struct abnf_str out_file;
};

struct abnf_print_comment {
	char *pre_comment;
	char *line_comment;
	char *post_comment;
};

/** print list of files to the stream as comment
 */
extern void abnf_print_header(FILE *stream, struct abnf_print_info *info, struct abnf_print_comment *comment);

struct abnf_str abnf_mk_str(char *s);
struct abnf_str abnf_dupl_str(struct abnf_str s);
void abnf_destroy_str(struct abnf_str *s);
int abnf_rule_count(struct abnf_rule *p);
int abnf_alternation_count(struct abnf_alternation *p);
int abnf_concatenation_count(struct abnf_concatenation *p);

#define abnf_malloc(_size_) malloc(_size_)
#define abnf_free(_p_) free(_p_)
#define abnf_realloc(_p_, _size_) realloc((_p_), (_size_))

extern struct abnf_rule* abnf_find_rule(struct abnf_rule* rules, struct abnf_str name);

/* struct abnf_repetition related stuff */
#define ABNF_INFINITY ((unsigned int)((((unsigned long long)1)<<(8*sizeof(unsigned int)))-1))
#define ABNF_IS_OPTIONAL(_r_) ((_r_).min==0 && (_r_).max==1)
#define ABNF_IS_ONCE(_r_) ((_r_).min==1 && (_r_).max==1)
#define ABNF_IS_ANY(_r_) ((_r_).min==0 && (_r_).max==ABNF_INFINITY)
#define ABNF_IS_MORE(_r_) ((_r_).min==1 && (_r_).max==ABNF_INFINITY)

#define ABNF_IS_ALPHA(_c_) ( ((_c_)>='a' && (_c_)<='z') || ((_c_)>='A' && (_c_)<='Z') )
#define ABNF_IS_DIGIT(_c_) ( ((_c_)>='0' && (_c_)<='9') )
#define ABNF_IS_VALID_TOKEN_CHAR(_c_) ( ((_c_)>=0x20 && (_c_)<=0x21) || ((_c_)>=0x23 && (_c_)<=0x7e) )

extern struct abnf_rule* abnf_add_rule(struct abnf_str name, struct abnf_alternation* alternation, struct abnf_rule* next);
extern struct abnf_alternation* abnf_add_alternation(struct abnf_concatenation *concatenation, struct abnf_alternation *next);
extern struct abnf_concatenation* abnf_add_concatenation(struct abnf_repetition repetition, struct abnf_concatenation *next);
extern struct abnf_repetition abnf_mk_repetition(struct abnf_element element, unsigned int min, unsigned int max);
#define abnf_mk_optional(_element_) abnf_mk_repetition((_element_), 0, 1)
#define abnf_mk_once(_element_) abnf_mk_repetition((_element_), 1, 1)
#define abnf_mk_any(_element_) abnf_mk_repetition((_element_), 0, ABNF_INFINITY)
#define abnf_mk_more(_element_) abnf_mk_repetition((_element_), 1, ABNF_INFINITY)
extern struct abnf_element abnf_mk_element_empty(void);
extern struct abnf_element abnf_mk_element_rule(struct abnf_str name);
extern struct abnf_element abnf_mk_element_range(unsigned char lo, unsigned char hi);
#define abnf_mk_element_char(_c_) abnf_mk_element_range((_c_), (_c_))
extern struct abnf_element abnf_mk_element_string(struct abnf_str string);
extern struct abnf_element abnf_mk_element_token(struct abnf_str token);
extern struct abnf_element abnf_mk_element_group(struct abnf_alternation* group);
extern struct abnf_element abnf_mk_element_action(abnf_action_t action, void* user_data);

/** assign origin for rules chain up to rules_end (excluded), if rules_end is null then assign to all rules */
extern void abnf_rule_assign_origin(struct abnf_rule* rules, struct abnf_rule* rules_end, struct abnf_str origin);

extern void abnf_destroy_rules(struct abnf_rule* rules);
extern void abnf_destroy_alternations(struct abnf_alternation* alternation);
extern void abnf_destroy_concatenations(struct abnf_concatenation* concatenation);
extern void abnf_destroy_element(struct abnf_element* e);
#define abnf_remove_list_item(_start_,_p_) \
	if ((_p_)->prev) { \
		(_p_)->prev->next = (_p_)->next; \
	} \
	if ((_p_)->next) { \
		(_p_)->next->prev = (_p_)->prev; \
	} \
	if (/*(_start_) && */(_start_)==(_p_)) { \
		(_start_) = (_p_)->next; \
	} \
	(_p_)->prev = (_p_)->next = NULL;

extern struct abnf_rule* abnf_declare_core_rules(struct abnf_rule* next);   /* RFC2234 Core rules */
extern struct abnf_rule* abnf_declare_abnf_rules(struct abnf_rule* next);   /* RFC2234 ABNF definition of ABNF */

extern int abnf_resolve_rule_dependencies(FILE *stream, struct abnf_rule **rules);
extern int abnf_check_rules(FILE *stream, struct abnf_rule *rules);

/* code located in print_*.c */
extern void abnf_print_abnf_rules(FILE *stream, struct abnf_rule *rules, struct abnf_print_info *info);
extern void abnf_print_ragel_rules(FILE *stream, struct abnf_rule *rules, struct abnf_print_info *info, char* machine_name, int instantiate);
extern void abnf_print_self_rules(FILE *stream, struct abnf_rule *rules, struct abnf_print_info *info);

/* code located in parse_*.c */
/** if origin non empty then string will be duplicated for each rule */
extern int abnf_parse_abnf(FILE* in_stream, struct abnf_rule **rules, struct abnf_str origin);

extern int abnf_stop_flag;

#endif

