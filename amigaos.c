#include <string.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <dos.h>

#include "sh.h"

#include <dos/dos.h>
#ifndef CLIBHACK
#include <dos/dostags.h>
#endif
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/utility.h>

char * __stdio_window_specification = "CON:20/20/600/150/"ABC_VERSION"/AUTO/CLOSE";

#define FUNC //printf("<%s>\n", __PRETTY_FUNCTION__);fflush(stdout);
#define FUNCX //printf("</%s>\n", __PRETTY_FUNCTION__);fflush(stdout);

#define __USE_RUNCOMMAND__


/* clib2 specific controls */
int __minimum_os_lib_version = 51;
char * __minimum_os_lib_error = "Requires AmigaOS 4.0";
BOOL __open_locale = FALSE;


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

static int pipenum = 0;

int pipe(int filedes[2])
{
        char pipe_name[1024];

#ifdef USE_TEMPFILES
#if defined(__amigaos4__)
        sprintf(pipe_name, "/T/%x.%08x", pipenum++,((struct Process*)FindTask(0))->pr_ProcessID);
#else
        sprintf(pipe_name, "/T/%x.%08x", pipenum++,GetUniqueID());
#endif
#else
#if defined(__amigaos4__)
        sprintf(pipe_name, "/PIPE/%x%08x/4096/0", pipenum++,((struct Process*)FindTask(0))->pr_ProcessID);
#else
        sprintf(pipe_name, "/PIPE/%x%08x/4096/0", pipenum++,GetUniqueID());
#endif
#endif

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

        ksh_dup2(mystdin, 0, false);
        ksh_dup2(mystdout, 1, false);
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

char *convert_path_a2u(const char *filename)
{
    struct name_translation_info nti;

    if(!filename)
    {
        return 0;
    }

    __translate_amiga_to_unix_path_name(&filename,&nti);

    return strdup(filename);

}
char *convert_path_u2a(const char *filename)
{
        struct name_translation_info nti;
        FUNC;

        if (!filename)
        {
            FUNCX;
            return 0;
        }

        if (strcmp(filename, "/dev/tty") == 0)
        {
            return  strdup( "CONSOLE:");;
            FUNCX;

        }

        __translate_unix_to_amiga_path_name(&filename,&nti);

        return strdup(filename);
}

static void createvars(char * const* envp)
{
    /* Set a loal var to indicate to any subsequent sh that it is not */
    /* The top level shell and so should only inherit local amigaos vars */

    SetVar("ABCSH_IMPORT_LOCAL","true",5,GVF_LOCAL_ONLY);

    while(*envp != NULL)
    {
        int len;
        char *var;
        char *val;


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

static bool contains_whitespace(char *string)
{

    if(strchr(string,' ')) return true;
    if(strchr(string,'\t')) return true;
    if(strchr(string,'\n')) return true;
    if(strchr(string,0xA0)) return true;
    if(strchr(string,'"')) return true;
    return false;
}

static int no_of_escapes(char *string)
{
    int cnt = 0;
    char *p;
    for(p=string;p<string + strlen(string);p++)
    {
        if(*p=='"') cnt++;
        if(*p=='*') cnt++;
        if(*p=='\n') cnt++;
        if(*p=='\t') cnt++;
    }
    return cnt;
}

struct command_data
{
    STRPTR args;
    BPTR seglist;
    struct Task *parent;
};

int execve(const char *filename, char *const argv[], char *const envp[])
{
        FILE *fh;
        char buffer[1000];
        int size = 0;
        char **cur;
        char *interpreter = 0;
        char * interpreter_args = 0;
        char *full = 0;
        char *filename_conv = 0;
        char *interpreter_conv = 0;
        char *tmp = 0;
        char *fname;
        int tmpint;
        uint32 error;
        struct Task *thisTask = FindTask(0);

        FUNC;

        /* Calculate the size of filename and all args, including spaces and quotes */
        size = 0;//strlen(filename) + 1;
        for (cur = (char **)argv+1; *cur; cur++)
        {
            size += strlen(*cur) + 1 + (contains_whitespace(*cur)?(2 + no_of_escapes(*cur)):0);
        }
        /* Check if it's a script file */

        fh = fopen(filename, "r");
        if (fh)
        {
                if (fgetc(fh) == '#' && fgetc(fh) == '!')
                {
                        char *p;
                        char *q;
                        fgets(buffer, 999, fh);
                        p = buffer;
                        while (*p == ' ' || *p == '\t') p++;
                        if(buffer[strlen(buffer) -1] == '\n') buffer[strlen(buffer) -1] = '\0';
                        if(q = strchr(p,' '))
                        {
                            *q++ = '\0';
                            if(*q != '\0')
                            {
                            interpreter_args = strdup(q);
                            }
                        }
                        else interpreter_args=strdup("");

                        interpreter = strdup(p);
                        size += strlen(interpreter) + 1;
                        size += strlen(interpreter_args) +1;
                }

                fclose(fh);
        }
        else
        {
            /* We couldn't open this why not? */
            if(errno == ENOENT)
            {
                /* file didn't exist! */
                return -1;
            }
        }


        /* Allocate the command line */
        filename_conv = convert_path_u2a(filename);


        if(filename_conv)
            size += strlen(filename_conv);
        size += 1;
        full = malloc(size+10);
        if (full)
        {
            if (interpreter)
            {
                interpreter_conv = convert_path_u2a(interpreter);
#if !defined(__USE_RUNCOMMAND__)
#warning (using system!)
                sprintf(full, "%s %s %s ", interpreter_conv, interpreter_args,filename_conv);
#else
                sprintf(full, "%s %s ",interpreter_args, filename_conv);
#endif
                free(interpreter);
                free(interpreter_args);

                if(filename_conv)
                    free(filename_conv);
               fname = strdup(interpreter_conv);

                if(interpreter_conv)
                    free(interpreter_conv);
            }
            else
            {
#ifndef __USE_RUNCOMMAND__
                sprintf(full, "%s ", filename_conv);
#else
                sprintf(full,"");
#endif
                fname = strdup(filename_conv);
                if(filename_conv)
                    free(filename_conv);
            }

            for (cur = (char**)argv+1; *cur != 0; cur++)
            {
                if(contains_whitespace(*cur))
                {
                    int esc = no_of_escapes(*cur);
                    if(esc > 0)
                    {
                        char *buff=malloc(strlen(*cur) + 4 + esc);
                        char *p = *cur;
                        char *q = buff;

                        *q++ = '"';
                        while(*p != '\0')
                        {
                            if(*p == '\n'){ *q++ = '*'; *q++ = 'N';p++;continue;}
                            else if(*p == '"'){ *q++ = '*'; *q++ = '"';p++;continue;}
                            else if(*p == '*' ){ *q++ = '*';}
                            *q++ = *p++;
                        }
                        *q++ = '"';
                        *q++ = ' ';
                        *q='\0';
                        strcat(full,buff);
                        free(buff);
                    }
                    else
                    {
                        strcat(full,"\"");
                        strcat(full,*cur);
                        strcat(full,"\" ");
                    }
                }
                else
                {
                    strcat(full, *cur);
                    strcat(full, " ");
                }

            }
            strcat(full,"\n");

            createvars(envp);

#ifndef __USE_RUNCOMMAND__
            lastresult = SystemTags(full,
                SYS_UserShell,true,
                NP_StackSize,  ((struct Process *)thisTask)->pr_StackSize,
                SYS_Input,((struct Process *)thisTask)->pr_CIS,
                SYS_Output,((struct Process *)thisTask)->pr_COS,
                SYS_Error,((struct Process *)thisTask)->pr_CES,
              TAG_DONE);
#else

            if (fname){

                BPTR seglist = LoadSeg(fname);
                if(seglist)
                {


                    /* check if we have an executable! */
                    struct PseudoSegList *ps = NULL;
                    if(!GetSegListInfoTags( seglist, GSLI_Native, &ps, TAG_DONE))
                    {
                        if(!GetSegListInfoTags( seglist, GSLI_68KPS, &ps, TAG_DONE))
                        {
                            GetSegListInfoTags( seglist, GSLI_68KHUNK, &ps, TAG_DONE);
                        }
                    }

                    if( ps != NULL )
                    {
                        SetProgramName(fname);
                        lastresult=RunCommand(seglist,8*1024,full,strlen(full));
                        errno=0;
                    }
                    else
                    {
                        errno=ENOEXEC;
                    }
                    UnLoadSeg(seglist);

                }
                else
                {
                    errno=ENOEXEC;
                }
               free(fname);
            }

#endif /* USE_RUNCOMMAND */

            free(full);
            FUNCX;
            if(errno == ENOEXEC) return -1;
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
        struct globals globenv;

        this = FindTask(0);
        t = ((struct userdata *)this->tc_UserData)->t;
        flags = ((struct userdata*)this->tc_UserData)->flags;
        parent = ((struct userdata*)this->tc_UserData)->parent;

        copyenv(&globenv);
        e->type=E_SUBSHELL;
        if(!(ksh_sigsetjmp(e->jbuf,0)))
        {
            execute(t, flags & (XEXEC | XERROK));
        }
        else
        {
            lastresult = exstat;
        }
        restoreenv(&globenv);
#if !defined(__amigaos4__)
        Forbid();
        Signal(parent, SIGBREAKF_CTRL_F);
#endif

        return 0;
}

int
exchild(struct op *t, int flags,
        int close_fd)   /* used if XPCLOSE or XCCLOSE */
{
#ifdef CLIBHACK
/*current input output*/
        int i;
/*close conditions*/
        long amigafd[3];
        int amigafd_close[3] = {0, 0, 0};
        bool procmayclose = false;
#else
        /*current input output*/
        long amigafd[2];
        int amigafd_close[2] = {1, 1};
#endif
        struct Process *proc = NULL;
        struct Task *thisTask = FindTask(0);
        struct userdata taskdata;

        char *name = NULL;

        FUNC;
        if(flags & XEXEC)
        {
            execute(t,flags&(XEXEC | XERROK));
        }
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
            BPTR lock;
            __get_default_file(i, &amigafd[i]);
            if(close_fd == i) amigafd_close[i] = true;

        }
#else
    if (t->type == 21)
        {
            if (amigain != -1)
            {
                amigafd_close[0] = false;
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
        else name=strdup("new sh process");

        proc = CreateNewProcTags(
            NP_Entry,                execute_child,
            NP_Child,                true,
            NP_StackSize,            ((struct Process *)thisTask)->pr_StackSize,
            NP_Input,                amigafd[0],
            NP_Output,               amigafd[1],
#ifdef CLIBHACK
            NP_CloseOutput,          false,
            NP_CloseInput,           false,
            NP_Error,                amigafd[2],
            NP_CloseError,           false,
#else
            NP_CloseOutput,          amigafd_close[1] ,
            NP_CloseInput,           amigafd_close[0] ,
#endif/*CLIBHACK*/
            NP_Cli,                  true,
            NP_Name,                 name,
#ifdef __amigaos4__
            NP_UserData,             (int)&taskdata,
            NP_NotifyOnDeathSigTask, thisTask,
#endif
            TAG_DONE);

#ifndef __amigaos4__
#warning this code has been included!
        proc->pr_Task.tc_UserData = &taskdata;
        Wait(SIGBREAKF_CTRL_F);
#else
        Wait(SIGF_CHILD);
#endif

        if(name)
            free(name);
#ifdef CLIBHACK
//        for(i=0; i < 3; i++)
//            if(amigafd_close[i]  && (flags & (XPCLOSE)))
//            {
          if((i=close_fd) >=0 && (flags & XPCLOSE) )
          {
#ifdef USE_TEMPFILES
                BPTR f=0;
                UBYTE pname[256];

                if(flags & XPIPEI)
                {

                      __get_default_file(i,&f);
                      NameFromFH(f,pname,255);

                }

                close(i);
                if(f)
                {
                    DeleteFile(pname);
                }
#else  /* using true pipes */

                if(flags & XPIPEI)
                {

                    char buffer[256];
                    int n,t;
                    t = 0;
                    while((n = read(i,buffer,255)) > 0) t +=n;
                }

                close(i);
#endif  /* USE_TEMPFILES */

           }
#else
/*close pipe input*/
        if (!amigafd_close[0])
                Close(amigafd[0]);
/*restore to stdin/out*/
        amigain = amigaout = -1;
#endif /*CLIBHACK*/

        FUNCX;
        return lastresult;
}

bool *assign_posix(void)
{
        AssignPath("bin", "SDK:C");
}

char *wb_init(void)
{
        Open("CON:20/20/600/150/abc-shell/AUTO/CLOSE", MODE_NEWFILE);
}

/* The following are wrappers for selected fcntl.h functions */
/* They allow usage of absolute amigaos paths as well unix style */
/* have added the rest of the functions that makes use of paths */

#ifdef open
#undef open
#endif
#ifdef stat
#undef stat
#endif
#ifdef lstat
#undef lstat
#endif
#ifdef chdir
#undef chdir
#endif
#ifdef opendir
#undef opendir
#endif
#ifdef access
#undef access
#endif
#ifdef readlink
#undef readlink
#endif
#ifdef unlink
#undef unlink
#endif


int __open(const char * path, int open_flag)
{
        struct name_translation_info nti;
        bool have_colon = false;
        char *p = (char *)path;

        while(p<path + strlen(path))
        {
            if(*p++ == ':') have_colon=true;
        }
        if(have_colon)
        {
            __translate_amiga_to_unix_path_name(&path,&nti);
        }
        return open(path, open_flag);

}

int __stat(const char * path, struct stat *buffer)
{
        struct name_translation_info nti;
        bool have_colon = false;
        char *p = (char *)path;
        int res;
        APTR old_proc_window;

        while(p<path + strlen(path))
        {
            if(*p++ == ':')have_colon=true;
        }
        if(have_colon)
        {
            __translate_amiga_to_unix_path_name(&path,&nti);
        }
        old_proc_window = SetProcWindow((APTR)-1);
        res = stat(path, buffer);
        SetProcWindow(old_proc_window);
        return res;
}

int __lstat(const char * path, struct stat *buffer)
{
        struct name_translation_info nti;
        bool have_colon = false;
        char *p = (char *)path;

        while(p<path + strlen(path))
        {
            if(*p++ == ':') have_colon=true;
        }
        if(have_colon)
        {
            __translate_amiga_to_unix_path_name(&path,&nti);
        }

        return lstat(path, buffer);

}

int __chdir(const char * path)
{
        struct name_translation_info nti;
        bool have_colon = false;
        char *p = (char *)path;

        while(p<path + strlen(path))
        {
            if(*p++ == ':') have_colon=true;
        }
        if(have_colon)
        {
            __translate_amiga_to_unix_path_name(&path,&nti);
        }
        return chdir(path);

}

int __opendir(const char * path)
{
        struct name_translation_info nti;
        bool have_colon = false;
        char *p = (char *)path;

        while(p<path + strlen(path))
        {
            if(*p++ == ':') have_colon=true;
        }
        if(have_colon)
        {
            __translate_amiga_to_unix_path_name(&path,&nti);
        }
        return opendir(path);

}

int __access(const char * path, int mode)
{
        struct name_translation_info nti;
        bool have_colon = false;
        char *p = (char *)path;
        int res;
        APTR old_proc_window;

        while(p<path + strlen(path)) {
            if(*p++ == ':') have_colon=true;
        }
        if(have_colon)
        {
            __translate_amiga_to_unix_path_name(&path,&nti);
        }
        old_proc_window = SetProcWindow((APTR)-1);
        res = access(path, mode);
        SetProcWindow(old_proc_window);
        return res;
}

/* not changing getcwd */

int __readlink(const char * path, char * buffer, int buffer_size)
{
        struct name_translation_info nti;
        bool have_colon = false;
        char *p = (char *)path;

        while(p<path + strlen(path))
        {
            if(*p++ == ':') have_colon=true;
        }
        if(have_colon)
        {
            __translate_amiga_to_unix_path_name(&path,&nti);
        }
        return readlink(path, buffer, buffer_size);

}

int __unlink(const char * path)
{
        struct name_translation_info nti;
        bool have_colon = false;
        char *p = (char *)path;

        while(p<path + strlen(path))
        {
            if(*p++ == ':') have_colon=true;
        }
        if(have_colon)
        {
            __translate_amiga_to_unix_path_name(&path,&nti);
        }
        return unlink(path);

}

char *convert_path_multi(const char *path)
{
        struct name_translation_info nti;
        bool have_colon = false;
        char *p = (char *)path;

        while(p<path + strlen(path))
        {
            if(*p++ == ':') have_colon=true;
        }
        if(have_colon)
        {
            __translate_amiga_to_unix_path_name(&path,&nti);
        }
    return strdup(path);
}
