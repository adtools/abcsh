#include "sh.h"
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/var.h>
#include <utility/hooks.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>

char **environ = NULL;


#ifdef __amigaos4__

#define MAX_ENV_SIZE 1024  /* maximum number of environ entries */

#ifdef AUTOINIT
#ifdef __GNUC__
void ___makeenviron() __attribute__((constructor));
void ___freeenviron() __attribute__((destructor));
#endif
#ifdef __VBCC__
#define ___makeenviron() _INIT_9_makeenviron()
#define ___freeenviron() _EXIT_9_makeenviron()
#endif
#endif


uint32
copy_env(struct Hook *hook, APTR userdata, struct ScanVarsMsg *message)
{
        static uint32 env_size = 1;  /* environ is null terminated */

        if(strlen(message->sv_GDir) <= 4)
        {
                if ( env_size == MAX_ENV_SIZE )
                {
                        return 0;
                }

                ++env_size;

                char **env = (char **)hook->h_Data;
                uint32 size = strlen(message->sv_Name) + 1 + message->sv_VarLen + 1 + 1;
                char *buffer=(char *)malloc(size);

                snprintf(buffer,size-1,"%s=%s", message->sv_Name, message->sv_Var);

                *env  = buffer;
                env++;
                hook->h_Data = env;
        }
        return 0;
}

void
___makeenviron()
{
        struct Hook hook;

        char varbuf[8];
        uint32 flags=0;
        size_t environ_size=MAX_ENV_SIZE * sizeof(char*);

        if(GetVar("ABCSH_IMPORT_LOCAL",varbuf,sizeof(varbuf),GVF_LOCAL_ONLY) > 0)
        {
            flags = GVF_LOCAL_ONLY;
        }
        else
        {
            flags = GVF_GLOBAL_ONLY;
        }

        environ = (char **)malloc(environ_size);
        if (!environ)
        {
                return;
        }

        memset(environ, 0, environ_size);

        memset(&hook, 0, sizeof(struct Hook));
        hook.h_Entry = copy_env;
        hook.h_Data = environ;

        ScanVars(&hook, flags, 0);
}

void
___freeenviron()
{
        char **i;
        for(i=environ;*i!=NULL;i++)
        {
                free(*i);
        }

        free(environ);
}
#endif
