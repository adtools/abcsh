#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>

#include <utility/hooks.h>
#include <dos/dos.h>
#include <dos/var.h>

#ifdef AUTOINIT
void ___makeenviron() __attribute__((constructor));
void ___freeenviron() __attribute__((destructor));
#endif

char **environ = NULL;

uint32 
size_env(struct Hook *hook, APTR userdata, struct ScanVarsMsg *message)
{
	uint32 size = (uint32)hook->h_Data;
	
	size += strlen(message->sv_Name) + 1 + message->sv_VarLen + 1;
	hook->h_Data = (APTR)size;
	
	return 0;
}

uint32 
copy_env(struct Hook *hook, APTR userdata, struct ScanVarsMsg *message)
{
	char *env = (char *)hook->h_Data;
	char buffer[1000];
	
	snprintf(buffer, 999, "%s=%s\n", message->sv_Name, message->sv_Var);
	buffer[998] = '\n';
	buffer[999] = 0;
	
	strcpy(buffer, env);
	env += strlen(buffer);
	
	hook->h_Data = (APTR)env;

	return 0;
}

void
___makeenviron() 
{
	struct Hook hook;
	
	hook.h_Entry = size_env;
	hook.h_Data = 0;
	
	IDOS->ScanVars(&hook, 0, 0);
	
   	environ = (char **)IExec->AllocVec((uint32)hook.h_Data, MEMF_ANY);
   	if (!environ)
   		return;
   	
   	hook.h_Entry = copy_env;
   	hook.h_Data = environ;
   	
	IDOS->ScanVars(&hook, 0, 0);
}

void
___freeenviron()
{
	IExec->FreeVec(environ);
}
