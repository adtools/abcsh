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
size_env(struct Hook *hook, APTR userdata, struct ScanVarsMsg *message)
{
        if(strlen(message->sv_GDir) <= 4)
        {
                hook->h_Data = (APTR)(((uint32)hook->h_Data) + 1);
        }
        return 0;
}

uint32
copy_env(struct Hook *hook, APTR userdata, struct ScanVarsMsg *message)
{
        if(strlen(message->sv_GDir) <= 4)
        {
                char **env = (char **)hook->h_Data;
                uint32 size = strlen(message->sv_Name) + 1 + message->sv_VarLen + 1 + 1;
                char *buffer=(char *)AllocVec((uint32)size,MEMF_ANY|MEMF_CLEAR);

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

        if(GetVar("ABCSH_IMPORT_LOCAL",varbuf,8,GVF_LOCAL_ONLY) > 0)
        {
            flags = GVF_LOCAL_ONLY;
        }
        else
        {
            flags = GVF_GLOBAL_ONLY;
        }


        hook.h_Entry = size_env;
        hook.h_Data = 0;

        ScanVars(&hook, flags, 0);
        hook.h_Data = (APTR)(((uint32)hook.h_Data) + 1);

        environ = (char **)AllocVec((uint32)hook.h_Data*sizeof(char **), MEMF_ANY|MEMF_CLEAR );
        if (!environ)
        {
                return;
        }
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
                FreeVec(*i);
        }

        FreeVec(environ);
}
#endif
