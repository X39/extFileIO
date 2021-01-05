#pragma once
#if defined(_MSC_BUILD)
#define DLLEXPORT __declspec (dllexport)
#define STDCALL __stdcall
#else
#define DLLEXPORT __attribute__((dllexport))
#define STDCALL 
#endif


extern "C"
{
    //--- Called by Engine on extension load 
    DLLEXPORT void STDCALL RVExtensionVersion(char* output, int outputSize);
    //--- STRING callExtension STRING
    DLLEXPORT void STDCALL RVExtension(char* output, int outputSize, const char* function);
    //--- STRING callExtension ARRAY
    DLLEXPORT int STDCALL RVExtensionArgs(char* output, int outputSize, const char* function, const char** argv, int argc);
}