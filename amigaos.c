#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "sh.h"

#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dos.h>

#define FUNC //printf("<%s>\n", __PRETTY_FUNCTION__);fflush(stdout);
#define FUNCX //printf("</%s>\n", __PRETTY_FUNCTION__);fflush(stdout);

/*store newly opened pipe fds*/
int pipefd[2];
/*used to reserve the pipe fds*/
int nextin = -1, nextout = -1;
/*pipecount*/
int pipe_flag = 0;
int xxcom_pipe_flag = 0;
extern int xxcom_nextout;
int amigaos_isabspath(const char *path)
{FUNC;
	if (*path == '/')
	{
		/* Special treatment: absolute path for Unix compatibility */
		FUNCX;
		return 1;
	}
		
	while (*path)
	{
		if (*path == ':')
		{
			FUNCX;
			return 1;
		}
		if (*path == '/')
		{
			FUNCX;
			return 0;
		}
		path++;	
	}
	
	FUNCX;
	return 0;
}

int amigaos_isrootedpath(const char *path)
{
	return amigaos_isabspath(path);
}

int amigaos_isrelpath(const char *path)
{
	return !amigaos_isabspath(path);
}

int amigaos_dupbbase(int fd, int base)
{
	int res;
	FUNC;
	res = fcntl(fd, F_DUPFD, base);
	FUNCX;
	
	return res;
}

int getppid(void)
{
	return 0;
}


unsigned int alarm(unsigned int seconds)
{
	fprintf(stderr,"alarm() not implemented\n");
	return 1;
}


void
closepipe(pv)
	register int *pv;
{
	FUNC;
	IDOS->Close(pv[0]);
	IDOS->Close(pv[1]);
//	pv[0] = pv[1] = -1;
	FUNCX;
}
int pipe(int filedes[2])
{
	char pipe_name[1024];
	FUNC;

	sprintf(pipe_name, "PIPE:%s\n", tmpnam(0));
//	printf("pipe: %s \n", pipe_name);
	
//	filedes[0] = open(pipe_name, O_WRONLY|O_CREAT);
//	filedes[1] = open(pipe_name, O_RDONLY);	

	filedes[1] = IDOS->Open(pipe_name, MODE_NEWFILE);
	filedes[0] = IDOS->Open(pipe_name, MODE_OLDFILE);


	if (filedes[0] == -1 || filedes[1] == -1)
	{
		if (filedes[0] != -1)
			IDOS->Close(filedes[0]);
		if (filedes[1] != -1)
			IDOS->Close(filedes[0]);

		FUNCX;
		return -1;
	}
//	printf("filedes %d %d\n", filedes[0], filedes[1]);fflush(stdout);

	FUNCX;
	return 0;
}

int openstdinout()
{
	char pipe_name[1024];
	FUNC;
	int mystdin, mystdout;

	mystdin = IDOS->Open("CONSOLE:", MODE_OLDFILE);
	mystdout = IDOS->Open("CONSOLE:", MODE_OLDFILE);

	ksh_dup2(mystdin, 0, FALSE);
	ksh_dup2(mystdout, 1, FALSE);
	FUNCX;
	return 0;
}


int fork(void)
{
	fprintf(stderr,"Can not bloody fork\n");
	errno = ENOMEM;
	return -1;
}

int wait(int *status)
{
	fprintf(stderr,"No wait\n");
	errno = ECHILD;
	return -1;
}

char *convert_path(const char *filename)
{
	int abs;
	char *newname;
	char *out;
	
	FUNC;
	
	abs = filename[0] == '/';

	if (!filename)
	{
		FUNCX;
		return 0;
	}
	
	newname = malloc(strlen(filename) + 1);
	out = newname;
	if (!out)
	{
		FUNCX;
		return 0;
	}
	
	if (abs)
	{
		filename++;
		while (*filename != '/')
			*out++ = *filename++;
			
		*out++ = ':';
		filename++;
	}
		
	while (*filename)
		*out++ = *filename++;
	
	*out = 0;
	
	FUNCX;
	return newname;
}
		
	
int execve(const char *filename, char *const argv[], char *const envp[])
{
	FILE *fh;
	char buffer[1000];
	int size = 0;
	char **cur;
	char *interpreter = 0;
	char *full = 0;
	char *filename_conv;
	char *interpreter_conv;
	uint32 error;
	
	FUNC;
	
	/* Calculate the size of filename and all args, including spaces and quotes */
	size = strlen(filename) + 1;
	for (cur = (char **)argv+1; *cur; cur++)
		size += strlen(*cur) + 1;
	
	
	/* Check if it's a script file */
	fh = fopen(filename, "r");
	if (fh)
	{
		if (fgetc(fh) == '#' && fgetc(fh) == '!')
		{
			fgets(buffer, 999, fh);
			interpreter = strdup(buffer);
			size += strlen(interpreter) + 1;
		}
	
		fclose(fh);
	}
	
	/* Allocate the command line */
	full = malloc(size+1);
	if (full)
	{
		if (interpreter)
		{
			interpreter_conv = convert_path(interpreter);
			filename_conv = convert_path(filename);
			
			sprintf(full, "%s %s ", interpreter_conv, filename_conv);
			
			free(interpreter);
			free(filename_conv);
			free(interpreter_conv);
		}
		else
		{
			filename_conv = convert_path(filename);
			
			sprintf(full, "%s ", filename_conv);
			
			free(filename_conv);
		}
			
		for (cur = (char**)argv+1; *cur != 0; cur++)
		{
			strcat(full, *cur);
			strcat(full, " ");
		}
		
		IDOS->SystemTags(full, TAG_DONE);

		free(full);
		FUNCX;
		return 0;
	}
	
	errno = ENOMEM;
	FUNCX;
	return -1;
}

int pause(void)
{
	fprintf(stderr,"Pause not implemented\n");
	
	errno = EINTR;
	return -1;
}


LONG execute_child(STRPTR args, int len)
{
	struct op *t;
	int flags;
	struct Task *parent;
		
	sscanf(args, "%08lx %08lx %08lx", &t, &flags, &parent);
	execute(t, flags & (XEXEC | XERROK));

	IExec->Signal(parent, SIGBREAKF_CTRL_F);
	return 0;
}

int
exchild(t, flags, close_fd)
	struct op	*t;
	int		flags;
	int		close_fd;	/* used if XPCLOSE or XCCLOSE */

{
/*pipe flags (flags & XPIPEI/O)*/
	int pipeo = 0;	
	int pipei = 0;
/*close conditions*/
	int close_fd_i = 1;
	int close_fd_o = 1;
/*current input output*/
	int inputfd = -1, outputfd = -1;

	char args[30];
	struct Task *thisTask = IExec->FindTask(0);
	FUNC;
#if 1	
//	printf("close_fd = %d", close_fd);fflush(stdout);
	printf("flags = ");
	if (flags & XEXEC) printf("XEXEC ");
	if (flags & XFORK) printf("XFORK ");
	if (flags & XBGND) printf("XBGND ");
	if (flags & XPIPEI) printf("XPIPEI ");
	if (flags & XPIPEO) printf("XPIPEO ");
	if (flags & XXCOM) printf("XXCOM ");
	if (flags & XPCLOSE) printf("XPCLOSE ");
	if (flags & XCCLOSE) printf("XCCLOSE ");
	if (flags & XERROK) printf("XERROK ");
	if (flags & XCOPROC) printf("XCOPROC ");
	if (flags & XTIME) printf("XTIME ");
	printf("\n"); fflush(stdout);
#endif
	
	sprintf(args, "%08lx %08lx %08lx", t, flags & (XEXEC | XERROK), thisTask);
/*XCCOM is here when ` ` expansion used, needs further investigation*/
	pipeo = ((flags & XPIPEO));// && !(flags & XXCOM));
	pipei = (flags & XPIPEI);
	close_fd_i = TRUE;//((!(flags & XPCLOSE) && pipei) || !pipei);
	close_fd_o = TRUE;//(((flags & XCCLOSE) && pipeo) || !pipeo);
	printf("TYPE %d TCOM %d \n", t->type, TCOM);fflush(stdout);
#if 1

/*buffer the pipefd[1] as next input*/
	if (pipei)
		nextin = pipefd[0];
/*detect the new pipe, raise the count and buffer the input*/
	if (pipeo)
	{
/*XXCOM is when comsub() is used, only open the pipe, comsub'll collect it */
		if (!(flags & XXCOM))
		{
			pipe(pipefd);
			nextout = pipefd[1];
			pipe_flag++;
		}
		else
			xxcom_pipe_flag++;
	}

/*type==21 occurs in execute() after XPIPEI/O here*/
	if ((t->type == 21) && pipe_flag && !(xxcom_pipe_flag > 0))
	{
/*there's nextout pendindg*/
		if (nextout != -1)
		{
/*nextin pendidng*/
			if(nextin != -1)
			{
/*use it*/
				inputfd = nextin;
/*remove pending*/
				nextin = -1;
/*we will close it later*/
				close_fd_i = FALSE;
/* one pipe is done -- reduce count*/
				pipe_flag--;
			}
			else
				inputfd = IDOS->Open("CONSOLE:",MODE_OLDFILE);

			outputfd = nextout;
			nextout = -1;
		}
		else
		{
/*same stuff without nextout pending*/
			if(nextin != -1)
			{
				inputfd = nextin;
				nextin = -1;
				close_fd_i = FALSE;
				pipe_flag--;
			}
			else
				inputfd = IDOS->Open("CONSOLE:",MODE_OLDFILE);

			outputfd = IDOS->Open("CONSOLE:",MODE_OLDFILE);
		}
	}
/*defaults*/
	else if ((t->type == 21) && (xxcom_pipe_flag > 0))
	{
		outputfd = xxcom_nextout;
		inputfd = IDOS->Open("CONSOLE:",MODE_OLDFILE);
		xxcom_pipe_flag--;
	}
	else
	{
		outputfd =IDOS->Open("CONSOLE:",MODE_OLDFILE);
		inputfd = IDOS->Open("CONSOLE:",MODE_OLDFILE);
	}

//	printf("PIPE_FLAG %d\n", pipe_flag);fflush(stdout);
//	outputfd = (pipeo  ? pipefd[0] : IDOS->Open("CONSOLE:",MODE_OLDFILE));
//	printf ("in %d out %d\n",inputfd, outputfd);fflush(stdout);
#else
	outputfd =IDOS->Open("CONSOLE:",MODE_OLDFILE);
	inputfd = IDOS->Open("CONSOLE:",MODE_OLDFILE);
#endif

	IDOS->CreateNewProcTags(
			NP_Entry, 		execute_child,
/*			NP_Child,		TRUE, */
			NP_Input,               inputfd,
			NP_CloseInput,		close_fd_i,
			NP_Output,		outputfd,
			NP_CloseOutput,		close_fd_o,
//			NP_Error,		IDOS->ErrorOutput(),
//			NP_CloseError,		FALSE,
			NP_Arguments,		args,
			TAG_DONE);

			
	IExec->Wait(SIGBREAKF_CTRL_F);		
/*close pipe input*/
	if (!close_fd_i)
	{
		IDOS->Close(inputfd);
	}
	FUNCX;		
	return 0;
}
 
	
	
