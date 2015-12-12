#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include "arraylist.h"
#include "jstring.h"
#include "parse.h"

PARSED_CMD *creat_pcmd(void);
void free_pcmd(PARSED_CMD *p_cmd);
REDIRECT *creat_redirect(void);
void free_redir(REDIRECT *redir);
int getfilename(REDIRECT *subredir, JSTRING *subcmd, int cur_index);
int sep_cmd(JSTRING *input_cmd, ARRAYLIST *bg_list, ARRAYLIST *tmp_list);

int parse_command(JSTRING *input_cmd, ARRAYLIST *cmd_list)
{
	//printf("%s\n", jstr_cstr(str_cmd));
	int i, j;
	char c;
	int iscmd = 0;
	int isopt = 0;
	int ret;
	// get subcmd
	ARRAYLIST *tmp_list = arrlist_create();
	ARRAYLIST *bg_list = arrlist_create();
	ret = sep_cmd(input_cmd, bg_list, tmp_list);
	if (ret)
		return ret;
	// deal with opt and redirection
	JSTRING *subcmd;
	JSTRING *p; /* pointer for operating */
	PARSED_CMD *p_cmd;
	REDIRECT *subredir;
	// for printing 
	//PARSED_CMD *p_show;
	//REDIRECT *r_show;

	int tmpredir_len = 0;
	for (i = 0; i < arrlist_size(tmp_list); i++)
	{
		iscmd = 1;
		isopt = 0;
		p_cmd = creat_pcmd();
		subcmd = (JSTRING* )arrlist_get(tmp_list, i);
		p = p_cmd->command;
		p_cmd->background = *(int *)arrlist_get(bg_list, i);

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
					tmpredir_len = getfilename(subredir, subcmd, i);
					if (tmpredir_len == 0)
						return SYNTAX_ERR;
					else{
						arrlist_add(p_cmd->redirect_list, subredir);
						i += tmpredir_len;
					}
				}
				if (isopt){
					arrlist_add(p_cmd->opt, p);
					p = jstr_create("");
					subredir = creat_redirect();
					tmpredir_len = getfilename(subredir, subcmd, i);
					if (tmpredir_len == 0)
						return SYNTAX_ERR;
					else{
						arrlist_add(p_cmd->redirect_list, subredir);
						i += tmpredir_len;
					}
				}
				continue;
			}
			jstr_append(p, c);
		}
		arrlist_add(cmd_list, p_cmd);
	}
	/*
	for (i = 0; i < arrlist_size(cmd_list); i++)
	{
		p_show = (PARSED_CMD *)arrlist_get(cmd_list, i);
		printf("cmd %s\n", jstr_cstr(p_show->command));
		for (j = 0; j < arrlist_size(p_show->opt); j++)
			printf("opt%d: %s\n", j, jstr_cstr((JSTRING *)arrlist_get(p_show->opt, j)));

		for (j = 0; j < arrlist_size(p_show->redirect_list); j++){
			r_show = (REDIRECT *)arrlist_get(p_show->redirect_list, j);
			printf("redir%d: %s, %d\n", j, jstr_cstr(r_show->filename), r_show->type);
		}
		if (p_show->background)
			printf("in background\n");
	}
	*/
	// free tmp_list
	for (i = 0; i < arrlist_size(tmp_list); i++)
	{
		jstr_free((JSTRING*)arrlist_get(tmp_list, i));
	}
	arrlist_free(tmp_list);
	// free bg_list
	int *freebg;
	for (i = 0; i < arrlist_size(bg_list); i++)
	{
		freebg=(int*)arrlist_get(bg_list, i);
		free(freebg);
		freebg = NULL;
	}
	arrlist_free(bg_list);
	return 0;
}

int sep_cmd(JSTRING *input_cmd, ARRAYLIST *bg_list, ARRAYLIST *tmp_list)
{
	int i;
	char c;
	int *bg;

	JSTRING *tmp_cmd = jstr_create("");
	for (i = 0; i < jstr_length(input_cmd); i++)
	{
		c = jstr_charat(input_cmd, i);
		if (c == '|'){
			if (jstr_length(tmp_cmd) == 0)
				return SYNTAX_ERR;
			else{
				arrlist_add(tmp_list, tmp_cmd);
				bg = (int *)malloc(sizeof(int));
				*bg = 0;
				arrlist_add(bg_list, bg);
			}
			tmp_cmd = jstr_create("");
			continue;
		}
		if (c == '&'){
			if (jstr_length(tmp_cmd) == 0)
				return SYNTAX_ERR;
			else{
				arrlist_add(tmp_list, tmp_cmd);
				bg = (int *)malloc(sizeof(int));
				*bg = 1;
				arrlist_add(bg_list, bg);
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
		bg = (int *)malloc(sizeof(int));
		*bg = 0;
		arrlist_add(bg_list, bg);
	}
	return 0;
}


int getfilename(REDIRECT *subredir, JSTRING *subcmd, int cur_index)
{
	int i = 0;
	int ifredir = 0;
	subredir->filename = jstr_create("");
	char c;
	for (i = cur_index; i < jstr_length(subcmd); i++)
	{
		c = jstr_charat(subcmd, i);
		if (c == '<'){
			if (jstr_length(subredir->filename) == 0){
				if (ifredir)
					return SYNTAX_ERR;
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
					return SYNTAX_ERR;
				else{
					ifredir = 1;
					if (jstr_charat(subcmd, i + 1) == '>'){
						subredir->type = REDIR_STDAPPEND;
						i++;
					}
					else
						subredir->type = REDIR_STDOUT;
					continue;
				}
			}
		}
		if (isspace(c)){
			if (jstr_length(subredir->filename) == 0)
				continue;
			else
				return jstr_length(subredir->filename);
		}
		jstr_append(subredir->filename, c);
	}
	if (jstr_length(subredir->filename) == 0)
		return SYNTAX_ERR;
	else
		return jstr_length(subredir->filename);
}

PARSED_CMD *creat_pcmd(void)
{
	PARSED_CMD *p_cmd;
	p_cmd = (PARSED_CMD *)malloc(sizeof(PARSED_CMD));
	p_cmd->background = 0;
	p_cmd->command = jstr_create("");
	p_cmd->opt = arrlist_create();
	p_cmd->redirect_list = arrlist_create();
	return p_cmd;
}
void free_pcmd(PARSED_CMD *p_cmd)
{
	int i;
	REDIRECT* p;
	jstr_free(p_cmd->command);
	for (i = 0; i < arrlist_size(p_cmd->opt); i++){
		jstr_free((JSTRING*)arrlist_get(p_cmd->opt, i));
	}
	for (i = 0; i < arrlist_size(p_cmd->redirect_list); i++){
		p = (REDIRECT *)arrlist_get(p_cmd->redirect_list, i);
		jstr_free(p->filename);
	}
	arrlist_free(p_cmd->opt);
	arrlist_free(p_cmd->redirect_list);
	free(p_cmd);
	p_cmd = NULL;
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