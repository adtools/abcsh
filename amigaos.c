#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "sh.h"

#include <dos/dos.h>
#ifndef CLIBHACK
#include <dos/dostags.h>
#endif
#include <proto/dos.h>
#include <proto/exec.h>

#define FUNC //printf("<%s>\n", __PRETTY_FUNCTION__);fflush(stdout);
#define FUNCX //printf("</%s>\n", __PRETTY_FUNCTION__);fflush(stdout);

/*used to stdin/out fds*/
#ifndef CLIBHACK
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
#endif /*CLIBHACK*/
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
/*      printf("pipe: %s \n", pipe_name);*/

#ifdef CLIBHACK
        filedes[1] = open(pipe_name, O_WRONLY|O_CREAT);
        filedes[0] = open(pipe_name, O_RDONLY);
#else
        filedes[1] = Open(pipe_name, MODE_NEWFILE);
        filedes[0] = Open(pipe_name, MODE_OLDFILE);
#endif/*CLIBHACK*/

    if (filedes[0] == -1 || filedes[1] == -1)
        {
#ifdef CLIBHACK
                if (filedes[0] != -1)
                    close(filedes[0]);
                if (filedes[1] != -1)
                    close(filedes[1]);
#else
                if (filedes[0] != -1)
                    Close(filedes[0]);
                if (filedes[1] != -1)
                    Close(filedes[1]);
#endif/*CLIBHACK*/

                FUNCX;
                return -1;
        }
/*      printf("filedes %d %d\n", filedes[0], filedes[1]);fflush(stdout);*/

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

        if (!filename)
        {
            FUNCX;
            return 0;
        }

        if (strcmp(filename, "/dev/null") == 0)
        {
            out = newname =  strdup( "NIL:");;
            FUNCX;
            return out;
        }

        if (strcmp(filename, "/dev/tty") == 0)
        {
            out = newname =  strdup( "CON:");;
            FUNCX;
            return out;
        }


        abs = filename[0] == '/';

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

static createvars(char **envp)
{
    while(*envp != NULL)
    {
        int len;
        char *var;
        char *val;
        /* Set a loal var to indicate to any subsequent sh that it is not */
        /* The top level shell and so should only inherit local amigaos vars */

        SetVar("ABCSH_IMPORT_LOCAL","TRUE",5,GVF_LOCAL_ONLY);

        if(len = strlen(*envp)){
            if(var = (char *)malloc(len+1))
            {
                strcpy(var,*envp);

                val = strchr(var,'=');
                if(val)
                {
                    *val++='\0';
                    if (*val)
                    {
                        SetVar(var,val,strlen(val)+1,GVF_LOCAL_ONLY);
                    }
                }
                free(var);
            }
        }
        envp++;
    }
}

int execve(const char *filename, char *const argv[], char *const envp[])
{
        FILE *fh;
        char buffer[1000];
        int size = 0;
        char **cur;
        char *interpreter = 0;
        char *full = 0;
        char *filename_conv = 0;
        char *interpreter_conv = 0;
        char *tmp = 0;
        int tmpint;
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
                        if(interpreter)
                        {
                            if (interpreter[strlen(interpreter)-1] == 10)
                            {
                                tmp = interpreter;
                                tmpint = strlen(interpreter)-1;
                                tmp[tmpint] = 0;
                                interpreter = strdup(tmp);
                                tmp[tmpint] = 10;
                                free(tmp);
                            }
                            size += strlen(interpreter) + 1;
                        }
                }

                fclose(fh);
        }

        /* Allocate the command line */
        filename_conv = convert_path(filename);
        if(filename_conv)
            size += strlen(filename_conv);
        size += 1;
        full = malloc(size+1);
        if (full)
        {
            if (interpreter)
            {
                interpreter_conv = convert_path(interpreter);
                sprintf(full, "%s %s ", interpreter_conv, filename_conv);
                free(interpreter);
                if(filename_conv)
                    free(filename_conv);
                if(interpreter_conv)
                    free(interpreter_conv);
            }
            else
            {
                sprintf(full, "%s ", filename_conv);
                if(filename_conv)
                    free(filename_conv);
            }

            for (cur = (char**)argv+1; *cur != 0; cur++)
            {
                strcat(full, *cur);
                strcat(full, " ");
            }

            createvars(envp);

            SystemTags(full, TAG_DONE);

            free(full);
            FUNCX;
            return 0;
        }

        if(interpreter)
            free(interpreter);
        if(filename_conv)
            free(filename_conv);

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
#ifdef CLIBHACK
/*current input output*/
        int i;
/*close conditions*/
        long amigafd[3];
        int amigafd_close[3] = {0, 0, 0};
#else
        /*current input output*/
        long amigafd[2];
        int amigafd_close[2] = {1, 1};
#endif
        char args[30];
        struct Process *proc = NULL;
        struct Task *thisTask = FindTask(0);
        struct userdata taskdata;

        char *name = NULL;

        FUNC;
#if 0
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

        taskdata.t      = t;
        taskdata.flags  = flags & (XEXEC | XERROK);
        taskdata.parent = thisTask;
#ifdef CLIBHACK
    for(i = 0; i < 3; i++)
        {
            __get_default_file(i, &amigafd[i]);
            if(close_fd == i) amigafd_close[i] = TRUE;
        }
#else
    if (t->type == 21)
        {
            if (amigain != -1)
            {
                amigafd_close[0] = FALSE;
                amigafd[0] = amigain;
            }
            else
                amigafd[0] = Open("CONSOLE:",MODE_OLDFILE);

            if (amigaout != -1)
                amigafd[1] = amigaout;
            else
                amigafd[1] = Open("CONSOLE:",MODE_OLDFILE);

        }
        else
        {
            amigafd[1] = Open("CONSOLE:",MODE_OLDFILE);
            amigafd[0] = Open("CONSOLE:",MODE_OLDFILE);
        }
#endif /*CLIBHACK*/

        if(t->str)
            name = strdup(t->str);

        proc = CreateNewProcTags(
            NP_Entry,               execute_child,
/*          NP_Child,               TRUE, */
            NP_Input,               amigafd[0],
            NP_Output,              amigafd[1],
#ifdef CLIBHACK
            NP_CloseOutput,         FALSE,
            NP_CloseInput,          FALSE,
            NP_Error,               amigafd[2],
            NP_CloseError,          FALSE,
#else
            NP_CloseOutput,         amigafd_close[1] ,
            NP_CloseInput,          amigafd_close[0] ,
#endif/*CLIBHACK*/
            NP_Cli,                 TRUE,
            NP_Name,                name,
#ifdef __amigaos4__
            NP_UserData,            (int)&taskdata,
#endif
            TAG_DONE);

#ifndef __amigaos4__
        proc->pr_Task.tc_UserData = &taskdata;
#endif

        Wait(SIGBREAKF_CTRL_F);
        if(name)
            free(name);
#ifdef CLIBHACK
        for(i=0; i < 3; i++)
            if(amigafd_close[i])
                close(i);
#else
/*close pipe input*/
        if (!amigafd_close[0])
                Close(amigafd[0]);
/*restore to stdin/out*/
        amigain = amigaout = -1;
#endif /*CLIBHACK*/

        FUNCX;
        return 0;
}

