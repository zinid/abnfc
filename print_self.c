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

/* export to self structures of abnfc
 */
inline static void abnf_print_rep_char(FILE *stream, char c, int n) {
	for (; n > 0; n--) {
		fprintf(stream, "%c", c);
	}
}

inline static void findent(FILE *stream, int indent) {
	for (; indent>0; indent--) fprintf(stream, "\t");
}

/* warning two commands if used in blockless place */
#define findentf findent(stream, indent); fprintf

static void abnf_print_self_element(FILE *stream, struct abnf_element *e, int indent);
static void abnf_print_self_alternations(FILE *stream, struct abnf_alternation *pa, int indent) {
	struct abnf_concatenation *pc;
	int ia, ic;
	if (!pa) {
		findentf(stream, "NULL");   /* no CRLF */
	}
	else {
		for (ia = 0; pa; pa = pa->next, ia++) {
			findentf(stream, "abnf_add_alternation(\n");
			indent++;
			if (!pa->concatenation) {
				findentf(stream, "NULL,\n");
			}
			else {
				for (ic = 0, pc = pa->concatenation; pc; ic++, pc = pc->next) {
					int fl = 0;
					findentf(stream, "abnf_add_concatenation(\n");
					indent++;
					if (ABNF_IS_OPTIONAL(pc->repetition)) {
						findentf(stream, "abnf_mk_optional(\n");
					} else if (ABNF_IS_ONCE(pc->repetition)) {
						findentf(stream, "abnf_mk_once(\n");
					} else if (ABNF_IS_ANY(pc->repetition)) {
						findentf(stream, "abnf_mk_any(\n");
					} else if (ABNF_IS_MORE(pc->repetition)) {
						findentf(stream, "abnf_mk_more(\n");
					} else {
						fl = 1;
						findentf(stream, "abnf_mk_repetition(\n");
					}
					indent++;
					abnf_print_self_element(stream, &pc->repetition.element, indent);

					if (fl) {
						findentf(stream, ", ");
						if (pc->repetition.min == ABNF_INFINITY)
							fprintf(stream, "ABNF_INFINITY, ");
						else
							fprintf(stream, "%d, ", pc->repetition.min);
						if (pc->repetition.max == ABNF_INFINITY)
							fprintf(stream, "ABNF_INFINITY\n");
						else
							fprintf(stream, "%d\n", pc->repetition.max);
					}
					indent--;
					findentf(stream, "),\n");
					indent--;
				}
				indent++;
				findentf(stream, "NULL  /* end of concatenations */\n");
				indent--;
				findent(stream, indent);
				for (; ic > 0; ic--) fprintf(stream, ")");  /* no CRLF */
				fprintf(stream, ",\n");
    		}
			indent--;
		}
		indent++;
		findentf(stream, "NULL  /* end of alternations */\n");
		indent--;
		findent(stream, indent);
		for (; ia > 0; ia--) fprintf(stream, ")");
		/* no CRLF */
	}
}

static void print_char(FILE *stream, unsigned char c) {
	switch (c) {
		case '\r': fprintf(stream, "'\\r'"); return;
		case '\n': fprintf(stream, "'\\n'"); return;
		case '\t': fprintf(stream, "'\\t'"); return;
		case '\'': fprintf(stream, "'\\'"); return;
		default:
		;
	}
	if (c >= ' ' && c <= 0x7e)
		fprintf(stream, "'%c'", c);
	else
		fprintf(stream, "0x%.2x", c);
}

static void print_string(FILE *stream, struct abnf_str s) {
	int i;
	if (s.len == 0) {
		fprintf(stream, "NULL");
	}
	else {
		fprintf(stream, "\"");
		for (i=0; i<s.len; i++) {
			if (s.s[i] >= ' ' && s.s[i] <= 0x7E) {
				fprintf(stream, "%c", s.s[i]);
			}
			else {
				switch (s.s[i]) {
					case '\r': fprintf(stream, "\\r"); break;
					case '\n': fprintf(stream, "\\n"); break;
					case '\t': fprintf(stream, "\\t"); break;
					case '\"': fprintf(stream, "\\\""); break;
					case '\0': fprintf(stream, "\\0"); break;
					default:
						fprintf(stream, "\\%.2x", (unsigned char) s.s[i]);
						break;
				}
			}
		}
		fprintf(stream, "\"");
	}
}

static void abnf_print_self_element(FILE *stream, struct abnf_element *e, int indent) {
	switch (e->type) {
		case ABNF_ET_RULE:
			findentf(stream, "abnf_mk_element_rule(abnf_mk_str(");
			print_string(stream, e->u.rule.name);
			fprintf(stream, "))\n");
			break;
		case ABNF_ET_GROUP:
			findentf(stream, "abnf_mk_element_group(\n");
			abnf_print_self_alternations(stream, e->u.group, indent+1);
			fprintf(stream, "\n");
			findentf(stream, ")\n");
			break;
		case ABNF_ET_RANGE:
			if (e->u.range.lo == e->u.range.hi) {
				findentf(stream, "abnf_mk_element_char(");
			}
			else {
				findentf(stream, "abnf_mk_element_range(");
				print_char(stream, e->u.range.lo);
				fprintf(stream, ", ");
			}
			print_char(stream, e->u.range.hi);
			fprintf(stream, ")\n");
			break;
		case ABNF_ET_STRING:
			findentf(stream, "abnf_mk_element_string(abnf_mk_str(");
			print_string(stream, e->u.string);
			fprintf(stream, "))\n");
			break;
		case ABNF_ET_TOKEN:
			findentf(stream, "abnf_mk_element_token(abnf_mk_str(");
			print_string(stream, e->u.string);
			fprintf(stream, "))\n");
			break;

		default:
			findentf(stream, "abnf_mk_element_empty()\n");
			break;
	}
}

void abnf_print_self_rules(FILE *stream, struct abnf_rule *rules, struct abnf_print_info *info) {
	int ir, indent;
	struct abnf_rule *pr;
	struct abnf_print_comment comment_def = {.pre_comment = "/*\n", .line_comment = " * ", .post_comment = " */\n"};

	abnf_print_header(stream, info, &comment_def);

	indent = 0;
	findentf(stream, "#include \"abnf.h\"\n");
	findentf(stream, "struct abnf_rule* abnf_declare_custom_rules(struct abnf_rule* next) {\n");
	fprintf(stream, "\n");
	indent++;
	findentf(stream, "struct abnf_rule *rule_list;\n");
	fprintf(stream, "\n");

	findentf(stream, "rule_list = \n");
	indent++;
	if (!rules) {
		findentf(stream, "NULL;\n");
	}
	else {

		for (pr = rules, ir = 0; pr; pr = pr->next, ir++) {
			findentf(stream, "abnf_add_rule(\n");
			indent++;
			findentf(stream, "abnf_mk_str(\"%.*s\"),\n", pr->name.len, pr->name.s);
			abnf_print_self_alternations(stream, pr->alternation, 3);
			fprintf(stream, ",\n");
			indent--;
		}
		indent++;
		fprintf(stream, "\n");
		findentf(stream, "next  /* end of rules */\n");
		indent--;
		findent(stream, indent);
		for (; ir > 0; ir--) fprintf(stream, ")");
		fprintf(stream, ";\n");
	}
	indent--;
	fprintf(stream, "\n");
	findentf(stream, "/* abnf_rule_assign_origin(rule_list, next, abnf_mk_str(\"custom\")); */\n");
	fprintf(stream, "\n");
	findentf(stream, "return rule_list;\n");
	indent--;
	findentf(stream, "}\n");
}
