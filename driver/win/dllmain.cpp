#include "../platform.h"

/// Saved module handle.
HINSTANCE module_instance = 0;

extern "C" 
{

BOOL WINAPI
    DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            module_instance = (HINSTANCE)hInst;	/* Save for dialog boxes */
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_PROCESS_DETACH:
            break;

        case DLL_THREAD_DETACH:
            break;

        default:
            break;
    }

    return TRUE;
}

}
