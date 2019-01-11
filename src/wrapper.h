//-----------------------------------------------------------------------------
/*!
   \file
   \brief Wrapper to access VxD services from C language.

   Some x86 informations:
   ----------------------
   Stack is addressed via stack pointer register ESP.
   CALL instruction pushes the return address on the stack.
   RET instruction removes the return address from stack.

   Register usage and calling convention:
   --------------------------------------
   Registers EAX, ECX and EDX are callee-save registers.
   Return value is passed via register EAX.
   C-Calling convention passed function parameter via stack from right to left.
   C-Calling convention says that the caller is responsible to remove the
   parameter when the call returns.
   Std-Call convention passed function parameter (also) via stack from right to left.
   However the callee is responsible to remove the parameter from the stack.
   The advantage of StdCall versus C-Call is that code is smaller, as the instructions
   to remove the parameter from the stack is required only once (inside the function),
   not at every occurance of the call.
   However Std-Call does not allow a variable number of arguments, which is possible
   when using C-Call.

   The calling convention can be coded into a function signature. E.g.
   int _cdecl myCCallFunction(int a, int b, int c);
   int _stdcall myStdCallFunction(int a, int b, int c);

   Inline Assembly:
   ----------------
   To mix C and assembly code, the technique of "inline assembly" can be used.
   In inline assembly user has not to take care about to preserve the registers
   EAX, ECX, EDD as well as EBX, EDI and ESI.
   However the compiler must "see", that these registers are used within the
   inline assembly. The compiler can't see what's going on, if code is emitted
   (using _emit xy), like the VxDCall and VMMCall macros do!
   Therefor, if a service called using VxDCall or VMMCall clobbers any of these
   registers, we don't have to preserve the register value "manually", but we have
   to let the compiler know about there usage, so that the compiler can take care
   about to preserve there value....
   To let the compiler know which registers are clobbered, it is enough to have
   them in the inline assembly code, like
      mov esi, parameter1
   However if the VxD service clobbers additional registers I use dummy instructions
   to let the compiler know about that ...
      sub reg, reg
   This dummy instruction tells the compiler that register "reg" is clobbered.
   In addition the "sub" instruction clobbers the CPU flags like "zero-flag" or
   "carry-flag". The comiler also notice that!

   If the inline assembly is using more than the 6 registers listed above,
   the programmer is responsible to save them in a proper way...
*/
//-----------------------------------------------------------------------------
#ifndef WRAPPER_H_
#define WRAPPER_H_


/* -- Includes ------------------------------------------------------------ */
#include "basedef.h"
#include "vmm.h"
#include "shell.h"



#ifdef __cplusplus
extern "C" {
#endif

/* -- Defines ------------------------------------------------------------- */
#define JC_FORWARED(n);  _asm _emit 0x72 _asm _emit n
#define JZ_FORWARED(n);  _asm _emit 0x74 _asm _emit n


/* -- Types --------------------------------------------------------------- */
typedef DWORD   ULONG;

/* -- Global Variables ---------------------------------------------------- */


/* -- Functions ------------------------------------------------------------ */

/*----------------------------------------------------------------------------
   \brief Send message (box) to user.

   \param   hvm         Handle of the VM that is responsible for the message
   \param   pszCaption  32-bit linear address of the message box caption
   \param   pszMessage  32-bit linear address of the message to display
----------------------------------------------------------------------------*/
VXDINLINE void SHELL_SendMessage(HVM hvm, PCHAR pszCaption, PCHAR pszMessage)
{
   //in inline assembly i don't have to preserve EAX, ECX, EDX as well as EBX, EDI, ESI
   _asm mov ebx, hvm            //virtual machine handle
   _asm mov eax, MB_SYSTEMMODAL //message box flags (0 ^= MB_OK)
   _asm mov ecx, pszMessage     //address of message text
   _asm mov edi, pszCaption     //address of caption text
   _asm sub esi, esi            //address of callback
   _asm sub edx, edx            //reference data for callback
   // VxDCall(SHELL_Message);
   VxDCall(SHELL_SYSMODAL_Message);
}



VXDINLINE WORD VCOMM_GetVersion(VOID)
{
   WORD version;

   // touch callee-save registers clobberd by VxDCall
   //  ....in order to let the inline-assembler know about that
   _asm sub eax, eax
   _asm sub ecx, ecx
   _asm sub edx, edx
   VxDCall(VCOMM_Get_Version);
   _asm mov version, ax
   return version;
}


VXDINLINE BOOL VCOMM_RegisterPortDriver(PFN pDriverControl)
{
   BOOL status;

   // touch callee-save registers clobberd by VxDCall
   //  ....in order to let the inline-assembler know about that
   _asm sub eax, eax
   _asm sub ecx, ecx
   _asm sub edx, edx
   // VxDCall is using C calling connvention
   _asm push pDriverControl
   VxDCall(_VCOMM_Register_Port_Driver);
   _asm mov status, eax
   _asm add esp, 1*4       //clean up stack
   return status;
}


VXDINLINE BOOL VCOMM_AddPort(DWORD refData, PFN pPortOpen, char * pPortName)
{
   BOOL status;

   // touch callee-save registers clobberd by VxDCall
   //  ....in order to let the inline-assembler know about that
   _asm sub eax, eax
   _asm sub ecx, ecx
   _asm sub edx, edx
   // VxDCall is using C calling connvention
   _asm push pPortName
   _asm push pPortOpen
   _asm push refData
   VxDCall(_VCOMM_Add_Port);
   _asm mov status, eax
   _asm add esp, 3*4       //clean up stack
   return status;
}



VXDINLINE void * List_CreateList(DWORD sizeOfElements, DWORD flags)
{
   void * list = NULL;

   _asm mov eax, flags
   _asm mov ecx, sizeOfElements
   _asm sub esi, esi       //touch clobberd registers by VxDCall
   VMMCall(List_Create);
   JC_FORWARED(3);         //skip next assignmet inscruction (3 bytes) in case of carry flag (error indicator)
   _asm mov list, esi
   return list;
}

VXDINLINE void * List_AllocateNode(void * list)
{
   void * node = NULL;

   _asm mov esi, list
   _asm sub eax, eax       //touch clobberd registers by VxDCall
   VMMCall(List_Allocate);
   JC_FORWARED(3);         //skip next assignmet inscruction (3 bytes) in case of carry flag (error indicator)
   _asm mov node, eax
   return node;
}


VXDINLINE void List_AttachNode(void * list, void * node)
{
   _asm mov esi, list
   _asm mov eax, node
   _asm sub ecx, ecx       //touch clobberd registers by VxDCal
   VMMCall(List_Attach_Tail);
}


VXDINLINE void * List_GetFirstNode(void * list)
{
   void * first = NULL;

   _asm mov esi, list
   _asm sub eax, eax       //touch clobberd registers by VxDCall
   VMMCall(List_Get_First);
   JZ_FORWARED(3);         //skip next assignmet inscruction (3 bytes) in case of zero flag (error indicator)
   _asm mov first, eax
   return first;
}


VXDINLINE void * List_GetNextNode(void * list, void * node)
{
   void * next = NULL;

   _asm mov esi, list
   _asm mov eax, node
   _asm sub ecx, ecx
   VMMCall(List_Get_Next);
   JZ_FORWARED(3);      //skip next assignmet inscruction (3 bytes) in case of zero flag (error indicator)
   _asm mov next, eax
   return next;
}




VXDINLINE void * Heap_Allocate(DWORD numOfBytes, DWORD flags)
{
   void * memory = NULL;

   // touch callee-save registers clobberd by VxDCall
   //  ....in order to let the inline-assembler know about that
   _asm sub eax, eax
   _asm sub ecx, ecx
   _asm sub edx, edx
   // VxDCall is using C calling connvention
   _asm push flags
   _asm push numOfBytes
   VxDCall(_HeapAllocate);
   _asm mov memory, eax
   _asm add esp, 2*4            //clean up stack
   return memory;
}


/*----------------------------------------------------------------------------
   \brief Read data out of registry

   \param   dnDevNode      Handle to a device node. The devnode can have a NULL device ID, but in this case CR_NO_SUCH_VALUE is returned
   \param   pszSubKey      Name of the subkey. Can be NULL if none.
   \param   pszValueName   Name of the value
   \param   ulExpectedType Either REG_SZ (1) if a string is wanted or REG_BINARY (3) if a binary value is wanted
   \param   pvBuffer       Address of the buffer that receives the registry data. This can be NULL if you just want to get the size of the value.
   \param   pulLength      The length of the buffer (both input and output)
   \param   ulFlags        Must be a combination of the following
                           - CM_REGISTRY_HARDWARE (0): Select the hardware branch
                           - CM_REGISTRY_SOFTWARE (1): Select the softwar branch
                           - CM_REGISTRY_USER (0x100): Use HKEY_CURRENT_USER

   \retval CR_SUCCESS (0)  if the function is successful
   \retval CR_INVALID_DEVNODE
   \retval CR_INVALID_FLAG
   \retval CR_INVALID_POINTER
   \retval CR_NO_SUCH_VALUE
   \retval CR_WRONG_TYPE
   \retval CR_REGITRY_ERROR
   \retval CR_BUFFER_SMALL
----------------------------------------------------------------------------*/
enum { ___CONFIGMG_Read_Registry_Value = 0x33003E }; //include von configmg.h verursacht probleme. deswegen muss ich hier selbst berechnen...
VXDINLINE DWORD CONFIGMG_ReadRegistryValue(DWORD dnDevNode, char * pszSubKey, char * pszValueName,
                                           DWORD ulExpectedType, void * pvBuffer, DWORD * pulLength,
                                           DWORD ulFlags)
{
   DWORD status;

   // touch callee-save registers clobberd by VxDCall
   //  ....in order to let the inline-assembler know about that
   _asm sub eax, eax
   _asm sub ecx, ecx
   _asm sub edx, edx
   // VxDCall is using C calling connvention
   _asm push ulFlags
   _asm push pulLength
   _asm push pvBuffer
   _asm push ulExpectedType
   _asm push pszValueName
   _asm push pszSubKey
   _asm push dnDevNode
   VxDCall(_CONFIGMG_Read_Registry_Value);
   _asm mov status, eax
   _asm add esp, 7*4               //clean up stack
   return status;
}


VXDINLINE DWORD System_GetTime(void)
{
   DWORD time1ms; //time in milli seconds, since windows was started

   _asm sub eax, eax    //touch eax register
   VMMCall(Get_System_Time);
   _asm mov time1ms, eax
   return time1ms;
}






#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
