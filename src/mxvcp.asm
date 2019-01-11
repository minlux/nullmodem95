;******************************************************************************
; MXVCP.ASM: VxD to implement pairs of virutal com ports (virtually connected together via nullmodem cable).
;******************************************************************************
.386p
include   VMM.INC
include   VCOMM.INC


EXTRN _MXVCP_DeviceInit:PROC



;******************************************************************************
;               V I R T U A L   D E V I C E   D E C L A R A T I O N
;******************************************************************************
Declare_Virtual_Device MXVCP, 1, 00, MXVCP_Control, Undefined_Device_ID, VCD_Init_Order,,,



VxD_Locked_Code_Seg

   ;------------------------------------------------------------------------------
   ; MXVCP_Control: Description: Dispatcher for system messages.
   ;------------------------------------------------------------------------------
   BeginProc MXVCP_Control
;      Control_Dispatch DEVICE_INIT, MXVCP_DeviceInit, cCall, <ebx>
      Control_Dispatch SYS_DYNAMIC_DEVICE_INIT, _MXVCP_DeviceInit, cCall, <ebx>
      Control_Dispatch SYS_DYNAMIC_DEVICE_EXIT, _MXVCP_DeviceExit, cCall, <ebx>
      clc
      ret
   EndProc MXVCP_Control


VxD_Locked_Code_Ends



end
