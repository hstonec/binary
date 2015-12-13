#ifndef _PAESE_H
#define _PAESE_H

#define SYNTAX_ERR 127

#define REDIR_STDIN 0
#define REDIR_STDOUT 1
#define REDIR_STDAPPEND 2

typedef struct redirect{
	int type;	// 0:stdin_redirect, 1:stdout_redirect, 2:append_stdout
	JSTRING *filename;
}REDIRECT;

typedef struct parsed_cmd{
	JSTRING *command;		/* cmd */
	ARRAYLIST *opt;			/* options of cmd (jstring)*/
	ARRAYLIST *redirect_list; /* redirection info in this cmd (redirect)*/
} PARSED_CMD;

int parse_command(JSTRING *input_cmd, ARRAYLIST *cmd_list, BOOL *ifbg);
void free_pcmd(ARRAYLIST *cmd_list);
#endif