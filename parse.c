#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <bsd/stdlib.h>

#include "macros.h"
#include "arraylist.h"
#include "jstring.h"
#include "parse.h"

PARSED_CMD *creat_pcmd(void);
REDIRECT *creat_redirect(void);
void free_redir(REDIRECT *redir);
int getfilename(REDIRECT *subredir, JSTRING *subcmd, int *cur_index);
int sep_cmd(JSTRING *input_cmd, BOOL *ifbg, ARRAYLIST *tmp_list);

int parse_command(JSTRING *input_cmd, ARRAYLIST *cmd_list, BOOL *ifbg)
{
	//printf("%s\n", jstr_cstr(str_cmd));
	int i, j;
	char c;
	int iscmd = 0;
	int isopt = 0;
	int ret;
	// get subcmd
	ARRAYLIST *tmp_list = arrlist_create();
	
	ret = sep_cmd(input_cmd, ifbg, tmp_list);
	if (ret)
		return ret;
	// deal with opt and redirection
	JSTRING *subcmd;
	JSTRING *p; /* pointer for operating */
	PARSED_CMD *p_cmd;
	REDIRECT *subredir;
	// for printing 

	int tmpredir_len = 0;
	for (i = 0; i < arrlist_size(tmp_list); i++)
	{
		iscmd = 1;
		isopt = 0;
		p_cmd = creat_pcmd();
		subcmd = (JSTRING* )arrlist_get(tmp_list, i);
		p = p_cmd->command;

		for (j = 0; j < jstr_length(subcmd); j++)
		{
			c = jstr_charat(subcmd, j);
			if (isspace(c)){
				if (jstr_length(p)==0)
					continue;
				if (iscmd){
					iscmd = 0;
					isopt = 1;
					arrlist_add(p_cmd->opt, p);
					p = jstr_create("");
					continue;
				}
				if (isopt){
					arrlist_add(p_cmd->opt, p);
					p = jstr_create("");
					continue;
				}
			}
			if (c == '<' || c == '>'){
				if (iscmd&&jstr_length(p) > 0){
					iscmd = 0;
					isopt = 1;
					arrlist_add(p_cmd->opt, p);
					p = jstr_create("");
					subredir = creat_redirect();
					tmpredir_len = getfilename(subredir, subcmd, &j);
					if (tmpredir_len == 0){
						fprintf(stderr, "-%s: syntax error near unexpected token '%c'\n", getprogname(), c);
						return SYNTAX_ERR;
					}
					else{
						arrlist_add(p_cmd->redirect_list, subredir);
					}
					continue;
				}
				if (isopt){
					if (jstr_length(p) > 0){
						arrlist_add(p_cmd->opt, p);
						p = jstr_create("");
					}
					subredir = creat_redirect();
					tmpredir_len = getfilename(subredir, subcmd, &j);
					if (tmpredir_len == 0){
						fprintf(stderr, "-%s: syntax error near unexpected token '%c'\n", getprogname(), c);
						return SYNTAX_ERR;
					}
					else{
						arrlist_add(p_cmd->redirect_list, subredir);
					}
				}
				continue;
			}
			jstr_append(p, c);
		}
		if (jstr_length(p) > 0)
			arrlist_add(p_cmd->opt, p);
		arrlist_add(cmd_list, p_cmd);
	}
	// free tmp_list
	for (i = 0; i < arrlist_size(tmp_list); i++)
	{
		jstr_free((JSTRING*)arrlist_get(tmp_list, i));
	}
	arrlist_free(tmp_list);	
	return 0;
}

int sep_cmd(JSTRING *input_cmd, BOOL *ifbg, ARRAYLIST *tmp_list)
{
	int i;
	char c;
	int bg = 0;
	int ifpipe = 0;
	int bgflag = 0;
	JSTRING *tmp_cmd = jstr_create("");
	for (i = 0; i < jstr_length(input_cmd); i++)
	{
		c = jstr_charat(input_cmd, i);
		if (c == '|'){
			if (jstr_length(tmp_cmd) == 0){
				fprintf(stderr, "-%s: syntax error near unexpected token '%c'\n", getprogname(), c);
				return SYNTAX_ERR;
			}
			else{
				ifpipe = 1;
				bgflag = 0;
				arrlist_add(tmp_list, tmp_cmd);
			}
			tmp_cmd = jstr_create("");
			continue;
		}
		if (c == '&'){
			if (jstr_length(tmp_cmd) == 0){
				fprintf(stderr, "-%s: syntax error near unexpected token '%c'\n", getprogname(), c);
				return SYNTAX_ERR;
			}
			else{
				bgflag = 1;
				ifpipe = 0;
				arrlist_add(tmp_list, tmp_cmd);
				if (!bg)
					bg = 1;
				else
					return SYNTAX_ERR;
			}
			tmp_cmd = jstr_create("");
			continue;
		}
		if (isspace(c) && jstr_length(tmp_cmd)==0)
			continue;
		jstr_append(tmp_cmd, c);
	}
	if (jstr_length(tmp_cmd) > 0){
		arrlist_add(tmp_list, tmp_cmd);
	}
	else if (ifpipe&&!bgflag){
		fprintf(stderr, "-%s: syntax error near unexpected token '|'\n", getprogname());
		return SYNTAX_ERR;
	}
	*ifbg = bg;
	return 0;
}


int getfilename(REDIRECT *subredir, JSTRING *subcmd, int *cur_index)
{
	int i = 0;
	int ifredir = 0;
	int count_space = 0;
	subredir->filename = jstr_create("");
	char c;
	for (i = *cur_index; i < jstr_length(subcmd); i++)
	{
		c = jstr_charat(subcmd, i);
		if (c == '<'){
			if (jstr_length(subredir->filename) == 0){
				if (ifredir)
					return 0;
				else{
					ifredir = 1;
					subredir->type = REDIR_STDIN;
					continue;
				}
			}
			else{
				return jstr_length(subredir->filename);
			}
		}
		if (c == '>'){
			if (jstr_length(subredir->filename) == 0){
				if (ifredir)
					return 0;
				else{
					ifredir = 1;
					if (i + 1 < jstr_length(subcmd)){
						if (jstr_charat(subcmd, i + 1) == '>'){
							subredir->type = REDIR_STDAPPEND;
							i++;
						}
						else
							subredir->type = REDIR_STDOUT;
					}
					else
						return 0;
					continue;
				}
			}
		}
		if (isspace(c)){
			if (jstr_length(subredir->filename) == 0){
				count_space++;
				continue;
			}
			else{
				if (subredir->type == REDIR_STDAPPEND)
					*cur_index = *cur_index + jstr_length(subredir->filename) + count_space + 1;
				else
					*cur_index = *cur_index + jstr_length(subredir->filename) + count_space;
				return jstr_length(subredir->filename);
			}
		}
		jstr_append(subredir->filename, c);
	}
	if (subredir->type == REDIR_STDAPPEND)
		*cur_index = *cur_index + jstr_length(subredir->filename) + count_space + 1;
	else
		*cur_index = *cur_index + jstr_length(subredir->filename) + count_space;
	if (jstr_length(subredir->filename) == 0)
		return 0;
	else
		return jstr_length(subredir->filename);
}

PARSED_CMD *creat_pcmd(void)
{
	PARSED_CMD *p_cmd;
	p_cmd = (PARSED_CMD *)malloc(sizeof(PARSED_CMD));
	p_cmd->command = jstr_create("");
	p_cmd->opt = arrlist_create();
	p_cmd->redirect_list = arrlist_create();
	return p_cmd;
}
void free_pcmd(ARRAYLIST *cmd_list)
{
	int i, j, l;
	REDIRECT* p;
	PARSED_CMD *p_cmd;
	for (j = 0; j < arrlist_size(cmd_list); j++){
		p_cmd = (PARSED_CMD *)arrlist_get(cmd_list, j);
		jstr_free(p_cmd->command);
		for (i = 1; i < arrlist_size(p_cmd->opt); i++){
			jstr_free((JSTRING*)arrlist_get(p_cmd->opt, i));
		}
		for (i = 0; i < arrlist_size(p_cmd->redirect_list); i++){
			p = (REDIRECT *)arrlist_get(p_cmd->redirect_list, i);
			free_redir(p);
		}
		arrlist_free(p_cmd->opt);
		arrlist_free(p_cmd->redirect_list);
		free(p_cmd);
		p_cmd = NULL;
	}
	l = arrlist_size(cmd_list);
	for (i = 0; i < l; i++)
	{
		arrlist_remove(cmd_list, 0);
	}
}

REDIRECT *creat_redirect(void)
{
	REDIRECT *redir;
	redir = (REDIRECT *)malloc(sizeof(REDIRECT));
	redir->filename = jstr_create("");
	redir->type = -1;
	return redir;
}
void free_redir(REDIRECT *redir)
{
	jstr_free(redir->filename);
	free(redir);
	redir = NULL;
}