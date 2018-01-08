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

/* export to ABNF */
inline static void abnf_print_rep_char(FILE *stream, char c, int n) {
	for (; n > 0; n--) {
		fprintf(stream, "%c", c);
	}
}

static void abnf_print_abnf_element(FILE *stream, struct abnf_element *e, int already_in_group);
static void abnf_print_abnf_alternations(FILE *stream, struct abnf_alternation *pa) {
	struct abnf_concatenation *pc;
	int ia, na, ic, nc;
	na = abnf_alternation_count(pa);
	for (ia = 0; pa; pa = pa->next, ia++) {
		if (ia > 0) fprintf(stream, " / ");
		nc = abnf_concatenation_count(pa->concatenation);
		if (na > 1 && nc > 1) fprintf(stream, "( ");
		for (ic = 0, pc = pa->concatenation; pc; ic++, pc = pc->next) {
			if (ic > 0) fprintf(stream, " ");
			if (ABNF_IS_OPTIONAL(pc->repetition))
				fprintf(stream, "[ ");
			else if (!ABNF_IS_ONCE(pc->repetition)) {
				if (pc->repetition.min > 0) fprintf(stream, "%u", pc->repetition.min);
				fprintf(stream, "*");
				if (pc->repetition.max != ABNF_INFINITY) fprintf(stream, "%u", pc->repetition.max);
			}
			abnf_print_abnf_element(stream, &pc->repetition.element, ABNF_IS_OPTIONAL(pc->repetition));
			if (ABNF_IS_OPTIONAL(pc->repetition)) fprintf(stream, " ]");
		}
		if (na > 1 && nc > 1) fprintf(stream, " )");
	}
}

static void abnf_print_abnf_element(FILE *stream, struct abnf_element *e, int already_in_group) {
	int i, j, n, na, nc;
	switch (e->type) {
		case ABNF_ET_RULE:
			fprintf(stream, "%.*s", e->u.rule.name.len, e->u.rule.name.s);
			break;
		case ABNF_ET_GROUP:
			na = abnf_alternation_count(e->u.group);
			if (e->u.group)
				nc = abnf_concatenation_count(e->u.group->concatenation);
			else
				nc = 0;
			if (!already_in_group && (na > 1 || (na == 1 && nc > 1))) fprintf(stream, "( "); /* abnf_print_abnf_alternation won't add parenthesis */
				abnf_print_abnf_alternations(stream, e->u.group);
			if (!already_in_group && (na > 1 || (na == 1 && nc > 1))) fprintf(stream, " )"); /* abnf_print_abnf_alternation won't add parenthesis */
			break;
		case ABNF_ET_RANGE:
			if (e->u.range.lo == e->u.range.hi) {
				if (ABNF_IS_ALPHA(e->u.range.lo) || !ABNF_IS_VALID_TOKEN_CHAR(e->u.range.lo) ) {
					fprintf(stream, "%%x%.2x", e->u.range.lo);
				}
				else
					fprintf(stream, "\"%c\"", e->u.range.lo);
			}
			else {
					fprintf(stream, "%%x%.2x-%.2x", e->u.range.lo, e->u.range.hi);
			}
			break;
		case ABNF_ET_STRING:
			fprintf(stream, "%%x");
			for (i=0; i < e->u.string.len; i++) {
				if (i > 0) fprintf(stream, ".");
				fprintf(stream, "%.2x", (unsigned char) e->u.string.s[i]);
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
				if (ABNF_IS_VALID_TOKEN_CHAR(e->u.token.s[i])) {
					for (i++; i < e->u.token.len && ABNF_IS_VALID_TOKEN_CHAR(e->u.token.s[i]); i++);
					fprintf(stream, "\"%.*s\"", i-j, e->u.token.s + j);
				}
				else {
					fprintf(stream, "%%x");
					for (i++; i < e->u.token.len && !ABNF_IS_VALID_TOKEN_CHAR(e->u.token.s[i]); i++) {
						if (i > j) fprintf(stream, ".");
						fprintf(stream, "%.2x", (unsigned char) e->u.token.s[i]);
					}
				}
			}
			if (n > 1) fprintf(stream, " )");
			break;

		default:
			;
	}
}

void abnf_print_abnf_rules(FILE *stream, struct abnf_rule *rules, struct abnf_print_info *info) {
	int max_rule_len;
	struct abnf_rule *pr;
	struct abnf_print_comment comment_def = {.pre_comment = NULL, .line_comment = "; ", .post_comment = NULL};

	abnf_print_header(stream, info, &comment_def);

	for (pr = rules, max_rule_len = 0; pr; pr = pr->next) {
		if (pr->name.len > max_rule_len)
			max_rule_len = pr->name.len;
	}
	for (pr = rules; pr; pr = pr->next) {
		fprintf(stream, "%.*s", pr->name.len, pr->name.s);
		abnf_print_rep_char(stream, ' ', max_rule_len-pr->name.len);
		fprintf(stream, " = ");
		abnf_print_abnf_alternations(stream, pr->alternation);
		fprintf(stream, "\n");
	}
}
