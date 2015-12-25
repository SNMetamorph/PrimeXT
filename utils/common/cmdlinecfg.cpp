#include "cmdlib.h"
#include "scriplib.h"
#include "cmdlinecfg.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef SYSTEM_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef ZHLT_PARAMFILE
const char paramfilename[_MAX_PATH] = "settings.txt";
const char sepchr = '\n';
bool error = false;
#define SEPSTR "\n"

int plen (const char *p)
{
	int l;
	for (l = 0; ; l++)
	{
		if (p[l] == '\0')
			return -1;
		if (p[l] == sepchr)
			return l;
	}
}
bool pvalid (const char *p)
{
	return plen (p) >= 0;
}
bool pmatch (const char *cmdlineparam, const char *param)
{
	int cl, cstart, cend, pl, pstart, pend, k;
	cl = plen (cmdlineparam);
	pl = plen (param);
	if (cl < 0 || pl < 0)
		return false;
	bool anystart = (pl > 0 && param[0] == '*');
	bool anyend = (pl > 0 && param[pl-1] == '*');
	pstart = anystart ? 1 : 0;
	pend = anyend ? pl-1 : pl;
	if (pend < pstart) pend = pstart;
	for (cstart = 0; cstart <= cl; ++cstart)
	{
		for (cend = cl; cend >= cstart; --cend)
		{
			if (cend - cstart == pend - pstart)
			{
				for (k = 0; k < cend - cstart; ++k)
					if (tolower (cmdlineparam[k+cstart]) != tolower (param[k+pstart]))
						break;
				if (k == cend - cstart)
					return true;
			}
			if (!anyend)
				break;
		}
		if (!anystart)
			break;
	}
	return false;
}
char * pnext (char *p)
{
	return p + (plen (p) + 1);
}
char * findparams (char *cmdlineparams, char *params)
{
	char *c1, *c, *p;
	for (c1 = cmdlineparams; pvalid (c1); c1 = pnext (c1))
	{
		for (c = c1, p = params; pvalid (p); c = pnext (c), p = pnext (p))
			if (!pvalid (c) || !pmatch (c, p))
				break;
		if (!pvalid (p))
			return c1;
	}
	return NULL;
}
void addparams (char *cmdline, char *params, unsigned int n)
{
	if (strlen (cmdline) + strlen (params) + 1 <= n)
		strcat (cmdline, params);
	else
		error = true;
}
void delparams (char *cmdline, char *params)
{
	char *c, *p;
	if (!pvalid (params)) //avoid infinite loop
		return;
	while (cmdline = findparams (cmdline, params), cmdline != NULL)
	{
		for (c = cmdline, p = params; pvalid (p); c = pnext (c), p = pnext (p))
			;
		memmove (cmdline, c, strlen (c) + 1);
	}
}
typedef enum
{
	IFDEF, IFNDEF, ELSE, ENDIF, DEFINE, UNDEF
}
command_t;
typedef struct
{
	int stack;
	bool skip;
	int skipstack;
}
execute_t;
void parsecommand (execute_t &e, char *cmdline, char *words, unsigned int n)
{
	command_t t;
	if (!pvalid (words))
		return;
	if (pmatch (words, "#ifdef" SEPSTR))
		t = IFDEF;
	else if (pmatch (words, "#ifndef" SEPSTR))
		t = IFNDEF;
	else if (pmatch (words, "#else" SEPSTR))
		t = ELSE;
	else if (pmatch (words, "#endif" SEPSTR))
		t = ENDIF;
	else if (pmatch (words, "#define" SEPSTR))
		t = DEFINE;
	else if (pmatch (words, "#undef" SEPSTR))
		t = UNDEF;
	else
		return;
	if (t == IFDEF || t == IFNDEF)
	{
		e.stack ++;
		if (!e.skip)
		{
			if (t == IFDEF && findparams (cmdline, pnext (words)) ||
				t == IFNDEF && !findparams (cmdline, pnext (words)))
				e.skip = false;
			else
			{
				e.skipstack = e.stack;
				e.skip = true;
			}
		}
	}
	else if (t == ELSE)
	{
		if (e.skip)
		{
			if (e.stack == e.skipstack)
				e.skip = false;
		}
		else
		{
			e.skipstack = e.stack;
			e.skip = true;
		}
	}
	else if (t == ENDIF)
	{
		if (e.skip)
		{
			if (e.stack == e.skipstack)
				e.skip = false;
		}
		e.stack --;
	}
	else
	{
		if (!e.skip)
		{
			if (t == DEFINE)
				addparams (cmdline, pnext(words), n);
			if (t == UNDEF)
				delparams (cmdline, pnext(words));
		}
	}
}
const char * nextword (const char *s, char *token, unsigned int n)
{
	unsigned int i;
	const char *c;
	bool quote, comment, content;
	for (c=s, i=0, quote=false, comment=false, content=false; c[0] != '\0'; c++)
	{
		if (c[0]=='\"')
			quote = !quote;
		if (c[0]=='\n')
			quote = false;
		if (c[0]=='\n')
			comment = false;
		if (!quote && c[0]=='/' && c[1]=='/')
			comment = true;
		if (!comment && !(c[0]=='\n' || isspace (c[0])))
			content = true;
		if (!quote && !comment && content && (c[0]=='\n' || isspace (c[0])))
			break;
		if (content && c[0]!='\"')
			if (i<n-1)
				token[i++] = c[0];
			else
				error = true;
	}
	token[i] = '\0';
	return content? c: NULL;
}
void parsearg (int argc, char ** argv, char *cmdline, unsigned int n)
{
	int i;
	strcpy (cmdline, "");
	strcat (cmdline, "<");
	strcat (cmdline, g_Program);
	strcat (cmdline, ">");
	strcat (cmdline, SEPSTR);
	for (i=1; i<argc; ++i)
	{
		if (strlen (cmdline) + strlen (argv[i]) + strlen (SEPSTR) + 1 <= n)
		{
			strcat (cmdline, argv[i]);
			strcat (cmdline, SEPSTR);
		}
		else
			error = true;
	}
}
void unparsearg (int &argc, char **&argv, char *cmdline)
{
	char *c;
	int i, j;
	i = 0;
	for (c = cmdline; pvalid (c); c = pnext (c))
		i++;
	argc = i;
	argv = (char **)malloc (argc * sizeof (char *));
	if (!argv)
	{
		error = true;
		return;
	}
	for (c = cmdline, i = 0; pvalid (c); c = pnext (c), i++)
	{
		argv[i] = (char *)malloc (plen (c) + 1);
		if (!argv[i])
		{
			error = true;
			return;
		}
		for (j = 0; j < plen (c); j++)
			argv[i][j] = c[j];
		argv[i][j] = '\0';
	}
}
void ParseParamFile (const int argc, char ** const argv, int &argcnew, char **&argvnew)
{
	char token[MAXTOKEN], words[MAXTOKEN], cmdline[MAXTOKEN];
	FILE *f;
	char *s;
	const char *c, *c0;
	char filepath[_MAX_PATH];
	s = NULL;

	char tmp[_MAX_PATH];
#ifdef SYSTEM_WIN32
	GetModuleFileName (NULL, tmp, _MAX_PATH);
#else
	safe_strncpy (tmp, argv[0], _MAX_PATH);
#endif
	ExtractFilePath (tmp, filepath);
	strcat (filepath, paramfilename);
	f = fopen (filepath, "r");
	if (f)
	{
		int len = 0x100000;
		s = (char *)malloc (len+1);
		if (s)
		{
			int i, j;
			for (i=0; j=fgetc (f), i<len && j!=EOF; i++)
				s[i] = j;
			s[i] = '\0';
		}
		fclose (f);
	}
	if (s)
	{
		c = s;
		execute_t e;
		memset (&e, 0, sizeof (e));
		strcpy (words, "");
		strcpy (token, "");
		parsearg (argc, argv, cmdline, MAXTOKEN);
		while (1)
		{
			while (1)
			{
				c = nextword (c, token, MAXTOKEN);
				if (token[0] == '#' || !c)
					break;
			}
			if (!c)
				break;
			if (strlen (token) + strlen (SEPSTR) + 1 <= MAXTOKEN)
			{
				strcpy (words, token);
				strcat (words, SEPSTR);
			}
			else
			{
				error = true;
				break;
			}
			while (1)
			{
				c0 = c;
				c = nextword (c, token, MAXTOKEN);
				if (token[0] == '#' || !c)
				{
					c = c0;
					break;
				}
				if (strlen (words) + strlen (token) + strlen (SEPSTR) + 1 <= MAXTOKEN)
				{
					strcat (words, token);
					strcat (words, SEPSTR);
				}
				else
				{
					error = true;
					break;
				}
			}
			parsecommand (e, cmdline, words, MAXTOKEN);
		}
		unparsearg (argcnew, argvnew, cmdline);
		if (error)
		{
			argvnew = argv;
			argcnew = argc;
		}
		argvnew[0] = argv[0];
		free (s);
	}
	else
	{
		argvnew = argv;
		argcnew = argc;
	}
}
#endif

