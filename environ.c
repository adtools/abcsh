#include "sh.h"
#include <utility/hooks.h>
#include <dos/dos.h>
#include <dos/var.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>

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

char **environ = NULL;

uint32
size_env(struct Hook *hook, APTR userdata, struct ScanVarsMsg *message)
{
        if(strlen(message->sv_GDir) == 4)
        {
                (uint32)hook->h_Data++;
        }
        return 0;
}

uint32
copy_env(struct Hook *hook, APTR userdata, struct ScanVarsMsg *message)
{
        if(strlen(message->sv_GDir) == 4)
        {
                char **env = (char **)hook->h_Data;
                uint32 size = strlen(message->sv_Name) + 1 + message->sv_VarLen + 1 + 1;
                char *buffer=(char *)AllocVec((uint32)size,MEMF_ANY|MEMF_CLEAR);

                sprintf(buffer, "%s=%s\n", message->sv_Name, message->sv_Var);

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

        hook.h_Entry = size_env;
        hook.h_Data = 0;

        ScanVars(&hook, GVF_GLOBAL_ONLY, 0);
        hook.h_Data++;

        environ = (char **)AllocVec((uint32)hook.h_Data*sizeof(char **), MEMF_ANY|MEMF_CLEAR );
        if (!environ)
        {
                return;
        }
        hook.h_Entry = copy_env;
        hook.h_Data = environ;

        ScanVars(&hook, GVF_GLOBAL_ONLY, 0);
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
