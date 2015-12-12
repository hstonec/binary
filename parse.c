#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "arraylist.h"
#include "jstring.h"
#include "parse.h"

int parse_command(JSTRING *input_cmd, ARRAYLIST *cmd_list)
{
	//printf("%s\n", jstr_cstr(str_cmd));
	int i, j;
	int f_cmd = 1;
	int f_opt = 1;
	char c;
	int iscmd = 0;
	int isopt = 0;
	PARSED_CMD *p_cmd;
	ARRAYLIST *tmp_list = arrlist_create();
	JSTRING *tmp_cmd = jstr_create("");
	// separate cmd
	for (i = 0; i < jstr_length(input_cmd); i++)
	{
		c = jstr_charat(input_cmd, i);
		if (c=='|')
		{
			if (jstr_length(tmp_cmd) == 0)
				return SYNTAX_ERR;
			arrlist_add(cmd_list, tmp_cmd);
			tmp_cmd = jstr_create("");
		}
		jstr_append(tmp_cmd, c);
	}

	// deal with opt and redirection
	JSTRING *subcmd;
	JSTRING *p;
	JSTRING *tmpopt;
	JSTRING *tmpredir;
	int tmpredir_len = 0;
	for (i = 0; i < arrlist_size(tmp_list); i++)
	{
		iscmd = 1;
		isopt = 0;
		p_cmd = creat_pcmd();/* parsed_cmd */
		subcmd = (JSTRING*)arrlist_get(tmp_list, i);
		p = p_cmd->command;
		for (j = 0, j < jstr_length(subcmd); j++)
		{
			c = jstr_charat(subcmd, j);
			if (isspace(c)){
				if (jstr_length(p)==0)
					continue;
				if (iscmd){
					iscmd = 0;
					isopt = 1;
					p = jstr_create("");
				}
				if (isopt){
					arrlist_add(p_cmd->opt, p);
					p = jstr_create("");
				}
			}
			if (c == '<'||c=='>'){
				if (iscmd&&jstr_length(p) > 0){
					iscmd = 0;
					isopt = 1;
					p = jstr_create("");
					REDIRECT *subredir = creat_redirect();
					tmpredir_len = getfilename(tmpredir, subcmd, i, &subredir->type);
					if (tmpredir_len == 0)
						return SYNTAX_ERR;
					else
						i += tmpredir_len;
				}
			}
			jstr_append(p, c);
		}
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


int getfilename(JSTRING *tmpredir, JSTRING *subcmd, int cur_index, int *redir_type)
{
	int i = 0;
	tmpredir = jstr_create("");
	char c;
	for (i = cur_index; i < jstr_length(subcmd); i++)
	{
		if (c == '<')
			redir_type==
		c = jstr_charat(subcmd, i);
		if (isspace(c))
			break;
		jstr_append(tmpredir, c);
	}
	return jstr_length(tmpredir);
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
	jstr_free(p_cmd->command);
	jstr_free(p_cmd->opt);
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