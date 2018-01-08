/*
 *  Copyright 2007 by Tomas Mandys <tomas.mandys at 2p dot cz>
 */

/*  This file is part of abnf tools.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "abnf.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <errno.h>

#define _GNU_SOURCE
#include <getopt.h>
#include <signal.h>
#include <string.h>

static int verbose = 0;

static void print_version() {
	printf("%s", NAME_S" - ABNF compiler, v"VERSION_S"\n");
}

static void print_help(char *name) {
	print_version();
	printf("Usage: %s [options] <file> [[options] <file> ...]\n", basename(name));
	printf("Following internal rule lists supported\n");
	printf("  'core': rfc2234 core rules\n");
	printf("  'abnf': rfc2234 ABNF rules\n");
	printf("  '-':    forces reading from stdin\n");
	printf("\n");
	printf("  -f format   format of output\n");
	printf("              'abnf':  print ABNF rules\n");
	printf("              'ragel': print Ragel rules (default)\n");
	printf("              'self':  print abnfc C rules\n");
	printf("  -o file     output file name, default: stdout\n");
	printf("  -t in_type  type of next input file\n");
	printf("              'file': load rules from file\n");
	printf("              'self': load internal rules (default)\n");
	printf("\n");
	printf("Common options:\n");
	printf("  -F          print output even an ABNF rule error is detected\n");
	printf("  -h,-H       print this help\n");
	printf("  -v          print more info to stderr\n");
	printf("  -V          print version\n");
	printf("\n");
	printf("Examples:\n");
	printf("  %s core rfc3261.txt -f ragel -F -o rfc3261.rl\n", basename(name));
	printf("  %s core - -f abnf\n", basename(name));
	printf("\n");
}

int abnf_stop_flag = 0;

static void sig_term(int signr) {
    abnf_stop_flag++;
	fprintf(stderr, "Signal (%d) detected\n", signr);
	if (signr != SIGINT || abnf_stop_flag > 1) { /* double ^C */
		exit(0);
	}
}
#define SIGNAL(sig) \
	if (signal(sig, sig_term) == SIG_ERR) {\
		printf("can't install signal handler for %s\n", #sig);\
		exit (-1);\
}

int main(int argc, char** argv) {

	#define MAX_IN_FILES 50

	enum {of_Default, of_Ragel, of_Abnf, of_Self} out_fmt = of_Default;
	enum {if_File, if_Internal} cur_in_fmt = if_Internal, in_flags[MAX_IN_FILES];
	static char short_opts[] = "+f:o:t:FhHvV";
	int i, c, in_file_count = 0, force_flag = 0;
	struct abnf_str in_files[MAX_IN_FILES];
	char *out_file = NULL;
	struct abnf_rule *rules = NULL, *pr;
	FILE *out_stream, *in_stream;
	struct abnf_print_info info;

	#define append_rule(_pr_) {\
		if (_pr_) { \
			if (rules == NULL) { \
				rules = (_pr_); \
			} \
			else { \
				struct abnf_rule *last_rule = NULL; \
				for (last_rule = rules; last_rule->next; last_rule=last_rule->next);\
				last_rule->next = (_pr_); \
				(_pr_)->prev = last_rule; \
			} \
		} \
	}

	/* look if there is a -h, e.g. -f -h construction won't catch it later */
	opterr = 0;
	while (optind < argc) {
		c = getopt(argc, argv, short_opts);
		if (optind > argc)
			break;
		if (c == 'h' || (optarg && strcmp(optarg, "-h") == 0)) {
			print_help(argv[0]);
			return 0;
		}
		if (c == -1) optind++;
	}
	optind = 1;  /* reset getopt */
	opterr = 0;
	while (optind < argc) {
		c = getopt(argc, argv, short_opts);
		if (optind > argc)
			break;
		if (c == -1) {
			if (in_file_count < MAX_IN_FILES) {
				in_files[in_file_count] = abnf_mk_str(argv[optind]);
				in_flags[in_file_count] = cur_in_fmt;
				in_file_count++;
			}
			optind++;
		}
		else {
			switch (c) {
				case 'f':
					if (out_fmt) {
						fprintf(stderr, "ERROR: only one -f options allowed\n");
						goto err;
					}
					if (strcasecmp("rl", optarg)==0 || strcasecmp("ragel", optarg)==0)
						out_fmt = of_Ragel;
					else if (strcasecmp("abnf", optarg)==0)
						out_fmt = of_Abnf;
					else if (strcasecmp("self", optarg)==0)
						out_fmt = of_Self;
					else {
						fprintf(stderr, "ERROR: unknown format '-f %s'\n", optarg);
						goto err;
					}
					break;
				case 't':
					if (strcasecmp("self", optarg)==0)
						cur_in_fmt = if_Internal;
					else if (strcasecmp("file", optarg)==0)
						cur_in_fmt = if_File;
					else {
						fprintf(stderr, "ERROR: unknown type '-t %s'\n", optarg);
						goto err;
					}
					break;
				case 'o':
					out_file = optarg;
					break;
				case 'F':
					force_flag++;
					break;
				case 'v':
					verbose++;
					break;
				case 'V':
					print_version();
					return 0;
				case 'h':
				case 'H':
				case '?':
					break;
				default:
					fprintf(stderr, "getopt returned character code 0x%x\n", c);
					break;
			}
		}
	}

	SIGNAL(SIGTERM);
	SIGNAL(SIGINT);
	SIGNAL(SIGQUIT);

	if (in_file_count == 0) {
		in_files[in_file_count] = abnf_mk_str("");  /* stdin */
		in_flags[in_file_count] = if_File;
		in_file_count++;
	}
	for (i=0; i < in_file_count; i++) {
		if (abnf_stop_flag) return 0;
		switch (in_flags[i]) {
			case if_File:
				/* stdin / file */
			try_file:
				if (!in_files[i].len || (strcmp(in_files[i].s, "-") == 0)) {
					if (verbose) fprintf(stdout, "infile: stdin\n");
					if (abnf_parse_abnf(stdin, &rules, in_files[i]) < 0) goto err_2;
				}
				else {
					if (verbose) fprintf(stdout, "infile: %s\n", in_files[i].s);
					in_stream = fopen(in_files[i].s, "r");  /* it's null terminated */
					if (!in_stream) {
						fprintf(stderr, "ERROR: %s (errno:%d)\n", strerror(errno), errno);
						goto err_2;
					}
					if (abnf_parse_abnf(in_stream, &rules, in_files[i]) < 0) goto err_2;
					fclose(in_stream);
				}
				break;
			case if_Internal:
				if (verbose) fprintf(stdout, "self: %s\n", in_files[i].s);
				if (strcasecmp("core", in_files[i].s) == 0) {  /* we can do it, it's null terminated */
					pr = abnf_declare_core_rules(NULL);
				}
				else if (strcasecmp("abnf", in_files[i].s) == 0) {
					pr = abnf_declare_abnf_rules(NULL);
				}
				else {
					goto try_file;
				}
				append_rule(pr);
				break;
			default:
				;
		}
	}
	if (abnf_stop_flag) return 0;

	if (out_file) {
		if (verbose) fprintf(stdout, "outfile: %s\n", out_file);
		out_stream = fopen(out_file, "w+");
	}
	else {
		if (verbose) fprintf(stdout, "outfile: stdout\n");
		out_stream = stdout;
	}
	if (abnf_check_rules(stderr, rules) != 0 && force_flag == 0) {
		return 3;
	}

	info.in_files = in_files;
	info.in_file_count = in_file_count;
	info.out_file = abnf_mk_str(out_file);
	switch (out_fmt) {
		case of_Default:
		case of_Ragel:
			if (verbose) fprintf(stdout, "outformat: ragel\n");
			abnf_resolve_rule_dependencies(stderr, &rules);
			abnf_print_ragel_rules(out_stream, rules, &info);
			break;
		case of_Abnf:
			if (verbose) fprintf(stdout, "outformat: abnf\n");
			abnf_print_abnf_rules(out_stream, rules, &info);
			break;
		case of_Self:
			if (verbose) fprintf(stdout, "outformat: self\n");
			abnf_print_self_rules(out_stream, rules, &info);
			break;
		default:
			;
	}
	abnf_destroy_rules(rules);
    if (out_file) {
		fclose(out_stream);
	}

//destroy:
	return 0;

err:
	abnf_destroy_rules(rules);
	fprintf(stderr, "Type '%s -h <command>' for help on a specific command.\n", basename(argv[0]));
	return 1;
err_2:
	abnf_destroy_rules(rules);
	return 2;
}
