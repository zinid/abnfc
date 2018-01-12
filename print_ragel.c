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


/* export to ragel */
#define ABNF_IS_VALID_OR_ESCAPABLE_CHAR(_c_) ( ((_c_)>=0x20 && (_c_)<=0x7e) || \
		(_c_)=='\0' || (_c_)=='\a' || (_c_)=='\b' || (_c_)=='\t' || \
		(_c_)=='\n' || (_c_)=='\v' || (_c_)=='\f' || (_c_)=='\r' )

static char* abnf_escape_char(char c) {
	static char buff[5];
	if (ABNF_IS_VALID_OR_ESCAPABLE_CHAR(c)) {
		switch (c) {
			case '\0':
				return "\\0";
			case '\a':
				return "\\a";
			case '\b':
				return "\\b";
			case '\t':
				return "\\t";
			case '\n':
				return "\\n";
			case '\v':
				return "\\v";
			case '\f':
				return "\\f";
			case '\r':
				return "\\r";
			case '"':
				return "\\\"";
			case '\\':
				return "\\";
			default:
				buff[0] = c;
				buff[1] = '\0';
				break;
		}
	}
	else {
		snprintf(buff, sizeof(buff)-1, "%%x%.2x", (unsigned char) c);
	}
	return buff;
}

static char* abnf_get_ragel_rule_name(struct abnf_str s, struct abnf_rule *rules) {
	static char buff[100];
	struct abnf_rule *pr;
	unsigned int i;
	char *reserved[] = {"any", "ascii", "extend", "alpha", "digit", "alnum", "lower", "upper",
					"xdigit", "cntrl", "graph", "print", "punct", "space", "null", "empty", NULL};
	/* get name as declared, name is case sensitive in ragel but unsensitive in abnf */
	if (rules) {
		pr = abnf_find_rule(rules, s);
		if (pr) s = pr->name;
	}
	/* make copy */
	if (s.len > sizeof(buff)-1) s.len = sizeof(buff)-1;
	memcpy(buff, s.s, s.len);
	buff[s.len] = '\0';
	/* change '-' to '_' */
	for (i=0; i<s.len; i++) {
		if (buff[i] == '-') buff[i] = '_';
	}
	/* there are built in rules in ragel, add suffix '_' is name is in conflict */
	for (i=0; reserved[i]; i++) {
		if (strcmp(reserved[i], buff) == 0) {
			buff[s.len] = '_';
			s.len++;
			buff[s.len] = '\0';
		}
	}
	return buff;
}

static void abnf_print_ragel_element(FILE *stream, struct abnf_rule *pr, struct abnf_element *e);
static void abnf_print_ragel_alternations(FILE *stream, struct abnf_rule *pr, struct abnf_alternation *pa) {
	struct abnf_concatenation *pc;
	int ia, na, ic, nc;
	na = abnf_alternation_count(pa);
	for (ia = 0; pa; pa = pa->next, ia++) {
		if (ia > 0) fprintf(stream, " | ");
		nc = abnf_concatenation_count(pa->concatenation);
		if (na > 1 && nc > 1) fprintf(stream, "( ");
		for (ic = 0, pc = pa->concatenation; pc; ic++, pc = pc->next) {
			if (ic > 0) fprintf(stream, " "); /* or '.' */
			abnf_print_ragel_element(stream, pr, &pc->repetition.element);
			if (ABNF_IS_OPTIONAL(pc->repetition))
				fprintf(stream, "?");
			else if (ABNF_IS_ANY(pc->repetition))
				fprintf(stream, "*");
			else if (ABNF_IS_MORE(pc->repetition))
				fprintf(stream, "+");
			else if (!ABNF_IS_ONCE(pc->repetition)) {
				fprintf(stream, "{");
				if (pc->repetition.min > 0) fprintf(stream, "%u", pc->repetition.min);
				if (pc->repetition.min != pc->repetition.max) {
					fprintf(stream, ",");
					if (pc->repetition.max != ABNF_INFINITY) fprintf(stream, "%u", pc->repetition.max);
				}
				fprintf(stream, "}");
			}
		}
		if (na > 1 && nc > 1) fprintf(stream, " )");
	}
}

static void abnf_print_ragel_element(FILE *stream, struct abnf_rule *pr, struct abnf_element *e) {
	int i, j, n, na, nc;
	switch (e->type) {
		case ABNF_ET_RULE:
			fprintf(stream, "%s", abnf_get_ragel_rule_name(e->u.rule.name, pr));
			break;
		case ABNF_ET_GROUP:
			na = abnf_alternation_count(e->u.group);
			if (e->u.group)
				nc = abnf_concatenation_count(e->u.group->concatenation);
			else
				nc = 0;
			if (na > 1 || (na == 1 && nc > 1)) fprintf(stream, "( "); /* abnf_print_ragel_alternation won't add parenthesis */
				abnf_print_ragel_alternations(stream, pr, e->u.group);
			if (na > 1 || (na == 1 && nc > 1)) fprintf(stream, " )"); /* abnf_print_ragel_alternation won't add parenthesis */
			break;
		case ABNF_ET_RANGE:
			if (e->u.range.lo == e->u.range.hi) {
				if (ABNF_IS_ALPHA(e->u.range.lo) || !ABNF_IS_VALID_OR_ESCAPABLE_CHAR(e->u.range.lo) ) {
					fprintf(stream, "0x%.2x", e->u.range.lo);
				}
				else
					fprintf(stream, "\"%s\"", abnf_escape_char(e->u.range.lo));
			}
			else {
				/* if (ABNF_IS_VALID_OR_ESCAPABLE_CHAR(e->u.range.lo) && ABNF_IS_VALID_OR_ESCAPABLE_CHAR(e->u.range.hi)) {
					fprintf(stream, "\"%c\"..\"%c\"", e->u.range.lo, e->u.range.hi);
				}
				else */
					fprintf(stream, "0x%.2x..0x%.2x", e->u.range.lo, e->u.range.hi);
			}
			break;
		case ABNF_ET_STRING:
			for (i=0; i < e->u.string.len; i++) {
				if (i > 0) fprintf(stream, ".");
				fprintf(stream, "0x%.2x", (unsigned char) e->u.string.s[i]);
			}
			break;
		case ABNF_ET_TOKEN:
			for (i=0, n=0; i < e->u.token.len; n++) {
				if (ABNF_IS_VALID_TOKEN_CHAR(e->u.token.s[i])) {
					for (i++; i < e->u.token.len && ABNF_IS_VALID_TOKEN_CHAR(e->u.token.s[i]); i++);
				}
				else {
					for (i++; i < e->u.token.len && !ABNF_IS_VALID_TOKEN_CHAR(e->u.token.s[i]); i++);
				}
			}
			if (n > 1) fprintf(stream, "( ");
			for (i=0; i < e->u.token.len; ) {
				if (i > 0) fprintf(stream, " ");
				j = i;
				if (ABNF_IS_VALID_OR_ESCAPABLE_CHAR(e->u.token.s[i])) {
					int alpha_fl = 0;
					fprintf(stream, "\"");
					do {
						if (ABNF_IS_ALPHA(e->u.token.s[i]))
							alpha_fl = 1;
						fprintf(stream, "%s", abnf_escape_char(e->u.token.s[i]));
						i++;
					} while (ABNF_IS_VALID_OR_ESCAPABLE_CHAR(e->u.token.s[i]) && i < e->u.token.len);
					fprintf(stream, "\"");
					if (alpha_fl)
						fprintf(stream, "i");
				}
				else {
					for (i++; i < e->u.token.len && !ABNF_IS_VALID_TOKEN_CHAR(e->u.token.s[i]); i++) {
						if (i > j) fprintf(stream, ".");
						fprintf(stream, "0x%.2x", (unsigned char) e->u.token.s[i]);
					}
				}
			}
			if (n > 1) fprintf(stream, " )");
			break;

		default:
			;
	}
}

void abnf_print_ragel_rules(FILE *stream, struct abnf_rule *rules, struct abnf_print_info *info,
			    char *machine_name, int instantiate) {
	struct abnf_rule *pr, *last_pr;
	struct abnf_print_comment comment_def = {.pre_comment = NULL, .line_comment = "# ", .post_comment = NULL};

	abnf_print_header(stream, info, &comment_def);

	fprintf(stream, "%%%%{\n");
	fprintf(stream, "\t# write your name\n\tmachine %s;\n\n\t# generated rules, define required actions\n", machine_name);
	last_pr = NULL;
	for (pr = rules; pr; pr = pr->next) {
		fprintf(stream, "\t%s = ", abnf_get_ragel_rule_name(pr->name, NULL));
		abnf_print_ragel_alternations(stream, pr, pr->alternation);
		fprintf(stream, ";\n");
		last_pr = pr;
	}
	if (instantiate) {
	  fprintf(stream, "\n\t# instantiate machine rules\n");
	  if (last_pr)
	    fprintf(stream, "\tmain:= %s;\n", abnf_get_ragel_rule_name(last_pr->name, NULL));
	  else
	    fprintf(stream, "\t# main:= <rule_name>;\n");
	}
	fprintf(stream, "}%%%%\n");
}

