#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "sh.h"

#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/dos.h>
#include <proto/exec.h>

#define FUNC //printf("<%s>\n", __PRETTY_FUNCTION__);fflush(stdout);
#define FUNCX //printf("</%s>\n", __PRETTY_FUNCTION__);fflush(stdout);

/*used to stdin/out fds*/
int amigain = -1, amigaout = -1;


char amigaos_getc(int fd)
{
        char c;
        
        return FGetC(fd);
}

int amigaos_write(int fd, void *b, int len)
{
        return Write(fd, b, len);
}

int amigaos_read(int fd, void *b, int len)
{
        return Read(fd, b, len);
}

int amigaos_ungetc(char c, int fd)
{
        return UnGetC(fd, c) ;
}

int amigaos_getstdfd(int fd)
{
        return (fd == -1 ? Open("CONSOLE:",MODE_OLDFILE) : fd);
}

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


int pipe(int filedes[2])
{
        char pipe_name[1024];
        FUNC;

        sprintf(pipe_name, "PIPE:%s\n", tmpnam(0));
//      printf("pipe: %s \n", pipe_name);
        
//      filedes[0] = open(pipe_name, O_WRONLY|O_CREAT);
//      filedes[1] = open(pipe_name, O_RDONLY); 

        filedes[1] = Open(pipe_name, MODE_NEWFILE);
        filedes[0] = Open(pipe_name, MODE_OLDFILE);


        if (filedes[0] == -1 || filedes[1] == -1)
        {
                if (filedes[0] != -1)
                        Close(filedes[0]);
                if (filedes[1] != -1)
                        Close(filedes[1]);

                FUNCX;
                return -1;
        }
//      printf("filedes %d %d\n", filedes[0], filedes[1]);fflush(stdout);

        FUNCX;
        return 0;
}

/*Jack: this one is wrong:
 * int openstdinout()
{
        char pipe_name[1024];
        FUNC;
        int mystdin, mystdout;
	
        mystdin = Open("CONSOLE:", MODE_OLDFILE);
        mystdout = Open("CONSOLE:", MODE_OLDFILE);
	
        ksh_dup2(mystdin, 0, FALSE);
        ksh_dup2(mystdout, 1, FALSE);
        FUNCX;
        return 0;
}
*/

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
        if (strcmp(filename, "/dev/null") == 0)
        {
            out = newname =  strdup( "NIL:");;
            FUNCX;
            return out;
        }
	
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
                while ((*filename != '/') && *filename )
                    *out++ = *filename++;
                        
                *out++ = ':';
                if (*filename)
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
        size = 0;//strlen(filename) + 1;
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
                        if (interpreter[strlen(interpreter)-1] == 10)
                                interpreter[strlen(interpreter)-1] = 0;
                        size += strlen(interpreter) + 1;
                }
        
                fclose(fh);
        }
        
        /* Allocate the command line */
        filename_conv = convert_path(filename);
        size += strlen(filename_conv) + 1;

        full = malloc(size+1);
        if (full)
        {
                if (interpreter)
                {
                        interpreter_conv = convert_path(interpreter);
                        
                        sprintf(full, "%s %s ", interpreter_conv, filename_conv);
                        
                        free(interpreter);
                        free(filename_conv);
                        free(interpreter_conv);
                }
                else
                {
                        sprintf(full, "%s ", filename_conv);
                        
                        free(filename_conv);
                }
                        
                for (cur = (char**)argv+1; *cur != 0; cur++)
                {
                        strcat(full, *cur);
                        strcat(full, " ");
                }
                SystemTags(full, TAG_DONE);

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

struct userdata
{
        struct op *t;
        int flags;
        struct Task *parent;
};

LONG execute_child(STRPTR args, int len)
{
        struct op *t;
        int flags;
        struct Task *parent;
        struct Task *this;

        this = FindTask(0);
        t = ((struct userdata *)this->tc_UserData)->t;
        flags = ((struct userdata*)this->tc_UserData)->flags;
        parent = ((struct userdata*)this->tc_UserData)->parent;

//      sscanf(args, "%08lx %08lx %08lx", &t, &flags, &parent);

        execute(t, flags & (XEXEC | XERROK));

        Signal(parent, SIGBREAKF_CTRL_F);
        return 0;
}

int
exchild(t, flags, close_fd)
        struct op       *t;
        int             flags;
        int             close_fd;       /* used if XPCLOSE or XCCLOSE */
{
/*pipe flags (flags & XPIPEI/O)*/
/*      int pipeo = 0;  
        int pipei = 0;*/
/*close conditions*/
        int close_fd_i = 1;
        int close_fd_o = 1;
/*current input output*/
        int inputfd = -1, outputfd = -1;

        char args[30];
        struct Process *proc;
        struct Task *thisTask = FindTask(0);
        struct userdata taskdata;

        char *name;

        FUNC;
#if 0   
//      printf("close_fd = %d", close_fd);fflush(stdout);
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
        
//      sprintf(args, "%08lx %08lx %08lx", t, flags & (XEXEC | XERROK), thisTask);
        taskdata.t      = t;
        taskdata.flags  = flags & (XEXEC | XERROK);
        taskdata.parent = thisTask;

/*      pipeo = ((flags & XPIPEO));// && !(flags & XXCOM));
        pipei = (flags & XPIPEI);*/
        close_fd_i = TRUE;
        close_fd_o = TRUE;
//      printf("TYPE %d TCOM %d \n", t->type, TCOM);fflush(stdout);

        if (t->type == 21)
        {
                if (amigain != -1)
                {
                        close_fd_i = FALSE;
                        inputfd = amigain;
                }
                else
                        inputfd = Open("CONSOLE:",MODE_OLDFILE);
                        
                if (amigaout != -1)
                        outputfd = amigaout;
                else
                        outputfd = Open("CONSOLE:",MODE_OLDFILE);
                        
        }
        else
        {
                        outputfd = Open("CONSOLE:",MODE_OLDFILE);
                        inputfd = Open("CONSOLE:",MODE_OLDFILE);
        }

#if 1
        name = strdup(t->str);
#else
        name = convert_path(t->str);
#endif
        proc = CreateNewProcTags(
                        NP_Entry,               execute_child,
/*                      NP_Child,               TRUE, */
                        NP_Input,               inputfd,
                        NP_CloseInput,          close_fd_i,
                        NP_Output,              outputfd,
                        NP_CloseOutput,         close_fd_o,
//                      NP_Error,               ErrorOutput(),
//                      NP_CloseError,          FALSE,
//                      NP_Arguments,           args,
                        NP_Name,                name,
#ifdef __amigaos4__
                        NP_UserData,            (int)&taskdata,
#endif
                        TAG_DONE);
        free(name);

#ifndef __amigaos4__
        proc->pr_Task.tc_UserData = &taskdata;
#endif
                        
        Wait(SIGBREAKF_CTRL_F);          
/*close pipe input*/

        if (!close_fd_i)
                Close(inputfd);

/*restore to stdin/out*/
        amigain = amigaout = -1;

        FUNCX;          
        return 0;
}

