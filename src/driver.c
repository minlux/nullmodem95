/* -- Includes ------------------------------------------------------------ */
#include "basedef.h"
// #include "vxdwraps.h"
#include "vmm.h"
#include "vcomm.h"
#include "wrapper.h"
#include "stdutils.h"


/* -- Defines ------------------------------------------------------------- */
#define FIFO_SIZE_1BY         (512) //size of fifo buffer
#define NUMBER_OF_PORTS       (6)   //shall be a multiple of 2 (as we build pairs!)
#define PORTNAME_LENGTH       (16)

/* -- Types --------------------------------------------------------------- */
typedef struct _PortInformation PortInformation; //forward declaration

/*----------------------------------------------------------------------------
   \brief Client-defined callback function that VCOMM calls when the transmit or receive queue passes a
   specified threshold or a communications event occurs.

   \param   hPort          Handle of the communications resource.
   \param   lReferenceData 32-bit variable passed to _VCOMM_EnableCommNotification.
   \param   lEvent         Event typ.
                           -CN_RECEIVED
                           -CN_TRANSMIT
                           -CN_EVENT
   \param   lSubEvent      Enabled communications events that were detected, if the lEvent parameter
                           is CN_EVENT. Otherwise, this parameter is ignored.
                           - See comm.doc, page 26
----------------------------------------------------------------------------*/
typedef void (_cdecl * PCommNotifyProc)(PortInformation * hPort, DWORD lReferenceData,
                                        DWORD lEvent, DWORD lSubEvent);



/*----------------------------------------------------------------------------
   Contains information about an open port. The first field must be a PORTDATA_t structure; additional fields can
   contain information specific to a particular port driver. The PortOpen function returns the address of this
   structure and VCOMM specifies the address in subsequent calls to port-driver functions.
----------------------------------------------------------------------------*/
struct _PortInformation
{
   PortData portData;   //port data: has to be first
   char portName[PORTNAME_LENGTH];
   char pairPortName[PORTNAME_LENGTH];
   PortInformation * pairPort;
   BOOL  isOpen;
   DWORD eventMask;
   DWORD * eventRegister;
   PCommNotifyProc eventCallback;
   PCommNotifyProc txCallback;
   DWORD txCallbackParameter;
   long txCallbackTriggerLevel;
   PCommNotifyProc rxCallback;
   DWORD rxCallbackParameter;
   long rxCallbackTriggerLevel;
};


/*----------------------------------------------------------------------------
   Contains the addresses of port-driver functions. If a port driver
   does not provide a particular function, the corresponding field
   should be NULL. The PORTDATA_t structure contains the address of
   the PORTFUNCTIONS_t structure for a port driver.
----------------------------------------------------------------------------*/
typedef struct _PortFunctionTable
{
   BOOL (_cdecl *pPortSetCommState)(PortInformation * hPort, _DCB * dcbPort, DWORD ActionMask);
   BOOL (_cdecl *pPortGetCommState)(PortInformation * hPort, _DCB * dcbPort);
   BOOL (_cdecl *pPortSetup)(PortInformation * hPort, void * RxQueue, DWORD cbRxQueue, void * TxQueue, DWORD cbTxQueue);
   BOOL (_cdecl *pPortTransmitChar)(PortInformation * hPort, DWORD ch);
   BOOL (_cdecl *pPortClose)(PortInformation * hPort); //address of PortClose
   BOOL (_cdecl *pPortGetQueueStatus)(PortInformation * hPort, _COMSTAT * cmst);
   BOOL (_cdecl *pPortClearError)(PortInformation * hPort, _COMSTAT * cmst);
   BOOL (_cdecl *pPortSetModemStatusShadow)(PortInformation * hPort, DWORD dwEventMask, BYTE * MSRShadow);
   BOOL (_cdecl *pPortGetProperties)(PortInformation * hPort, _COMMPROP * cmmp);
   BOOL (_cdecl *pPortEscapeFunction)(PortInformation * hPort, DWORD lFunc, DWORD InData, DWORD * OutData);
   BOOL (_cdecl *pPortPurge)(PortInformation * hPort, DWORD dwQueueType);
   BOOL (_cdecl *pPortSetEventMask)(PortInformation * hPort, DWORD dwMask, DWORD * dwEvents);
   BOOL (_cdecl *pPortGetEventMask)(PortInformation * hPort, DWORD dwMask, DWORD * dwEvents);
   BOOL (_cdecl *pPortWrite)(PortInformation * hPort, void * achBuffer, DWORD cchRequested, DWORD * cchWritten);
   BOOL (_cdecl *pPortRead)(PortInformation * hPort, void * achBuffer, DWORD cchRequested, DWORD * cchReceived);
   BOOL (_cdecl *pPortEnableNotification)(PortInformation * hPort, PCommNotifyProc commNotifyProc, DWORD lReferenceData);
   BOOL (_cdecl *pPortSetReadCallback)(PortInformation * hPort, long rxTrigger, PCommNotifyProc commNotifyProc, DWORD lReferenceData);
   BOOL (_cdecl *pPortSetWriteCallback)(PortInformation * hPort, long txTrigger, PCommNotifyProc commNotifyProc, DWORD lReferenceData);
   BOOL (_cdecl *pPortGetModemStatus)(PortInformation * hPort, DWORD * dwModemStatus);
   BOOL (_cdecl *pPortGetCommConfig)(PortInformation * hPort, _DCB * dcbPort, DWORD * dwSize);
   BOOL (_cdecl *pPortSetCommConfig)(PortInformation * hPort, _DCB * dcbPort, DWORD * dwSize);
   BOOL (_cdecl *pPortGetWin32Error)(PortInformation * hPort, DWORD * dwError);
   BOOL (_cdecl *pPortDeviceIOCtl)(PortInformation * hPort, ...);
} PortFunctionTable;



/* -- Module Global Function Prototypes ----------------------------------- */
static void _cdecl m_DriverControl(DWORD fCode, DWORD DevNode, DWORD DCRefData,
                                   DWORD AllocBase, DWORD AllocIrq, char *portName);

static PortInformation * _cdecl m_PortOpen(char *PortName, DWORD VMId, long *lpError);
static BOOL _cdecl m_PortSetup(PortInformation * hPort, void * RxQueue, DWORD cbRxQueue,
                               void * TxQueue, DWORD cbTxQueue);
static BOOL _cdecl m_PortClose(PortInformation * hPort);

static BOOL _cdecl m_PortRead(PortInformation * hPort, void * achBuffer, DWORD cchRequested, DWORD * cchReceived);
static BOOL _cdecl m_PortWrite(PortInformation * hPort, void * achBuffer, DWORD cchRequested, DWORD * cchWritten);
static BOOL _cdecl m_PortTransmitChar(PortInformation * hPort, DWORD ch);
static BOOL _cdecl m_PortPurge(PortInformation * hPort, DWORD dwQueueType);
static BOOL _cdecl m_PortGetQueueStatus(PortInformation * hPort, _COMSTAT * cmst);

static BOOL _cdecl m_PortGetEventMask(PortInformation * hPort, DWORD dwMask, DWORD * dwEvents);
static BOOL _cdecl m_PortSetEventMask(PortInformation * hPort, DWORD dwMask, DWORD * dwEvents);
static BOOL _cdecl m_PortEnableNotification(PortInformation * hPort, PCommNotifyProc commNotifyProc, DWORD lReferenceData);
static BOOL _cdecl m_PortSetReadCallback(PortInformation * hPort, long rxTrigger, PCommNotifyProc commNotifyProc,
                                         DWORD lReferenceData);
static BOOL _cdecl m_PortSetWriteCallback(PortInformation * hPort, long txTrigger, PCommNotifyProc commNotifyProc,
                                          DWORD lReferenceData);

static BOOL _cdecl m_PortGetProperties(PortInformation * hPort, _COMMPROP * cmmp);
static BOOL _cdecl m_PortGetCommConfig(PortInformation * hPort, _DCB * dcbPort, DWORD * dwSize);
static BOOL _cdecl m_PortSetCommConfig(PortInformation * hPort, _DCB * dcbPort, DWORD * dwSize);
static BOOL _cdecl m_PortGetCommState(PortInformation * hPort, _DCB * dcbPort);
static BOOL _cdecl m_PortSetCommState(PortInformation * hPort, _DCB * dcbPort, DWORD ActionMask);
static BOOL _cdecl m_PortGetModemStatus(PortInformation * hPort, DWORD * dwModemStatus);
static BOOL _cdecl m_PortSetModemStatusShadow(PortInformation * hPort, DWORD dwEventMask, BYTE * MSRShadow);

static BOOL _cdecl m_PortClearError(PortInformation * hPort, _COMSTAT * cmst);
static BOOL _cdecl m_PortGetWin32Error(PortInformation * hPort, DWORD * dwError);
static BOOL _cdecl m_PortEscapeFunction(PortInformation * hPort, DWORD lFunc, DWORD InData, DWORD * OutData);




/* -- Module Global Variables --------------------------------------------- */
static HVM m_SysVmHandle;
static PortFunctionTable m_PortFunctionTable =
{
   &m_PortSetCommState,
   &m_PortGetCommState,
   &m_PortSetup,
   &m_PortTransmitChar,
   &m_PortClose,
   &m_PortGetQueueStatus,
   &m_PortClearError,
   &m_PortSetModemStatusShadow,
   &m_PortGetProperties,
   &m_PortEscapeFunction,
   &m_PortPurge,
   &m_PortSetEventMask,
   &m_PortGetEventMask,
   &m_PortWrite,
   &m_PortRead,
   &m_PortEnableNotification,
   &m_PortSetReadCallback,
   &m_PortSetWriteCallback,
   &m_PortGetModemStatus,
   &m_PortGetCommConfig,
   &m_PortSetCommConfig,
   &m_PortGetWin32Error,
   NULL
};
static PortInformation m_PortInformation[NUMBER_OF_PORTS];
static BYTE m_FifoBuffer[NUMBER_OF_PORTS][FIFO_SIZE_1BY]; //each port has only an input (receive) buffer.
static unsigned int m_NextFreePort;

//for debugging purpose in combination with SHELL_SendMessage
#if 0
   //SHELL_SendMessage is an asynchronous service, that requres the data stay available until execution
   //thats why the the message buffer is static.
   //Furthermore the data must be in a locked data segment (not in a pageable!). The "mxvcp.def"  locates
   //_BSS (as well _DATA and _CODE) into a locked data segment. Therefor i don't have to to use special
   //pragmas here, to fore location in a locked data segment.
   #define dbgMsgLen   (sizeof(dbgMsg))
   static char dbgMsg[128];
#endif

/* -- Implementation ------------------------------------------------------ */


//thats an overlay for the PortData->QInAddr resp. PortData->QOutAddr
typedef struct _PortFifo
{
   BYTE * QxAddr;       // Address of the queue
   DWORD QxSize;        // Length of queue in bytes
   DWORD reserved[2];
   DWORD QxCount;       // # of bytes currently in queue
   DWORD QxGet;         // Offset into q to get bytes from
   DWORD QxPut;         // Offset into q to put bytes in
} PortFifo;

static __inline void m_FifoInit(PortInformation * hPort, BYTE * buffer, DWORD size)
{
   //initialize input (receive) buffer
   PortFifo * fifo = (PortFifo *)&(hPort->portData.QInAddr);
   fifo->QxAddr = buffer;
   fifo->QxSize = size;
   fifo->QxCount = 0;
   fifo->QxGet = 0;
   fifo->QxPut = 0;
   /*initialize output (send) buffer
   //there is no need for output (send) buffer, as we will write directly into the fifo of pair port
   fifo = (PortFifo *)&(hPort->portData.QOutAddr);
   fifo->QxAddr = NULL;
   fifo->QxSize = 0;
   fifo->QxCount = 0;
   fifo->QxGet = 0;
   fifo->QxPut = 0;
   */
}

static __inline void m_FifoFlush(PortInformation * hPort)
{
   //flush input (receive) buffer
   PortFifo * fifo = (PortFifo *)&(hPort->portData.QInAddr);
   fifo->QxCount = 0;
   fifo->QxGet = 0;
   fifo->QxPut = 0;
   /*flush output (send) buffer
   fifo = (PortFifo *)&(hPort->portData.QOutAddr);
   fifo->QxCount = 0;
   fifo->QxGet = 0;
   fifo->QxPut = 0;
   */
}

//return number of written bytes
static __inline DWORD m_FifoWrite(PortInformation * hPort, BYTE * data, DWORD count)
{
   PortFifo * fifo = (PortFifo *)&(hPort->portData.QInAddr);
   DWORD space = fifo->QxSize - fifo->QxCount;
   DWORD written = 0;

   //normaly this should always be false
   if ((int)fifo->QxCount < 0) //it this occurs ... we have a kind of race condition on the fifo ...
   {
      m_FifoFlush(hPort);
      return 0;
   }

   //
   while (count && space)
   {
      fifo->QxAddr[fifo->QxPut++] = *data++;
      if (fifo->QxPut >= fifo->QxSize) //wrap around
      {
         fifo->QxPut = 0;
      }
      written++;
      space--;
      count--;
   }
   if (written) fifo->QxCount += written; //increment by number of written chars
   return written;
}

//return number of read bytes
static __inline DWORD m_FifoRead(PortInformation * hPort, BYTE * buffer, DWORD size)
{
   PortFifo * fifo = (PortFifo *)&(hPort->portData.QInAddr);
   DWORD count = fifo->QxCount;
   DWORD read = 0;

   //normaly this should always be false
   if ((int)fifo->QxCount < 0) //it this occurs ... we have a kind of race condition on the fifo ...
   {
      m_FifoFlush(hPort);
      return 0;
   }

   //
   while (count && size)
   {
      *buffer++ = fifo->QxAddr[fifo->QxGet++];
      if (fifo->QxGet >= fifo->QxSize) //wrap around
      {
         fifo->QxGet = 0;
      }
      count--;
      read++;
      size--;
   }
   if (read) fifo->QxCount -= read; //decrement by number of read chars
   return read;
}

//return number of bytes in RX fifo
static __inline DWORD m_FifoCount(PortInformation * hPort)
{
   PortFifo * fifo = (PortFifo *)&(hPort->portData.QInAddr);
   return fifo->QxCount;
}









/*----------------------------------------------------------------------------
   \brief Initialize port driver.

   This function gets called from the VxDs control function "MXVCP_Control"
   on reception of a system message.
   It registers itself as a port driver with VCOMM. VCOMM will call back the given
   function (m_DriverControl) to complete initialization by initialization
   of the available ports.

   This function is only called once when this VXD is loaded. This VXD is
   loaded as far as any of its COM ports is opened. The opening of further
   COM ports does not lead to execute this function again.
   However if all COM ports of this VXD get closed, the VXD is unloaded
   (cf. MXVCP_DeviceExit). If after that a COM port is opened again,
   the VXD is loaded again and thus, this function is called again.

   The VXD stays loaded, as long as at least one of its COM ports is open!

   \return
      if port(s) could be configured then NC (carry flag clear)
      else CY (carry flag set)
----------------------------------------------------------------------------*/
BOOL _cdecl MXVCP_DeviceInit(HVM vmHandle)
{
#ifdef dbgMsgLen
   dbgMsg[0] = 0; //ensure zero termination
   dbgMsg[dbgMsgLen - 1] = 0; //ensure zero termination
#endif
#if 0
   stdutils_strncpy(dbgMsg, "MXVCP_DeviceInit", dbgMsgLen);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   m_NextFreePort = 0;
   m_SysVmHandle = Get_Sys_VM_Handle(); //save handle
   VCOMM_RegisterPortDriver((PFN)&m_DriverControl); //register driver
   _asm clc; //clear carry
   return 1;
}


/*----------------------------------------------------------------------------
   \brief Unload port driver.

   This function is only called once when the VXD is unloaded. This VXD is
   unloaded as far as all of its COM ports get closed.
   The closing of any COM ports does not lead to execute this function as
   long as any other COM port of this VXD is open.

   \return
      if port(s) could be configured then NC (carry flag clear)
      else CY (carry flag set)
----------------------------------------------------------------------------*/
BOOL _cdecl MXVCP_DeviceExit(HVM vmHandle)
{
#if 0
   stdutils_strncpy(dbgMsg, "MXVCP_DeviceExit", dbgMsgLen);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   m_SysVmHandle = 0;
   _asm clc; //clear carry
   return 1;
}


/*----------------------------------------------------------------------------
   \brief   Called by VCOMM to initialize a port.

   This function is called, whenever a COM port of this VXD is requested to become
   opened. So this function can be called multiple times.

   \note
   This function is called before the m_PortOpen function is called.

   \param fCode      code of function to perform
   \param DevNode    devnode in plug and play scheme
   \param DCRefData  reference data to refer to while calling VCOMM back.
   \param AllocBase  allocated base
   \param AllocIrq   allocated irq
   \param portName   name of port.
----------------------------------------------------------------------------*/
static void _cdecl m_DriverControl(DWORD fCode, DWORD DevNode, DWORD DCRefData,
                                   DWORD AllocBase, DWORD AllocIrq, char *portName)
{
   //initialize driver
   if (fCode == DC_Initialize)
   {
#if 0
      unsigned int len;
      len = stdutils_strncpy(dbgMsg, "m_DriverControl:", dbgMsgLen);
      stdutils_strncpy(&dbgMsg[len], portName, dbgMsgLen - len);
      SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
      PortInformation * port = NULL;
      unsigned int p;

      //check if this port was already opened before ...
      //therefore, search for its name in the available ports ...
      for (p = 0; p < m_NextFreePort; ++p)
      {
         if (stdutils_strncmp(m_PortInformation[p].portName, portName, PORTNAME_LENGTH) == 0)
         {
            //found
            port = &m_PortInformation[p];
            break;
         }
      }

      //if i didn't found the port in the list of available ports, try to "allocated" a new one
      if ((port == NULL) && (m_NextFreePort < NUMBER_OF_PORTS))
      {
         port = &m_PortInformation[m_NextFreePort++];

         //initialize instance
         stdutils_memclr(port, sizeof(PortInformation)); //zero out all data
         port->portData.PDLength = sizeof(PortData);
         port->portData.PDVersion = 0x10A;
         port->portData.PDfunctions = (PortFunctions *)&m_PortFunctionTable;
         port->portData.PDNumFunctions = sizeof(PortFunctionTable) / 4;

         //initialize fifo buffers
         m_FifoInit(port, m_FifoBuffer[m_NextFreePort - 1], FIFO_SIZE_1BY);

         //set port name
         stdutils_strncpy(port->portName, portName, PORTNAME_LENGTH);

         //set pair-port
#if 0 //port shall be linked to itself!
         stdutils_strncpy(port->pairPortName, portName, PORTNAME_LENGTH);
         port->pairPort = port;
#else //port shall be linked to a pair-port, specified by its name in the registry
         {
            char pairPortName[PORTNAME_LENGTH];
            DWORD status;
            DWORD len;
            //read pair port name from register
            len = sizeof(pairPortName);
            status = CONFIGMG_ReadRegistryValue(DevNode, 0, "PairPortName", REG_SZ, pairPortName, &len, 0); //read from hardware branch
            if ((status == 0) && (len > 0))
            {
               //add zero termination ... and copy into data
               pairPortName[len] = 0;
               stdutils_strncpy(port->pairPortName, pairPortName, PORTNAME_LENGTH);
            }
            //link port-instance and port pair instance to each other
            //therefore: find instance of pair port, by name
            for (p = 0; p < (m_NextFreePort - 1); ++p)
            {
               PortInformation * const pairPort = &m_PortInformation[p];
               if (stdutils_strncmp(pairPort->portName, port->pairPortName, PORTNAME_LENGTH) == 0)
               {
                  port->pairPort = pairPort;
                  pairPort->pairPort = port;
                  break;
               }
            }
         }
#endif
      }

      //add port to VCOMM
      if (port != NULL)
      {
         //add port with given name
         VCOMM_AddPort(DCRefData, (PFN)&m_PortOpen, portName);
      }
   }
}



/*----------------------------------------------------------------------------
   \brief Open a port.

   This assumes that VCOMM called here only when it knows that we can support the port.
   It does not validate  parameters because of this assumption.

   \param PortName   Non-null if open port using name
   \param VMId       Id of the caller
   \param lpError    location to update error in

   \return
   PortHandle if successful, else 0
   Updates to *lpError are
   IE_OPEN      port already open
   IE_HARDWARE   hardware error
   IE_DEFAULT   generic error
----------------------------------------------------------------------------*/
static PortInformation * _cdecl m_PortOpen(char *PortName, DWORD VMId, long *lpError)
{
   unsigned int p;
#if 0
   unsigned int len;
   len = stdutils_strncpy(dbgMsg, "m_PortOpen:", dbgMsgLen);
   stdutils_strncpy(&dbgMsg[len], PortName, dbgMsgLen - len);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   //find instance of port, by name
   for (p = 0; p < m_NextFreePort; ++p)
   {
      PortInformation * const port = &m_PortInformation[p];
      if (stdutils_strncmp(port->portName, PortName, PORTNAME_LENGTH) == 0)
      {
         //already open?
         if (port->isOpen)
         {
            //default error
            port->portData.dwLastError = IE_OPEN;
            if (lpError != NULL) *lpError = IE_OPEN;
            return NULL;
         }

         //clear flags and callsbacks
         port->eventMask = 0;
         port->portData.dwDetectedEvents = 0;
         port->eventRegister = &port->portData.dwDetectedEvents; //this is the initial event register
         port->eventCallback = 0;
         port->portData.dwClientRefData = 0;
         port->txCallback = 0;
         port->txCallbackParameter = 0;
         port->txCallbackTriggerLevel = -1;
         port->rxCallback = 0;
         port->rxCallbackParameter = 0;
         port->rxCallbackTriggerLevel = -1;
         port->portData.dwLastReceiveTime = 0;

         //flush fifo
         m_FifoFlush(port);

         //success
         port->portData.dwLastError = 0;
         port->isOpen = 1;

         //issue CTS, DTS event to pair port
         if (port->pairPort && port->pairPort->isOpen)
         {
            DWORD events = (EV_CTS | EV_DSR) & port->pairPort->eventMask;
            *port->pairPort->eventRegister |= (EV_CTS | EV_DSR);
            if (events && port->pairPort->eventCallback)
            {
               events |= (events & EV_CTS) ? EV_CTSS : 0; //set CTS state (if user is interested in CTS events)
               events |= (events & EV_DSR) ? EV_DSRS : 0; //set DSR state (if user is interested in DSR events)
               port->pairPort->eventCallback(port->pairPort, port->pairPort->portData.dwClientRefData, CN_EVENT, events);
            }
         }
         return port;
      }
   }

   //default error
   if (lpError != NULL) *lpError = IE_DEFAULT;
   return NULL;
}



/*----------------------------------------------------------------------------
   \brief Called by the _VCOMM_SetupComm service to set the receive and transmit queues for a port,
   and retrieve the status of previous receive queue.

   \param   hPort    Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   RxQueue  Base address of the receive queue.
   \param   cbRxQueue Size of the receive queue.
   \param   TxQueue  Base address of the transmit queue.
   \param   cbTxQueue Size of the transmit queue.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise

   \note
   Any characters in the receive and transmit queues are discarded.
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortSetup(PortInformation * hPort, void * RxQueue, DWORD cbRxQueue,
                               void * TxQueue, DWORD cbTxQueue)
{
#if 0
   unsigned int len;
   len  = stdutils_strncpy(dbgMsg, "m_PortSetup:", dbgMsgLen);
   len += stdutils_uitoa(&dbgMsg[len], (unsigned long)RxQueue, dbgMsgLen - len);
   dbgMsg[len++] = ':';
   len += stdutils_uitoa(&dbgMsg[len], cbRxQueue, dbgMsgLen - len);
   dbgMsg[len++] = ',';
   len += stdutils_uitoa(&dbgMsg[len], (unsigned long)TxQueue, dbgMsgLen - len);
   dbgMsg[len++] = ':';
   len += stdutils_uitoa(&dbgMsg[len], cbTxQueue, dbgMsgLen - len);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   hPort->portData.dwLastError = 0;
   return 1; //just ignore the given buffer
}


/*----------------------------------------------------------------------------
   \brief Called by the _COMM_CloseComm service to close a port.

   \param   hPort    Adress of a PORTINFORMATION_t structure returned by the PortOpen function.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise

   \note
   If the PortOpen function calls the _VCOMM_Acquire_Port service, the PortClose function musst call
   the _VCOMM_Release_Port service.
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortClose(PortInformation * hPort)
{
#if 0
   stdutils_strncpy(dbgMsg, "m_PortClose", dbgMsgLen);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   //close this port
   hPort->isOpen = 0;
   hPort->eventCallback = 0;
   hPort->txCallback = 0;
   hPort->rxCallback = 0;
   //issue CTS, DTS event to pair port
   if (hPort->pairPort && hPort->pairPort->isOpen)
   {
      DWORD event = (EV_CTS | EV_DSR) & hPort->pairPort->eventMask;
      *hPort->pairPort->eventRegister |= (EV_CTS | EV_DSR);
      if (event && hPort->pairPort->eventCallback)
      {
         hPort->pairPort->eventCallback(hPort->pairPort, hPort->pairPort->portData.dwClientRefData, CN_EVENT, event);
      }
   }
   hPort->portData.dwLastError = 0;
   return 1; //nothing more todo
}



/*----------------------------------------------------------------------------
   \brief Called by the _VCOMM_ReadComm service to read characters from the receive queue for the
   specified port.

   Must be callable at interrupt time.
   The port driver must update the QInGet field of the PORTDATA_t structure. The port driver may need
   to issue flow-control signals if the number of characters in the receive queue drops below the XON limit.
   The XON limit is specified by the device control block (DCB_t structure) for the port.

   \param   hPort          Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   achBuffer      Address of a buffer that receives the characters.
   \param   cchRequested   Number of characters to read from the receive queue
   \param   cchReceived    Address of a 32-bit variable that receives the number of characters actually read.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortRead(PortInformation * hPort, void * achBuffer, DWORD cchRequested, DWORD * cchReceived)
{
#if 0
   unsigned int len;
   len  = stdutils_strncpy(dbgMsg, "m_PortRead:", dbgMsgLen);
   stdutils_uitoa(&dbgMsg[len], cchRequested, dbgMsgLen - len);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   if (hPort->isOpen)
   {
      DWORD fifoCountBefore = m_FifoCount(hPort);
      DWORD received = m_FifoRead(hPort, achBuffer, cchRequested);
      *cchReceived = received;
      //trigger tx events of pair port
      if (received && hPort->pairPort && hPort->pairPort->isOpen)
      {
         DWORD events = EV_TXCHAR; //issue that event,if at least one char is read
         if (m_FifoCount(hPort) <= FIFO_SIZE_1BY/2) //if fillstate of receive fifo of this port is less than 50%...
         {
            events |= EV_TXEMPTY; //...then the tx fifo of the pair port is declared empty!
         }
         *hPort->pairPort->eventRegister |= events;
         events = events & hPort->pairPort->eventMask;
         if (events && hPort->pairPort->eventCallback)
         {
            hPort->pairPort->eventCallback(hPort->pairPort, hPort->pairPort->portData.dwClientRefData,
                                             CN_EVENT, events);
         }
         if (hPort->pairPort->txCallback)
         {
            //tx fifo of pair port is "emulated" by the rx fifo of this port ...
            DWORD fifoCountAfter = m_FifoCount(hPort);
            DWORD txFifoCountBefore = 0;
            DWORD txFifoCountAfter = 0;
            if (fifoCountBefore > FIFO_SIZE_1BY/2)
            {
               txFifoCountBefore = fifoCountBefore - FIFO_SIZE_1BY/2;
            }
            if (fifoCountAfter > FIFO_SIZE_1BY/2)
            {
               txFifoCountAfter = fifoCountAfter - FIFO_SIZE_1BY/2;
            }
            //the fillstate of the "tx fifo" is fallen "below" the threshold (due to this read operation)
            if ((txFifoCountBefore > (DWORD)(hPort->pairPort->txCallbackTriggerLevel)) &&
                (txFifoCountAfter <= (DWORD)(hPort->pairPort->txCallbackTriggerLevel)))
            {
               hPort->pairPort->txCallback(hPort->pairPort, hPort->pairPort->txCallbackParameter,
                                           CN_TRANSMIT, 0);
            }
         }
      }
      hPort->portData.dwLastError = 0;
      return 1; //success
   }
   hPort->portData.dwLastError = IE_NOPEN;
   return 0; //error - port not open
}


/*----------------------------------------------------------------------------
   \brief Called by the _VCOMM_WriteComm service to write characters to the transmit queue for the specified port.

   Must be callable at interrupt time.

   \param   hPort          Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   achBuffer      Address of an array of characters to write.
   \param   cchRequested   Number of characters to write.
   \param   cchWritten     Address of a 32-bit variable that receives the number of characters actually written.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortWrite(PortInformation * hPort, void * achBuffer, DWORD cchRequested, DWORD * cchWritten)
{
#if 0
   unsigned int len;
   len  = stdutils_strncpy(dbgMsg, "m_PortWrite:", dbgMsgLen);
   stdutils_uitoa(&dbgMsg[len], cchRequested, dbgMsgLen - len);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   if (hPort->isOpen)
   {
      DWORD written;
      //pair channel open
      if ((!hPort->pairPort) || (!hPort->pairPort->isOpen))
      {
         //drop all data while pair channel is close
         written = cchRequested;
         *cchWritten = written;
         //trigger tx event callback (if set)
         if (written)
         {
            DWORD events = (EV_TXCHAR | EV_TXEMPTY); //preset TX empty event
            *hPort->eventRegister |= events;
            events = events & hPort->eventMask;
            if (events && hPort->eventCallback)
            {
               hPort->eventCallback(hPort, hPort->portData.dwClientRefData, CN_EVENT, events);
            }
            if (hPort->txCallback)
            {
               DWORD fifoCount = 0; //everything was dropped away. to the tx fifo is empty
               if (fifoCount <= (DWORD)(hPort->txCallbackTriggerLevel))
               {
                  hPort->txCallback(hPort, hPort->txCallbackParameter, CN_TRANSMIT, 0);
               }
            }
         }
      }
      else
      {
         //otherwise: write into pair channels fifo
         written = m_FifoWrite(hPort->pairPort, achBuffer, cchRequested);
         *cchWritten = written;
         //trigger rx events of pair port
         if (written)
         {
            hPort->pairPort->portData.dwLastReceiveTime = System_GetTime();
            *hPort->pairPort->eventRegister |= EV_RXCHAR;
            if (hPort->pairPort->eventCallback)
            {
               if (hPort->pairPort->eventMask & EV_RXCHAR)
               {
                  hPort->pairPort->eventCallback(hPort->pairPort, hPort->pairPort->portData.dwClientRefData,
                                                 CN_EVENT, EV_RXCHAR);
               }
            }
            if (hPort->pairPort->rxCallback)
            {
               DWORD fifoCount = m_FifoCount(hPort->pairPort);
               if (fifoCount >= (DWORD)(hPort->pairPort->rxCallbackTriggerLevel))
               {
                  hPort->pairPort->rxCallback(hPort->pairPort, hPort->pairPort->rxCallbackParameter,
                                              CN_RECEIVE, 0);
               }
            }
         }
      }
      hPort->portData.dwLastError = 0;
      return 1; //success
   }
   hPort->portData.dwLastError = IE_NOPEN;
   return 0; //error - port not open
}


/*----------------------------------------------------------------------------
   \brief Called by the _VCOMM_TransmitCommChar service to send an "immediate" character to a port.
   The port driver must transmit the specified character before any characters in the transmit queue.

   Must be callable at interrupt time.

   \param   hPort   Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   ch      Character to transmit.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortTransmitChar(PortInformation * hPort, DWORD ch)
{
#if 0
   stdutils_strncpy(dbgMsg, "m_PortTransmitChar", dbgMsgLen);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   hPort->portData.dwLastError = IE_DEFAULT;
   return 0; //not supported
}


/*----------------------------------------------------------------------------
   \brief Called by the _VCOMM_PurgeComm service to discard information in the receive or transmit queue,
   and cancel pending reads or writes.

   \param   hPort        Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   dwQueueType  The queue to purge: 0 if transmit queue, 1 if receive queue.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortPurge(PortInformation * hPort, DWORD dwQueueType)
{
#if 0
   stdutils_strncpy(dbgMsg, "m_PortPurge", dbgMsgLen);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   if (hPort->isOpen)
   {
      if (dwQueueType == 1) //receive queue
      {
         m_FifoFlush(hPort);
      }
      hPort->portData.dwLastError = 0;
      return 1; //success (nothing todo for transmit queue)
   }
   hPort->portData.dwLastError = IE_NOPEN;
   return 0; //error - port not open
}


/*----------------------------------------------------------------------------
   \brief Called by _VCOMM_GetCommQueueStatus to retrieve information about the status of a communications
   channel and the amount of data in the receive and transmit queues.

   Must be callable at interrupt time.

   \param   hPort    Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   cmst     Address of a COMSTAT_t structure that receives status information.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortGetQueueStatus(PortInformation * hPort, _COMSTAT * cmst)
{
#if 0
   stdutils_strncpy(dbgMsg, "m_PortGetQueueStatus", dbgMsgLen);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   cmst->BitMask = 0;
   cmst->cbInque = m_FifoCount(hPort);
   cmst->cbOutque = 0;
   hPort->portData.dwLastError = 0;
   return 1;
}


/*----------------------------------------------------------------------------
   \brief Called by the _VCOMM_GetCommEventMask service to retrieve and clear the specified event flags for a port.

   Must be callable at interrupt time.

   \param   hPort    Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   dwMask   One or more values specifying the events to retrieve and clear.
   \param   dwEvents Address of the event variable.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortGetEventMask(PortInformation * hPort, DWORD dwMask, DWORD * dwEvents)
{
   DWORD * eventRegister = hPort->eventRegister;
   DWORD events;
#if 0
   unsigned int len;
   len  = stdutils_strncpy(dbgMsg, "m_PortGetEventMask:", dbgMsgLen);
   len += stdutils_uitoa(&dbgMsg[len], dwMask, dbgMsgLen - len);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   //get events and clear those flagst given by mask (TODO: make atomic sequence)
   events = *eventRegister;
   *eventRegister = (events & ~dwMask);
   //return flags to user
   *dwEvents = (events & dwMask);
   hPort->portData.dwLastError = 0;
   return 1;
}


/*----------------------------------------------------------------------------
   \brief Called by the _VCOMM_SetCommEventMask service to set the value of the event mask and the location of the event variable. .

   \param   hPort    Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   dwMask   The event mask for the communications resource. The event mask is a combination of values
                     specifying enabled events. For a list of events, see _VCOMM_GetCommEventMask. If notification is enabled,
                     an enabled event triggers a call to the notification function.
   \param   dwEvents Address of the event variable. Can be NULL, in which case the location of the event variable is unchanged.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortSetEventMask(PortInformation * hPort, DWORD dwMask, DWORD * dwEvents)
{
#if 0
   unsigned int len;
   len  = stdutils_strncpy(dbgMsg, "m_PortSetEventMask:", dbgMsgLen);
   len += stdutils_uitoa(&dbgMsg[len], dwMask, dbgMsgLen - len);
   dbgMsg[len++] = ':';
   len += stdutils_uitoa(&dbgMsg[len], (DWORD)dwEvents, dbgMsgLen - len);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   hPort->eventMask = dwMask;
   if (dwEvents != NULL)
   {
      hPort->eventRegister = dwEvents;
   }
   hPort->portData.dwLastError = 0;
   return 1;
}


/*----------------------------------------------------------------------------
   \brief Called by the _VCOMM_EnableCommNotification service to register a callback function for event notification.

   \param   hPort          Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   CommNotifyProc Address of a client-defined callback function that the port driver calls when an event
                           enabled by the PortSetEventMask function occurs. For more information, see CommNotifyProc.
                           Can be NULL to disable notification.
   \param   lReferenceData A client-defined 32-bit value that is passed to the callback function.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortEnableNotification(PortInformation * hPort, PCommNotifyProc commNotifyProc, DWORD lReferenceData)
{
#if 0
   unsigned int len;
   len  = stdutils_strncpy(dbgMsg, "m_PortEnableNotification:", dbgMsgLen);
   len += stdutils_uitoa(&dbgMsg[len], (DWORD)commNotifyProc, dbgMsgLen - len);
   dbgMsg[len++] = ':';
   len += stdutils_uitoa(&dbgMsg[len], (DWORD)lReferenceData, dbgMsgLen - len);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   hPort->portData.dwClientRefData = lReferenceData; //YES: The port driver sets this value when the PortEnableNotification function is called.
   hPort->eventCallback = commNotifyProc;
   hPort->portData.dwLastError = 0;
#if 1
   //immediattly trigger "pending" events
   if (hPort->eventCallback)
   {
      if (hPort->eventMask & EV_TXEMPTY)
      {
         hPort->eventCallback(hPort, hPort->portData.dwClientRefData, CN_EVENT, EV_TXEMPTY);
      }
   }
#endif
   return 1;
}



/*----------------------------------------------------------------------------
   \brief Called by the _VCOMM_SetReadCallback service to set the notification threshold for the receive queue
   and register a callback function for receive-queue threshold notification.

   \param   hPort          Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   RxTrigger      Receive-queue notification threshold. When the number of bytes in the receive queue
                           reaches this value, the port driver calls the function specified by CommNotifyProc.
                           Can be -1 to disable receive-queue notification.
   \param   CommNotifyProc Address of the callback function for receive-queue notification. Can be different
                           from the notification function for event notification and transmit-queue notification.
                           For more information, see the CommNotifyProc function.
   \param   lReferenceData A client-defined 32-bit value that is passed to the callback function. Can be different
                           from the reference data specified for event notification and transmit-queue notification.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortSetReadCallback(PortInformation * hPort, long rxTrigger, PCommNotifyProc commNotifyProc,
                                         DWORD lReferenceData)
{
#if 0
   unsigned int len;
   len  = stdutils_strncpy(dbgMsg, "m_PortSetReadCallback:", dbgMsgLen);
   len += stdutils_uitoa(&dbgMsg[len], (DWORD)commNotifyProc, dbgMsgLen - len);
   dbgMsg[len++] = ':';
   len += stdutils_uitoa(&dbgMsg[len], lReferenceData, dbgMsgLen - len);
   dbgMsg[len++] = ':';
   len += stdutils_uitoa(&dbgMsg[len], (DWORD)rxTrigger, dbgMsgLen - len);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   if (rxTrigger > FIFO_SIZE_1BY)
   {
      rxTrigger = FIFO_SIZE_1BY; //limit threshold value
   }
   hPort->rxCallbackTriggerLevel = rxTrigger;
   hPort->rxCallbackParameter = lReferenceData;
   hPort->rxCallback = commNotifyProc;
#if 1
   //immediattly trigger "pending" events
   if (hPort->rxCallback)
   {
      DWORD fifoCount = m_FifoCount(hPort);
      if (fifoCount >= (DWORD)(hPort->rxCallbackTriggerLevel))
      {
         hPort->rxCallback(hPort, hPort->rxCallbackParameter, CN_RECEIVE, 0);
      }
   }
#endif
   hPort->portData.dwLastError = 0;
   return 1;
}


/*----------------------------------------------------------------------------
   \brief Called by the _VCOMM_SetWriteCallback service to set the notification threshold for the transmit queue,
   and register a callback function for transmit-queue threshold notification.

   \param   hPort          Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   TxTrigger      Transmit-queue notification threshold. When the number of bytes in the transmit queue
                           reaches this value, the port driver calls the function specified by CommNotifyProc.
                           Can be -1 to disable transmit-queue notification.
   \param   CommNotifyProc Address of the callback function for transmit-queue notification. Can be different from
                           the notification function for event notification and receive-queue notification.
                           For more information, see the CommNotifyProc function.
   \param   lReferenceData A client-defined 32-bit value that is passed to the callback function. Can be different
                           from the reference data specified for event notification and receive-queue notification.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortSetWriteCallback(PortInformation * hPort, long txTrigger, PCommNotifyProc commNotifyProc,
                                          DWORD lReferenceData)
{
#if 0
   unsigned int len;
   len  = stdutils_strncpy(dbgMsg, "m_PortSetWriteCallback:", dbgMsgLen);
   len += stdutils_uitoa(&dbgMsg[len], (DWORD)commNotifyProc, dbgMsgLen - len);
   dbgMsg[len++] = ':';
   len += stdutils_uitoa(&dbgMsg[len], lReferenceData, dbgMsgLen - len);
   dbgMsg[len++] = ':';
   len += stdutils_uitoa(&dbgMsg[len], (DWORD)txTrigger, dbgMsgLen - len);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   if (txTrigger > (FIFO_SIZE_1BY/2 - 1)) //50% chosen randomly ;-)
   {
      txTrigger = (FIFO_SIZE_1BY/2 - 1); //limit threshold value
   }
   hPort->txCallbackTriggerLevel = txTrigger;
   hPort->txCallbackParameter = lReferenceData;
   hPort->txCallback = commNotifyProc;
#if 1
   //immediattly trigger "pending" events
   if (hPort->txCallback)
   {
      DWORD txFifoCount = 0; //if there is no open pair port, tx fifo is always empty
      if (hPort->pairPort && hPort->pairPort->isOpen)
      {
         //tx fifo of this port is "emulated" by the rx fifo of pair port ...
         DWORD fifoCount = m_FifoCount(hPort->pairPort);
         if (fifoCount > FIFO_SIZE_1BY/2)
         {
            txFifoCount = fifoCount - FIFO_SIZE_1BY/2;
         }
      }
      if (txFifoCount <= (DWORD)(hPort->txCallbackTriggerLevel))
      {
         hPort->txCallback(hPort, hPort->txCallbackParameter, CN_TRANSMIT, 0);
      }
   }
#endif
   hPort->portData.dwLastError = 0;
   return 1;
}


/*----------------------------------------------------------------------------
   \brief Called by the _VCOMM_GetCommProperties service to get the hardware and software capabilities and limitations of a port.

   \param   hPort    Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   cmmp     Address of a COMMPROP_t structure that is filled in.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortGetProperties(PortInformation * hPort, _COMMPROP * cmmp)
{
#if 0
   stdutils_strncpy(dbgMsg, "m_PortGetProperties", dbgMsgLen);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   stdutils_memclr(&cmmp, sizeof(_COMMPROP));
   cmmp->wPacketLength = sizeof(_COMMPROP);
   cmmp->wPacketVersion = 2;
   cmmp->dwServiceMask = SP_SERIALCOMM;
   cmmp->dwMaxBaud = BAUD_USER;
   cmmp->dwProvSubType = PST_RS232;
   // cmmp->dwProvCapabilities = 0FFh;
   // cmmp->dwSettableParams = 007Fh;
   // cmmp->dwSettableBaud = 1000FFFFh;
   // cmmp->wSettableData = 0Fh;
   // cmmp->wSettableStopParity = 01F07h;
   // cmmp->dwCurrentTxQueue = 0;
   cmmp->dwCurrentRxQueue = m_FifoCount(hPort);
   hPort->portData.dwLastError = 0;
   return 1;
}


/*----------------------------------------------------------------------------
   \brief Called by the _VCOMM_GetCommConfig service to get the Win32 device control block (DCB_t) of a port.

   Fills the dcbPort buffer with the Win32 DCB and fills dwSize with the size of the Win32 DCB. If dwSize is less than
   the size of the port's Win32 DCB structure on entry, the function does not copy the DCB into the buffer and
   returns FALSE.

   \param   hPort    Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   dcbPort  Address of a buffer that is filled with the Win32 DCB_t structure.
   \param   dwSize   Address of the size, in bytes, of the dcbPort buffer.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortGetCommConfig(PortInformation * hPort, _DCB * dcbPort, DWORD * dwSize)
{
   BOOL status = 0;
#if 0
   stdutils_strncpy(dbgMsg, "m_PortGetCommConfig", dbgMsgLen);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   if (*dwSize >= sizeof(_DCB))
   {
      status = 1;
      if (dcbPort != 0)
      {
         //say 9600 8n1
         stdutils_memclr(&dcbPort, sizeof(_DCB));
         dcbPort->DCBLength = sizeof(_DCB);
         dcbPort->BaudRate = CBR_9600;
         dcbPort->ByteSize = 8;
      }
   }
   *dwSize = sizeof(_DCB);
   hPort->portData.dwLastError = -1 + status;
   return status;
}


/*----------------------------------------------------------------------------
   \brief Called by the _VCOMM_SetCommConfig service to set the hardware state of a port and the software
   state of the associated port driver.

   Must be callable at interrupt time. Like PortGetCommConfig, on entry dwSize is the size of the dcbPort buffer and
   must be greater than or equal to the size of the port's Win32 DCB.

   \param   hPort    Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   dcbPort  Address of a DCB_t structure containing device control information.
   \param   dwSize   The size, in bytes, of the DCB_t structure.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortSetCommConfig(PortInformation * hPort, _DCB * dcbPort, DWORD * dwSize)
{
#if 0
   stdutils_strncpy(dbgMsg, "m_PortSetCommConfig", dbgMsgLen);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   hPort->portData.dwLastError = 0;
   return 1; //accept everything, but drop it away!
}


/*----------------------------------------------------------------------------
   \brief Called by the _VCOMM_GetCommState service to get the device control block (DCB_t) of a port.

   \param   hPort    Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   dcbPort  Address of a DCB_t structure that is filled in.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortGetCommState(PortInformation * hPort, _DCB * dcbPort)
{
#if 0
   stdutils_strncpy(dbgMsg, "m_PortGetCommState", dbgMsgLen);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   //alway say 9600 8n1
   stdutils_memclr(&dcbPort, sizeof(_DCB));
   dcbPort->DCBLength = sizeof(_DCB);
   dcbPort->BaudRate = CBR_9600;
   dcbPort->ByteSize = 8;
   hPort->portData.dwLastError = 0;
   return 1;
}


/*----------------------------------------------------------------------------
   \brief Called by the _VCOMM_SetCommState service to set the hardware state of a port and the software state
   of the associated port driver.

   Must be callable at interrupt time.

   \param   hPort       Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   dcbPort     Address of a DCB_t structure containing device control information.
   \param   ActionMask  One or more values specifying the communications settings to change. For more information,
                        see the _VCOMM_SetCommState service.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortSetCommState(PortInformation * hPort, _DCB * dcbPort, DWORD ActionMask)
{
#if 0
   stdutils_strncpy(dbgMsg, "m_PortSetCommState", dbgMsgLen);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   hPort->portData.dwLastError = 0;
   return 1; //accept everything, but drop it away!
}


/*----------------------------------------------------------------------------
   \brief Called by the _VCOMM_GetModemStatus service to retrieve the current value of the modem status register.

   Must be callable at interrupt time.

   \param   hPort          Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   dwModemStatus  Address of a 32-bit variable to receive the modem status. For a list of possible values,
                           see the _VCOMM_GetModemStatus service.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortGetModemStatus(PortInformation * hPort, DWORD * dwModemStatus)
{
   DWORD status = 0;
#if 0
   stdutils_strncpy(dbgMsg, "m_PortGetModemStatus", dbgMsgLen);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   if ((hPort->pairPort) && (hPort->pairPort->isOpen))
   {
      //TBD: ignore fifo fillstate - always issue CTS if pair port is open!
      // DWORD fifoCount = m_FifoCount(hPort->pairPort); //get number of bytes in rx buffer of pair channel
      // if (fifoCount < FIFO_SIZE_1BY)
      {
         status |= MS_CTS_ON;
      }
      status |= MS_DSR_ON;
   }
   *dwModemStatus = status;
   hPort->portData.dwLastError = 0;
   return 1;
}


/*----------------------------------------------------------------------------
   \brief Sets the location of the modem status shadow for a port. The port driver updates the byte at this location
   whenever the modem status register changes.

   ACHTUNG:
      Signatur der Funktion is unbekannt. In serfunc.asm sieht es so aus als waere die funktion so:
      BOOL _cdecl m_PortSetModemStatusShadow(PortInformation * hPort, BYTE * MSRShadow)
      also 2. Parameter ist Zeiger auf MSRShadow und EventMask gibt es nicht ... ???

   \param   hPort          Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   dwEventMask    Specifies the new event mask for the port.
   \param   MSRShadow      Address of the modem status shadow.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise

   \note
   When a port is opened, the modem status shadow is the bMSRShadow field of the PORTDATA_t structure.
   VCOMM calls this function when the VCOMM_PM_API_SetMSRShadow protected-mode API is called.
   This enables the 16-bit Windows communications driver to set the modem status shadow to a location
   in the system VM. There is no equivalent VCOMM service.
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortSetModemStatusShadow(PortInformation * hPort, DWORD dwEventMask, BYTE * MSRShadow)
{
#if 0
   stdutils_strncpy(dbgMsg, "m_PortSetModemStatusShadow", dbgMsgLen);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   hPort->portData.dwLastError = IE_DEFAULT;
   return 0; //not supported
}



/*----------------------------------------------------------------------------
   \brief Called by the _VCOMM_ClearCommError service to reenable a port after a communications error.

   Must be callable at interrupt time.

   \param   hPort    Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   cmst     Address of a COMSTAT_t structure that receives information about the state of the
                     communications channel. Can be NULL.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortClearError(PortInformation * hPort, _COMSTAT * cmst)
{
#if 0
   stdutils_strncpy(dbgMsg, "m_PortClearError", dbgMsgLen);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   if (cmst)
   {
      DWORD rxFifoCount = m_FifoCount(hPort);
      DWORD txFifoCount = 0;

      //tx fifo is "emulated" by rx fifo of pair port
      if (hPort->pairPort && hPort->pairPort->isOpen)
      {
         DWORD fifoCount = m_FifoCount(hPort->pairPort);
         if (fifoCount > FIFO_SIZE_1BY/2)
         {
            txFifoCount = fifoCount - FIFO_SIZE_1BY/2;
         }
      }
      //set fifo count
      cmst->BitMask = 0;
      cmst->cbInque = rxFifoCount;
      cmst->cbOutque = txFifoCount;
   }
   hPort->portData.dwLastError = 0;
   return 1;
}


/*----------------------------------------------------------------------------
   \brief Returns a Win32 error.

   \param   hPort    Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   dwError  Error word filled by a function.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortGetWin32Error(PortInformation * hPort, DWORD * dwError)
{
#if 0
   stdutils_strncpy(dbgMsg, "m_PortGetWin32Error", dbgMsgLen);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   *dwError = hPort->portData.dwLastError;
   hPort->portData.dwLastError = 0;
   return 1;
}


/*----------------------------------------------------------------------------
   \brief Called by _VCOMM_EscapeCommFunction to carry out an extended function.

   Must be callable at interrupt time.
   A port driver that is designed specifically to emulate a common port type should return success for extended
   functions that apply to the emulated port type, even if the function does not apply to the hardware used for emulation.
   There are several predefined extended functions. Microsoft reserves the first 200 non-negative extended function
   values (0 through 199).

   \param   hPort    Address of a PORTINFORMATION_t structure returned by the PortOpen function.
   \param   lFunc    Value identifying the extended function to carry out, or DUMMY to perform no action.
                     Common extended functions are defined in VCOMM.INC.
   \param   InData   Function-specific 32-bit parameter.
   \param   OutData  Address to receive function-specific data.

   \retval  TRUE     if successful
   \retval  FALSE    otherwise
----------------------------------------------------------------------------*/
static BOOL _cdecl m_PortEscapeFunction(PortInformation * hPort, DWORD lFunc, DWORD InData, DWORD * OutData)
{
#if 0
   stdutils_strncpy(dbgMsg, "m_PortEscapeFunction", dbgMsgLen);
   SHELL_SendMessage(m_SysVmHandle, NULL, dbgMsg);
#endif
   hPort->portData.dwLastError = 0;
   return 1; //say always success!
}
