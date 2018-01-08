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

%%{
	# TODO: in case of missing terminating CRLF or a char then final rulename is not processed
	# the pending action '%' is not called in case that is final action of a machine
	# to force calling it's necessary to create also final error action '%^' and duplicate code or %/ is enough ???
	# maybe "write eof" plus EOF state of rulename_scan is not adjusted correctly

	# write your name
	machine abnf_reader;
	alphtype char;

	action all {
		if (abnf_stop_flag) fbreak;
//		DBG("$");
	}

	action add_group {
		struct abnf_concatenation *pc;
		pc = top_stack.conc_prev;
		DBG("add_group");
		top_element.type = ABNF_ET_GROUP;
		top_element.u.group = NULL;
		if (alternation_count < ABNF_MAX_ALTERNATION-1) {
			DBG_STACK("AG++");
			alternation_count++;
			top_stack.top = &pc->repetition.element.u.group;
			top_stack.prev = NULL;
			top_stack.conc_prev = NULL;
		}
		else {
			fprintf(stderr, "alternation stack overflow\n");
			fbreak;
		}
		fhold;
		fcall alternation;
	}

	action add_rule {
		DBG("add_rule");
		if (alternation_count < ABNF_MAX_ALTERNATION-1) {
			struct abnf_rule *pr;

			DBG_STACK("AR++");
			alternation_count++;

			pr = abnf_find_rule(*rules, last_rulename);
			// fprintf(stderr, "adding rule:'%.*s'\n", last_rulename.len, last_rulename.s);
			if (!assign_rule_flag) {  /* "=/" */
				DBG("finding rule");
				if (!pr) DBG("not found");
			}
			else {    /* "=" */
				if (pr) {
					fprintf(stderr, "WARNING: overwriting rule '%.*s', comming from '%.*s' by '%.*s'\n",
						last_rulename.len, last_rulename.s,
						pr->origin.len, pr->origin.s,
						origin.len, origin.s);
					if (pr == last_rule) {
						last_rule = last_rule->prev;  /* last_rule is != NULL because pr!= NULL, if ->prev nil then no item left */
					}
					abnf_remove_list_item(*rules, pr);
					abnf_destroy_rules(pr);
					pr = NULL;  /* "=" */
				}
			}
			if (!pr) {
				pr = abnf_add_rule(
					abnf_dupl_str(last_rulename),
					NULL,
					NULL
				);
				if (!pr) fbreak;
				if (origin.len) {
					abnf_rule_assign_origin(pr, NULL, abnf_dupl_str(origin));
				}
				if (!last_rule) {
					*rules = pr;

				}
				else {
					last_rule->next = pr;
					pr->prev = last_rule;
				}
				last_rule = pr;
			}

			top_stack.top = &pr->alternation;
			if (pr->alternation) {
				for (top_stack.prev = pr->alternation;
					top_stack.prev->next;
					top_stack.prev = top_stack.prev->next);
			}
			else {
				top_stack.prev = NULL;
			}
			top_stack.conc_prev = NULL;
		}
		else {
			fprintf(stderr, "alternation stack overflow\n");
			fbreak;
		}
		fhold;
		fcall alternation;
	}

	action add_alternation {
		struct abnf_alternation *pa;
		DBG("add_alternation");
		pa = abnf_add_alternation(NULL, NULL);
		if (!pa) fbreak;
		if (top_stack.prev == NULL) {
			*(top_stack.top) = pa;
		}
		else {
			top_stack.prev->next = pa;
			pa->prev = top_stack.prev;
		}
		top_stack.prev = pa;
		top_stack.conc_prev = NULL;
	}

	action add_concatenation {
		struct abnf_concatenation *pc;
		struct abnf_element r;
		DBG("add_concatenation");
		r.type = ABNF_ET_NONE;
		pc = abnf_add_concatenation(abnf_mk_repetition(r, 1, 1), NULL);
		if (!pc) fbreak;
		if (top_stack.conc_prev == NULL) {
			top_stack.prev->concatenation = pc;
		}
		else {
			top_stack.conc_prev->next = pc;
			pc->prev = top_stack.conc_prev;
		}
		top_stack.conc_prev = pc;
		val_flag = 0;
		last_val = 0; last_val_mult = 10;
	}

	action set_value {
		top_element = abnf_mk_element_range(last_val, last_val);
		last_val = 0;
	}

	action set_range {
		top_element.u.range.hi = last_val;
	}

	action add_char {
		if (top_element.type == ABNF_ET_RANGE) {
			/* change range to string */
			struct abnf_str s;
			char buff[2];
			s.len = sizeof(buff);
			s.s = buff;
			buff[0] = top_element.u.range.lo;
			buff[1] = (char) last_val;
			top_element = abnf_mk_element_string(abnf_dupl_str(s));
			if (!top_element.u.string.s) fbreak;
		}
		else {
			/* prev alloc was sucessfull otherwise called fbreak */
			char *p;
			p = abnf_realloc(top_element.u.string.s, top_element.u.string.len+1);
			if (!p) fbreak;
			p[top_element.u.string.len++] = last_val;
			top_element.u.string.s = p;
		}
		last_val = 0;
	}

	# generated rules, define required actions
	ALPHA = 0x41..0x5a | 0x61..0x7a;
	BIT = "0" | "1"
		${last_val = (last_val * last_val_mult) + ((*fpc)-'0');};
	CHAR = 0x01..0x7f;
	CR = "\r";
	LF = "\n" > { line++;};
	# hack: rfc2234 does not support LF terminated lines
	CRLF = (CR LF) | LF;
	CTL = 0x00..0x1f | 0x7f;
	DIGIT = 0x30..0x39 ${last_val = (last_val * last_val_mult) + ((*fpc)-'0'); val_flag = 1;};
	DQUOTE = "\"";
	HEXDIG =
		DIGIT
		| ( "A"i | "B"i | "C"i | "D"i | "E"i | "F"i) ${
			last_val = (last_val * last_val_mult) + (10+(*fpc)-(((*fpc)>='A' && (*fpc)<='F')?'A':'a'));
		};
	HTAB = "\t";
	SP = " ";
	WSP = SP | HTAB;
	LWSP = ( WSP | ( CRLF WSP ) )*;
	OCTET = 0x00..0xff;
	VCHAR = 0x21..0x7e;

	rulename_scan := |*

		(ALPHA | DIGIT | "-")+ => {
			DBG("rulename_scan-A");
			last_rulename.s = ts;
			last_rulename.len = te-ts;
			fret;
		};

		any => {
			fhold;
			fret;
		};
	*|;

#   rulename = ALPHA ( ALPHA | DIGIT | "-" )*;
	rulename = ALPHA >{
		last_rulename.len = 0;
		fhold;
		DBG("call-rulename_scan");
		fcall rulename_scan;
	};

	comment = ";" ( WSP | VCHAR )* CRLF;
	c_nl = comment | CRLF;

	# plain (cn_l WSP) is ambigious, CRLF is eaten also in case that WSP won't follow
	# TODO seems does not work  "ABC =\r\n  %x10\n" (no space after equal)
	c_wsp_scan:= |*

		# consume spaces
		WSP+ => {
			DBG("c_wsp_scan-WSP");
		};

		#  consume regular c_wsp
		CRLF+ WSP => {
			DBG("c_wsp_scan-CRLR WSP");
		};

		# consume comments
		";" (WSP | VCHAR)* => {
			DBG("c_wsp_scan-\";\" CRLR WSP");
		};

		# false c_wsp, reduces multiple CRLF and hold last one if WSP won't follow
		CRLF+ => {
			if (*fpc == '\n') line--;
			fhold; /* set LF as next char */
			DBG("c_wsp_scan-CRLF");
			fret;
		};

		# other
		any => {
			fhold;
			DBG("c_wsp_scan-ANY");
			fret;
		};
	*|;

	c_wsp = ( WSP | ";" | CRLF) @{
		fhold;
		DBG("call c_wsp_scan");
		fcall c_wsp_scan;
	};

	# "/" ambigious with alternation delimiter "/" ???
	equal_scan:= |*
		"=/" => {
			assign_rule_flag = 0;
			//fhold;
			DBG("equal_scan-'=/'");
			fret;
		};
		"=" => {
			assign_rule_flag = 1;
			//fhold;
			DBG("equal_scan-'='");
			fret;
		};
		any => {
			fhold;
			fret;
		};
	*|;

	repeat =
		DIGIT+ %{
			if (val_flag) {
				top_stack.conc_prev->repetition.min = last_val;
				top_stack.conc_prev->repetition.max = last_val;
			}
		}
		| (
			DIGIT* "*" >{
				top_stack.conc_prev->repetition.min = val_flag?last_val:0;
				val_flag = 0;
				last_val = 0;
				top_stack.conc_prev->repetition.max = ABNF_INFINITY;
			}
			DIGIT* %{
				if (val_flag) {
					top_stack.conc_prev->repetition.max = last_val;
				}
			}
		);
#	group = "(" c_wsp* alternation c_wsp* ")";
	group =
		"("
		c_wsp?
		any >add_group
		c_wsp?
		")";
#	option = "[" c_wsp* alternation c_wsp* "]";
	option =
		"[" >{top_stack.conc_prev->repetition.min = 0; top_stack.conc_prev->repetition.max = 1;}
		c_wsp?
		any >add_group
		c_wsp?
		"]";
	char_val =
		DQUOTE
		( 0x20..0x21 | 0x23..0x7e )*
			>{token_flag = 1; last_str.s = fpc;}
			% {last_str.len = fpc-last_str.s;}
		DQUOTE;
	bin_val =
		"b"i >{last_val_mult=2;}
		BIT+ %set_value %^set_value %/set_value
		( ( "." BIT+ %add_char %^add_char %/add_char)+
		| ( "-" BIT+ %set_range %^set_range %/set_range)
		)?;
	dec_val =
		"d"i >{last_val_mult=10;}
		DIGIT+ %set_value %^set_value %/set_value
		( ( "." DIGIT+ %add_char %^add_char %/add_char)+
		| ( "-" DIGIT+ %set_range %^set_range %/set_range)
		)?;
	hex_val =
		"x"i >{last_val_mult=16;}
		HEXDIG+ %set_value %^set_value %/set_value
		( ( "." HEXDIG+ %add_char %^add_char %/add_char)+
		| ( "-" HEXDIG+ %set_range %^set_range %/set_range)
		)?;
	num_val =
		"%" >{last_val = 0;}
		( bin_val | dec_val | hex_val );
	prose_val =
		"<"
		( 0x20..0x3d | 0x3f..0x7e )*
			>{token_flag = 0; last_str.s = fpc;}
			%{last_str.len = fpc-last_str.s;}
		">";
	element =
		rulename
			%{top_element = abnf_mk_element_rule(abnf_dupl_str(last_rulename));}
			%^{top_element = abnf_mk_element_rule(abnf_dupl_str(last_rulename)); DBG("%!rulename");}
			%/{top_element = abnf_mk_element_rule(abnf_dupl_str(last_rulename)); DBG("%/rulename");}
		| group
		| option
		| char_val
			%{top_element = abnf_mk_element_token(abnf_dupl_str(last_str));}
			%^{top_element = abnf_mk_element_token(abnf_dupl_str(last_str));DBG("%!charval");}
			%/{top_element = abnf_mk_element_token(abnf_dupl_str(last_str));DBG("%/charval");}
		| num_val
		| ( prose_val
			%{top_element = abnf_mk_element_string(abnf_dupl_str(last_str));}
			%^{top_element = abnf_mk_element_string(abnf_dupl_str(last_str));DBG("%!proseval");}
			%/{top_element = abnf_mk_element_string(abnf_dupl_str(last_str));DBG("%/proseval");}
			)
		;
	repetition = ( repeat? element ) >add_concatenation;
#	concatenation = repetition ( c_wsp+ repetition )*;
	concatenation = (repetition ( c_wsp repetition )* ) >add_alternation;
#	alternation:= concatenation ( c_wsp* "/" c_wsp* concatenation )*;
	alternation:=
		(
			(concatenation ( c_wsp? "/" c_wsp? concatenation )*) $!{
					alternation_count--;
					DBG_STACK("AC--");
					fhold;
					DBG("$!RETAL");
					fret;
				}
				%/ {
					DBG_STACK("AT--");
				}
		) $all;
#	elements = alternation c_wsp*;
	elements = any >add_rule c_wsp?;
	# defined_as is called as new machine to avoid ragel optimization of c_wsp due missing alternation rule
	defined_as =
		c_wsp?
		"=" @{ fhold; fcall equal_scan; }
		c_wsp?;
#	rule = rulename defined_as elements c_nl;
	# ":>" entry guarded concatenation to give higher priority to n_nl and avoid endless loop
	rule = rulename defined_as elements :> c_nl;


#	rulelist = ( rule | ( c_wsp* c_nl ) )+;
	# ":>" entry guarded concatenation to give higher priority to n_nl and avoid endless loop
	rulelist = ( rule | ( c_wsp? :> c_nl ) )+;

	# instantiate machine rules
	main:= rulelist $all;
}%%

int abnf_parse_abnf(FILE* in_stream, struct abnf_rule** rules, struct abnf_str origin) {
    #define ABNF_BUFF_CHUNK 1024
	#define ABNF_MAX_ALTERNATION 50
	#define MAX_ERR_LIST_LEN 200
	#define top_stack alternation_stack[alternation_count-1]
	#define top_element top_stack.conc_prev->repetition.element
	#define DBG(_s_) { \
	/*	fprintf(stderr, "%s: #%d: cs: %d, top: %d: st+0:%d, st-1:%d, tok:%d, line: %d, (%d): '%.10s'\n", (_s_), __LINE__, cs, top, stack[top], top>0?stack[top-1]:-1, tokend-tokstart, line, *p, p); */ \
	}
	#define DBG_STACK(_s_) { \
	/*	fprintf(stderr, "%s:%d  line: %d, '%.10s'\n", (_s_), alternation_count, line, p); */ \
	}

	%% write data noerror;

	char *p, *pe, *buff, *ts, *te, *eof;
	int cs, top, stack[50], act;
	struct abnf_str last_rulename, last_str;
	unsigned int last_val, last_val_mult;
	int assign_rule_flag, token_flag, val_flag;
	struct abnf_rule *last_rule = NULL;
	unsigned int line;
	struct {
		struct abnf_alternation **top;
		struct abnf_alternation *prev;
		struct abnf_concatenation *conc_prev;
	} alternation_stack[ABNF_MAX_ALTERNATION];
	unsigned int alternation_count = 0;

	size_t i, n;

	/* read file into buffer */
	buff = NULL;
	n = 0;
	do {
		p = abnf_realloc(buff, n + ABNF_BUFF_CHUNK);
		if (!p) {
			if (buff) abnf_free(buff);
			return -1;
		}
		buff = p;
		i = fread(buff+n, sizeof(*buff), ABNF_BUFF_CHUNK, in_stream);
		n += i;
	} while (i == ABNF_BUFF_CHUNK);
	/* ABNF defines line ends as CRLF so adjust CR / LF */
	pe = buff + n;
	p = buff;

	%% write init;
	line = 1;
	/* unwarnings */
	assign_rule_flag = token_flag = val_flag = 0;
	last_val = last_val_mult = 0;
	last_rulename.s = last_str.s = 0;
	last_rulename.len = last_str.len = 0;
	i = abnf_reader_en_rulename_scan;
	i = abnf_reader_en_c_wsp_scan;
	i = abnf_reader_en_equal_scan;
	i = abnf_reader_en_alternation;
	i = abnf_reader_en_main;

	for (last_rule = *rules; last_rule && last_rule->next; last_rule=last_rule->next);

	%% write exec;

	if (cs < abnf_reader_first_final) {
		fprintf(stderr, "origin:'%.*s', line: %u, cs: %d, rule parsing error at %d\n", origin.len, origin.s, line, cs, p-buff);
		if (pe-p<=MAX_ERR_LIST_LEN) {
			fprintf(stderr, "%.*s\n", pe-p, p);
		}
		else {
			fprintf(stderr, "%.*s\n.......and %d chars continue\n", MAX_ERR_LIST_LEN, p, pe-p-MAX_ERR_LIST_LEN);
		}
	}
	else if (alternation_count > 1) {  /* BUG?: it should be zero but it's permanently 1 */
		fprintf(stderr, "stack is not empty, ac: %d, top: %d, cs: %d\n", alternation_count, top, cs);
	}
	abnf_free(buff);

	return 0;
}
