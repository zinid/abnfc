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


#include "abnf.h"
#include <time.h>

struct abnf_rule* abnf_find_rule(struct abnf_rule* rules, struct abnf_str name) {
	for (; rules; rules=rules->next) {
		if (rules->name.len == name.len && strncasecmp(rules->name.s, name.s, name.len) == 0) {
			return rules;
		}
	}
	return NULL;
}

struct abnf_str abnf_mk_str(char *s) {
	struct abnf_str as;
	as.s = s;
	as.len = s?strlen(s):0;
	as.flags = 0;
	return as;
};

struct abnf_str abnf_dupl_str(struct abnf_str s) {
	struct abnf_str as;
	if (s.len > 0) {
		as.s = abnf_malloc(s.len);
		if (as.s) {
			memcpy(as.s, s.s, s.len);
			as.len = s.len;
			as.flags = ABNF_STR_MALLOC;
		}
		else {
			as.len = 0;
			as.flags = 0;
		}
	}
	else {
		as.s = NULL;
		as.len = 0;
		as.flags = 0;
	}
	return as;
};

void abnf_destroy_str(struct abnf_str *s) {
	if (s->flags & ABNF_STR_MALLOC && s->s) {
		abnf_free(s->s);
	}
	s->s = NULL;
	s->flags = 0;
}

int abnf_rule_count(struct abnf_rule *p) {
	int n;
	for (n=0; p; p=p->next, n++);
	return n;
}

int abnf_alternation_count(struct abnf_alternation *p) {
	int n;
	for (n=0; p; p=p->next, n++);
	return n;
}

int abnf_concatenation_count(struct abnf_concatenation *p) {
	int n;
	for (n=0; p; p=p->next, n++);
	return n;
}


#define ABNF_ADD_LIST_ITEM(_p_, _next_) {\
	(_p_)->next = (_next_); \
	if ((_p_)->next) {  \
		(_p_)->prev = (_p_)->next->prev; \
		if ((_p_)->prev) { \
			(_p_)->prev->next = (_p_); \
		} \
		(_p_)->next->prev = (_p_); \
	} \
	else \
		(_p_)->prev = NULL; \
}

struct abnf_rule* abnf_add_rule(struct abnf_str name, struct abnf_alternation* alternation, struct abnf_rule* next) {
	struct abnf_rule* p;
	p = abnf_malloc(sizeof(*p));
	if (!p) return next;
	p->name = name;
	p->origin.len = 0;
	p->origin.s = 0;
	p->alternation = alternation;
	ABNF_ADD_LIST_ITEM(p, next);
	return p;
}

struct abnf_alternation* abnf_add_alternation(struct abnf_concatenation *concatenation, struct abnf_alternation *next) {
	struct abnf_alternation* p;
	p = abnf_malloc(sizeof(*p));
	if (!p) return next;
	p->concatenation = concatenation;
	ABNF_ADD_LIST_ITEM(p, next);
	return p;
}

struct abnf_concatenation* abnf_add_concatenation(struct abnf_repetition repetition, struct abnf_concatenation *next) {
	struct abnf_concatenation* p;
	p = abnf_malloc(sizeof(*p));
	if (!p) return next;
	p->repetition = repetition;
	ABNF_ADD_LIST_ITEM(p, next);
	return p;
}

struct abnf_repetition abnf_mk_repetition(struct abnf_element element, unsigned int min, unsigned int max) {
	struct abnf_repetition r;
	r.element = element;
	r.min = min;
	r.max = max;
	return r;
}

struct abnf_element abnf_mk_element_empty(void) {
	struct abnf_element r;
	r.type = ABNF_ET_NONE;
	return r;
}

struct abnf_element abnf_mk_element_rule(struct abnf_str name) {
	struct abnf_element r;
	r.type = ABNF_ET_RULE;
	r.u.rule.name = name;
	r.u.rule.resolved = NULL;
	return r;
}

struct abnf_element abnf_mk_element_range(unsigned char lo, unsigned char hi) {
	struct abnf_element r;
	r.type = ABNF_ET_RANGE;
	r.u.range.lo = lo;
	r.u.range.hi = hi;
	return r;
}

struct abnf_element abnf_mk_element_string(struct abnf_str string) {
	struct abnf_element r;
	r.type = ABNF_ET_STRING;
	r.u.string = string;
	return r;
}

struct abnf_element abnf_mk_element_token(struct abnf_str token) {
	struct abnf_element r;
	r.type = ABNF_ET_TOKEN;
	r.u.token = token;
	return r;
}

struct abnf_element abnf_mk_element_group(struct abnf_alternation* group) {
	struct abnf_element r;
	r.type = ABNF_ET_GROUP;
	r.u.group = group;
	return r;
}

struct abnf_element abnf_mk_element_action(abnf_action_t action, void* user_data) {
	struct abnf_element r;
	r.type = ABNF_ET_ACTION;
	r.u.action.action = action;
	r.u.action.user_data = user_data;
	return r;
}

void abnf_rule_assign_origin(struct abnf_rule* rules, struct abnf_rule* rules_end, struct abnf_str origin) {
	for (; rules && rules != rules_end; rules=rules->next) {
		rules->origin = origin;
	}
}

void abnf_destroy_rules(struct abnf_rule* rules) {
	struct abnf_rule* p;
	while (rules) {
		abnf_destroy_str(&rules->name);
		abnf_destroy_str(&rules->origin);
		abnf_destroy_alternations(rules->alternation);
		p = rules;
		rules = rules->next;
		abnf_free(p);
	}
}

void abnf_destroy_alternations(struct abnf_alternation* alternation) {
	struct abnf_alternation* p;
	while (alternation) {
		abnf_destroy_concatenations(alternation->concatenation);
		p = alternation;
		alternation = alternation->next;
		abnf_free(p);
	}
}

void abnf_destroy_concatenations(struct abnf_concatenation* concatenation) {
	struct abnf_concatenation* p;
	while (concatenation) {
		abnf_destroy_element(&concatenation->repetition.element);
		p = concatenation;
		concatenation = concatenation->next;
		abnf_free(p);
	}
}
void abnf_destroy_element(struct abnf_element* e) {
	switch (e->type) {
		case ABNF_ET_GROUP:
			abnf_destroy_alternations(e->u.group);
			e->u.group = NULL;
			break;
		case ABNF_ET_RULE:
			abnf_destroy_str(&e->u.rule.name);
			break;
		case ABNF_ET_STRING:
			abnf_destroy_str(&e->u.string);
			break;
		case ABNF_ET_TOKEN:
			abnf_destroy_str(&e->u.token);
			break;
		default:
			;
	}
	e->type = ABNF_ET_NONE;
}

static int abnf_check_element(FILE *stream, struct abnf_rule *rules, struct abnf_rule *pr, struct abnf_element* e);

static int abnf_check_alternations(FILE *stream, struct abnf_rule *rules, struct abnf_rule *pr, struct abnf_alternation *pa) {
	int ret = 0;
	struct abnf_concatenation* pc;
	for (; pa; pa = pa->next) {
		if (!pa->concatenation) {
			fprintf(stream, "rule '%.*s': concatenation is empty\n", pr->name.len, pr->name.s);
			ret = -1;
			continue;
		}
		for (pc = pa->concatenation; pc; pc = pc->next) {
			if (pc->repetition.min == 0 && pc->repetition.max == 0) {
				fprintf(stream, "rule '%.*s': repetition is zero\n", pr->name.len, pr->name.s);
				ret = -1;
			}
			if (pc->repetition.min > pc->repetition.max) {
				fprintf(stream, "rule '%.*s': bad repetition '%u' > '%u'\n", pr->name.len, pr->name.s, pc->repetition.min, pc->repetition.max);
				ret = -1;
			}
			if (abnf_check_element(stream, rules, pr, &pc->repetition.element) < 0)
				ret = -1;
		}
	}
	return ret;
}

static int abnf_check_element(FILE *stream, struct abnf_rule *rules, struct abnf_rule *pr, struct abnf_element* e) {
	switch (e->type) {
		case ABNF_ET_NONE:
			fprintf(stream, "rule '%.*s': element type is NONE\n", pr->name.len, pr->name.s);
			break;
		case ABNF_ET_RULE:
			if (e->u.rule.name.len == 0) {
				fprintf(stream, "rule '%.*s': rule name is empty\n", pr->name.len, pr->name.s);
				return -1;
			}
			e->u.rule.resolved = abnf_find_rule(rules, e->u.rule.name);
			if (!e->u.rule.resolved) {
				fprintf(stream, "rule '%.*s': rule '%.*s' not found\n", pr->name.len, pr->name.s, e->u.rule.name.len, e->u.rule.name.s);
				return -1;
			}
			break;
		case ABNF_ET_GROUP:
			if (!e->u.group) {
				fprintf(stream, "rule '%.*s': group is empty\n", pr->name.len, pr->name.s);
				return -1;
			}
			return abnf_check_alternations(stream, rules, pr, e->u.group);
		case ABNF_ET_TOKEN:
			if (e->u.string.len == 0) {
				fprintf(stream, "rule '%.*s': token is empty\n", pr->name.len, pr->name.s);
				return -1;
			}
			break;
		case ABNF_ET_STRING:
			if (e->u.string.len == 0) {
				fprintf(stream, "rule '%.*s': string is empty\n", pr->name.len, pr->name.s);
				return -1;
			}
			break;
		case ABNF_ET_RANGE:
			if (e->u.range.lo > e->u.range.hi) {
				fprintf(stream, "rule '%.*s': bad range '%u' > '%u'\n", pr->name.len, pr->name.s, e->u.range.lo, e->u.range.hi);
    			return -1;
			}
			break;
		default:
			;
	}
	return 0;
}

int abnf_check_rules(FILE *stream, struct abnf_rule *rules) {
	struct abnf_rule *pr;
	int ret = 0, i;
	for (pr = rules; pr; pr = pr->next) {
		if (!pr->alternation) {
			fprintf(stream, "rule '%.*s': alternation is empty\n", pr->name.len, pr->name.s);
			ret = -1;
			continue;
		}
		if (pr->name.len == 0) {
			fprintf(stream, "rule '%.*s': rule name is empty\n", pr->name.len, pr->name.s);
			ret = -1;
			continue;
		}
		for (i=0; i < pr->name.len; i++) {
			if ( ABNF_IS_ALPHA(pr->name.s[i]) ) continue;

			if (i > 0 && (ABNF_IS_DIGIT(pr->name.s[i]) || pr->name.s[i] == '-')) continue;
			fprintf(stream, "rule '%.*s': bad characters in rule name\n", pr->name.len, pr->name.s);
			ret = -1;
			goto cont;
		}
		if (abnf_find_rule(rules, pr->name) != pr) {
			fprintf(stream, "rule '%.*s': duplicate name\n", pr->name.len, pr->name.s);
			ret = -1;
			continue;
		}

		if (abnf_check_alternations(stream, rules, pr, pr->alternation) < 0)
			ret = -1;
	cont: ;
	}
	return ret;
}

#define ABNF_INTERNAL_CIRCULAR 0x01
#define ABNF_INTERNAL_RESOLVED 0x02

static int abnf_resolve_rule2_dependencies(FILE *stream, struct abnf_rule *pr, struct abnf_rule **pr_arr, unsigned int *n);

static int abnf_resolve_rule_alternation_dependencies(FILE *stream, struct abnf_rule *pr, struct abnf_alternation *pa, struct abnf_rule **pr_arr, unsigned int *n) {
	struct abnf_concatenation *pc;
	int ret = 0;
	for (; pa; pa = pa->next) {
		for (pc = pa->concatenation; pc; pc = pc->next) {
			switch (pc->repetition.element.type) {
				case ABNF_ET_RULE:
					if (pc->repetition.element.u.rule.resolved &&
						pc->repetition.element.u.rule.resolved != pr &&
						(pc->repetition.element.u.rule.resolved->internal.flags & ABNF_INTERNAL_RESOLVED) == 0
						) {
						if (pc->repetition.element.u.rule.resolved->internal.flags & ABNF_INTERNAL_CIRCULAR) {
							fprintf(stream, "rule '%.*s' has circular dependency with '%.*s'\n", pr->name.len, pr->name.s, pc->repetition.element.u.rule.resolved->name.len, pc->repetition.element.u.rule.resolved->name.s);
							ret = -1;
							break;
						}
						if (abnf_resolve_rule2_dependencies(stream, pc->repetition.element.u.rule.resolved, pr_arr, n) < 0) {
							ret = -1;
						}
					}
					break;

				case ABNF_ET_GROUP:
					if (abnf_resolve_rule_alternation_dependencies(stream, pr, pc->repetition.element.u.group, pr_arr, n) < 0) {
						ret = -1;
					}
					break;
				default:
					;
			}
		}
	}
	return ret;
}

static int abnf_resolve_rule2_dependencies(FILE *stream, struct abnf_rule *pr, struct abnf_rule **pr_arr, unsigned int *n) {
	int ret;
	/* if (pr->internal.flags & ABNF_INTERNAL_RESOLVED) return 0; tested before invocation */
	pr->internal.flags |= ABNF_INTERNAL_CIRCULAR;
	if (abnf_resolve_rule_alternation_dependencies(stream, pr, pr->alternation, pr_arr, n) < 0) {
		ret = -1;
	}
	pr_arr[*n] = pr;
	(*n)++;
	pr->internal.flags |= ABNF_INTERNAL_RESOLVED;
	return ret;
}

int abnf_resolve_rule_dependencies(FILE *stream, struct abnf_rule **rules) {

	struct abnf_rule **pr_arr, *pr, *pr2;
	unsigned int n, i;
	int ret = 0;
	/* we must reorder rules because ragel does not support forward links */
	for (pr = *rules, n=0; pr; pr = pr->next, n++) {
		pr->internal.flags &= ~ABNF_INTERNAL_RESOLVED;
	}
	if (n <= 1) return 0;
	pr_arr = abnf_malloc(sizeof(*pr_arr)*n);
	if (!pr_arr) return -1;

	n = 0;
	for (pr = *rules; pr; pr = pr->next) {
		if (pr->internal.flags & ABNF_INTERNAL_RESOLVED) continue;
		for (pr2 = *rules; pr2; pr2 = pr2->next) {
			pr2->internal.flags &= ~ABNF_INTERNAL_CIRCULAR; /* reset circualar flag */
		}
		if (abnf_resolve_rule2_dependencies(stream, pr, pr_arr, &n) < 0) {
			ret = -1;
		}
	}
	/* reorder rule chain */
	for (i=0; i<n; i++) {
		if (i>0) {
			pr_arr[i-1]->next = pr_arr[i];
			pr_arr[i]->prev = pr_arr[i-1];
		}
		else {
			*rules = pr_arr[i];
			pr_arr[i]->prev = NULL;
		}
		pr_arr[i]->next = NULL;
	}
	abnf_free(pr_arr);
	return ret;
}

void abnf_print_header(FILE *stream, struct abnf_print_info *info, struct abnf_print_comment *comment_def) {
	int i;
	if (info) {
		time_t t;
		char *lc;
		lc = comment_def->line_comment?comment_def->line_comment:"";
		t = time(NULL);
		if (comment_def->pre_comment)
			fprintf(stream, "%s", comment_def->pre_comment);
		fprintf(stream, "%sGenerated by abnfc at %s", lc, ctime(&t));
		if (info->out_file.len) {
			fprintf(stream, "%sOutput file: %.*s\n", lc, info->out_file.len, info->out_file.s);
		}
		if (info->in_file_count) {
			fprintf(stream, "%sSources:\n", lc);
			for (i=0; i<info->in_file_count; i++) {
				fprintf(stream, "%s\t%.*s\n", lc, info->in_files[i].len, info->in_files[i].s);
			}
		}
		if (comment_def->post_comment)
			fprintf(stream, "%s", comment_def->post_comment);
	}

}

/* gramatic declarations */
struct abnf_rule* abnf_declare_core_rules(struct abnf_rule* next) {

	struct abnf_rule *rule_list;

	rule_list =
		abnf_add_rule(
			abnf_mk_str("ALPHA"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_range(0x41, 0x5A)
					),
					NULL  /* end of concatenations */
				),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_range(0x61, 0x7A)
					),
					NULL  /* end of concatenations */
				),
				NULL  /* end of alternations */
			)),
		abnf_add_rule(
			abnf_mk_str("BIT"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_char('0')
					),
					NULL  /* end of concatenations */
				),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_char('1')
					),
					NULL  /* end of concatenations */
				),
				NULL  /* end of alternations */
			)),
		abnf_add_rule(
			abnf_mk_str("CHAR"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_range(0x01, 0x7F)
					),
					NULL  /* end of concatenations */
				),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("CR"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_char(0x0D)
					),
					NULL  /* end of concatenations */
				),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("CRLF"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("CR"))
					),
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("LF"))
					),
					NULL  /* end of concatenations */
				)),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("CTL"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_range(0x00, 0x1F)
					),
					NULL  /* end of concatenations */
				),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_char(0x7F)
					),
					NULL  /* end of concatenations */
				),
				NULL  /* end of alternations */
			)),
		abnf_add_rule(
			abnf_mk_str("DIGIT"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_range(0x30, 0x39)
					),
					NULL  /* end of concatenations */
				),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("DQUOTE"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_char(0x22)
					),
					NULL  /* end of concatenations */
				),
				NULL  /* end of alternations */
			),

		abnf_add_rule(
			abnf_mk_str("HEXDIG"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("DIGIT"))
					),
					NULL  /* end of concatenations */
				),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_token(abnf_mk_str("A"))
					),
					NULL  /* end of concatenations */
				),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_token(abnf_mk_str("B"))
					),
					NULL  /* end of concatenations */
				),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_token(abnf_mk_str("C"))
					),
					NULL  /* end of concatenations */
				),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_token(abnf_mk_str("D"))
					),
					NULL  /* end of concatenations */
				),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_token(abnf_mk_str("E"))
					),
					NULL  /* end of concatenations */
				),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_token(abnf_mk_str("F"))
					),
					NULL  /* end of concatenations */
				),
				NULL  /* end of alternations */
			))))))),
		abnf_add_rule(
			abnf_mk_str("HTAB"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_char(0x09)
					),
					NULL  /* end of concatenations */
				),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("LF"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_char(0x0A)
					),
					NULL  /* end of concatenations */
				),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("LWSP"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_any(
						abnf_mk_element_group(
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_rule(abnf_mk_str("WSP"))
									),
									NULL  /* end of concatenations */
								),
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_rule(abnf_mk_str("CRLF"))
									),
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_rule(abnf_mk_str("WSP"))
									),
									NULL  /* end of concatenations */
								)),
								NULL  /* end of alternations */
							))
						)
					),
					NULL  /* end of concatenations */
				),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("OCTET"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_range(0x00, 0xFF)
					),
					NULL  /* end of concatenations */
				),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("SP"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_char(0x20)
					),
					NULL  /* end of concatenations */
				),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("VCHAR"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_range(0x21, 0x7E)
					),
					NULL  /* end of concatenations */
				),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("WSP"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("SP"))
					),
					NULL  /* end of concatenations */
				),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("HTAB"))
					),
					NULL  /* end of concatenations */
				),
				NULL  /* end of alternations */
			)),

			next   /* end of rules */
		))))))))))))))));

	abnf_rule_assign_origin(rule_list, next, abnf_mk_str("RFC2234 Core"));
	return rule_list;
}

struct abnf_rule* abnf_declare_abnf_rules(struct abnf_rule* next) {

	struct abnf_rule *rule_list;

	rule_list =
		abnf_add_rule(
			abnf_mk_str("rulelist"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_more(
						abnf_mk_element_group(
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_rule(abnf_mk_str("rule"))
									),
									NULL  /* end of concatenations */
								),
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_any(
										abnf_mk_element_rule(abnf_mk_str("c-wsp"))
									),
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_rule(abnf_mk_str("c-nl"))
									),
									NULL  /* end of concatenations */
								)),
								NULL  /* end of alternations */
							))
						)
					),
					NULL  /* end of concatenations */
				),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("rule"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("rulename"))
					),
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("defined-as"))
					),
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("elements"))
					),
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("c-nl"))
					),
					NULL  /* end of concatenations */
				)))),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("rulename"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("ALPHA"))
					),
				abnf_add_concatenation(
					abnf_mk_any(
						abnf_mk_element_group(
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_rule(abnf_mk_str("ALPHA"))
									),
									NULL  /* end of concatenations */
								),
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_rule(abnf_mk_str("DIGIT"))
									),
									NULL  /* end of concatenations */
								),
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_char('-')
									),
									NULL  /* end of concatenations */
								),
								NULL  /* end of alternations */
							)))
						)
					),
					NULL  /* end of concatenations */
				)),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("defined-as"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_any(
						abnf_mk_element_rule(abnf_mk_str("c-wsp"))
					),
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_group(
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_char('=')
									),
									NULL  /* end of concatenations */
								),
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_token(abnf_mk_str("=/"))
									),
									NULL  /* end of concatenations */
								),
								NULL  /* end of alternations */
							))
						)
					),
				abnf_add_concatenation(
					abnf_mk_any(
						abnf_mk_element_rule(abnf_mk_str("c-wsp"))
					),
					NULL  /* end of concatenations */
				))),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("elements"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("alternation"))
					),
				abnf_add_concatenation(
					abnf_mk_any(
						abnf_mk_element_rule(abnf_mk_str("c-wsp"))
					),
					NULL  /* end of concatenations */
				)),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("c-wsp"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("WSP"))
					),
					NULL  /* end of concatenations */
				),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("c-nl"))
					),
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("WSP"))
					),
					NULL  /* end of concatenations */
				)),
				NULL  /* end of alternations */
			)),
		abnf_add_rule(
			abnf_mk_str("c-nl"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("comment"))
					),
					NULL  /* end of concatenations */
				),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("CRLF"))
					),
					NULL  /* end of concatenations */
				),
				NULL  /* end of alternations */
			)),
		abnf_add_rule(
			abnf_mk_str("comment"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_char(';')
					),
				abnf_add_concatenation(
					abnf_mk_any(
						abnf_mk_element_group(
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_rule(abnf_mk_str("WSP"))
									),
									NULL  /* end of concatenations */
								),
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_rule(abnf_mk_str("VCHAR"))
									),
									NULL  /* end of concatenations */
								),
								NULL  /* end of alternations */
							))
						)
					),
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("CRLF"))
					),
					NULL  /* end of concatenations */
				))),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("alternation"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("concatenation"))
					),
				abnf_add_concatenation(
					abnf_mk_any(
						abnf_mk_element_group(
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_any(
										abnf_mk_element_rule(abnf_mk_str("c-wsp"))
									),
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_char('/')
									),
								abnf_add_concatenation(
									abnf_mk_any(
										abnf_mk_element_rule(abnf_mk_str("c-wsp"))
									),
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_rule(abnf_mk_str("concatenation"))
									),
									NULL  /* end of concatenations */
								)))),
								NULL  /* end of alternations */
							)
						)
					),
					NULL  /* end of concatenations */
				)),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("concatenation"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("repetition"))
					),
				abnf_add_concatenation(
					abnf_mk_any(
						abnf_mk_element_group(
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_more(
										abnf_mk_element_rule(abnf_mk_str("c-wsp"))
									),
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_rule(abnf_mk_str("repetition"))
									),
									NULL  /* end of concatenations */
								)),
								NULL  /* end of alternations */
							)
						)
					),
					NULL  /* end of concatenations */
				)),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("repetition"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_optional(
						abnf_mk_element_rule(abnf_mk_str("repeat"))
					),
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("element"))
					),
					NULL  /* end of concatenations */
				)),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("repeat"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_more(
						abnf_mk_element_rule(abnf_mk_str("DIGIT"))
					),
					NULL  /* end of concatenations */
				),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_any(
						abnf_mk_element_rule(abnf_mk_str("DIGIT"))
					),
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_char('*')
					),
				abnf_add_concatenation(
					abnf_mk_any(
						abnf_mk_element_rule(abnf_mk_str("DIGIT"))
					),
					NULL  /* end of concatenations */
				))),
				NULL  /* end of alternations */
			)),
		abnf_add_rule(
			abnf_mk_str("element"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("rulename"))
					),
					NULL  /* end of concatenations */
				),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("group"))
					),
					NULL  /* end of concatenations */
				),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("option"))
					),
					NULL  /* end of concatenations */
				),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("char-val"))
					),
					NULL  /* end of concatenations */
				),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("num-val"))
					),
					NULL  /* end of concatenations */
				),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("prose-val"))
					),
					NULL  /* end of concatenations */
				),
				NULL  /* end of alternations */
			)))))),
		abnf_add_rule(
			abnf_mk_str("group"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_char('(')
					),
				abnf_add_concatenation(
					abnf_mk_any(
						abnf_mk_element_rule(abnf_mk_str("c-wsp"))
					),
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("alternation"))
					),
				abnf_add_concatenation(
					abnf_mk_any(
						abnf_mk_element_rule(abnf_mk_str("c-wsp"))
					),
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_char(')')
					),
					NULL  /* end of concatenations */
				))))),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("option"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_char('[')
					),
				abnf_add_concatenation(
					abnf_mk_any(
						abnf_mk_element_rule(abnf_mk_str("c-wsp"))
					),
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("alternation"))
					),
				abnf_add_concatenation(
					abnf_mk_any(
						abnf_mk_element_rule(abnf_mk_str("c-wsp"))
					),
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_char(']')
					),
					NULL  /* end of concatenations */
				))))),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("char-val"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("DQUOTE"))
					),
				abnf_add_concatenation(
					abnf_mk_any(
						abnf_mk_element_group(
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_range(0x20, 0x21)
									),
									NULL  /* end of concatenations */
								),
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_range(0x23, 0x7E)
									),
									NULL  /* end of concatenations */
								),
								NULL  /* end of alternations */
							))
						)
					),
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_rule(abnf_mk_str("DQUOTE"))
					),
					NULL  /* end of concatenations */
				))),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("num-val"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_char('%')
					),
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_group(
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_rule(abnf_mk_str("bin-val"))
									),
									NULL  /* end of concatenations */
								),
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_rule(abnf_mk_str("dec-val"))
									),
									NULL  /* end of concatenations */
								),
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_rule(abnf_mk_str("hex-val"))
									),
									NULL  /* end of concatenations */
								),
								NULL  /* end of alternations */
							)))
						)
					),
					NULL  /* end of concatenations */
				)),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("bin-val"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_token(abnf_mk_str("b"))
					),
				abnf_add_concatenation(
					abnf_mk_more(
						abnf_mk_element_rule(abnf_mk_str("BIT"))
					),
				abnf_add_concatenation(
					abnf_mk_optional(
						abnf_mk_element_group(
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_more(
										abnf_mk_element_group(
											abnf_add_alternation(
												abnf_add_concatenation(
													abnf_mk_once(
														abnf_mk_element_char('.')
													),
												abnf_add_concatenation(
													abnf_mk_more(
														abnf_mk_element_rule(abnf_mk_str("BIT"))
													),
													NULL  /* end of concatenations */
												)),
												NULL  /* end of alternations */
											)
										)
									),
									NULL  /* end of concatenations */
								),
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_char('-')
									),
								abnf_add_concatenation(
									abnf_mk_more(
										abnf_mk_element_rule(abnf_mk_str("BIT"))
									),
									NULL  /* end of concatenations */
								)),
								NULL  /* end of alternations */
							))
						)
					),
					NULL  /* end of concatenations */
				))),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("dec-val"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_token(abnf_mk_str("d"))
					),
				abnf_add_concatenation(
					abnf_mk_more(
						abnf_mk_element_rule(abnf_mk_str("DIGIT"))
					),
				abnf_add_concatenation(
					abnf_mk_optional(
						abnf_mk_element_group(
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_more(
										abnf_mk_element_group(
											abnf_add_alternation(
												abnf_add_concatenation(
													abnf_mk_once(
														abnf_mk_element_char('.')
													),
												abnf_add_concatenation(
													abnf_mk_more(
														abnf_mk_element_rule(abnf_mk_str("DIGIT"))
													),
													NULL  /* end of concatenations */
												)),
												NULL  /* end of alternations */
											)
										)
									),
									NULL  /* end of concatenations */
								),
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_char('-')
									),
								abnf_add_concatenation(
									abnf_mk_more(
										abnf_mk_element_rule(abnf_mk_str("DIGIT"))
									),
									NULL  /* end of concatenations */
								)),
								NULL  /* end of alternations */
							))
						)
					),
					NULL  /* end of concatenations */
				))),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("hex-val"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_token(abnf_mk_str("x"))
					),
				abnf_add_concatenation(
					abnf_mk_more(
						abnf_mk_element_rule(abnf_mk_str("HEXDIG"))
					),
				abnf_add_concatenation(
					abnf_mk_optional(
						abnf_mk_element_group(
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_more(
										abnf_mk_element_group(
											abnf_add_alternation(
												abnf_add_concatenation(
													abnf_mk_once(
														abnf_mk_element_char('.')
													),
												abnf_add_concatenation(
													abnf_mk_more(
														abnf_mk_element_rule(abnf_mk_str("HEXDIG"))
													),
													NULL  /* end of concatenations */
												)),
												NULL  /* end of alternations */
											)
										)
									),
									NULL  /* end of concatenations */
								),
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_char('-')
									),
								abnf_add_concatenation(
									abnf_mk_more(
										abnf_mk_element_rule(abnf_mk_str("HEXDIG"))
									),
									NULL  /* end of concatenations */
								)),
								NULL  /* end of alternations */
							))
						)
					),
					NULL  /* end of concatenations */
				))),
				NULL  /* end of alternations */
			),
		abnf_add_rule(
			abnf_mk_str("prose-val"),
			abnf_add_alternation(
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_char('<')
					),
				abnf_add_concatenation(
					abnf_mk_any(
						abnf_mk_element_group(
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_range(0x20, 0x3D)
									),
									NULL  /* end of concatenations */
								),
							abnf_add_alternation(
								abnf_add_concatenation(
									abnf_mk_once(
										abnf_mk_element_range(0x3F, 0x7E)
									),
									NULL  /* end of concatenations */
								),
								NULL  /* end of alternations */
							))
						)
					),
				abnf_add_concatenation(
					abnf_mk_once(
						abnf_mk_element_char('>')
					),
					NULL  /* end of concatenations */
				))),
				NULL  /* end of alternations */
			),
			next   /* end of rules */
		)))))))))))))))))))));

	abnf_rule_assign_origin(rule_list, next, abnf_mk_str("RFC2234 ABNF"));

	return rule_list;
}

