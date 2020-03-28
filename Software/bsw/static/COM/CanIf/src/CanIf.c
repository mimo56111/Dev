/*******************************************************************************
**                                                                            **
**  Copyright (C) AUTOSarZs olc (2020)		                                  **
**                                                                            **
**  All rights reserved.                                                      **
**                                                                            **
**  This document contains proprietary information belonging to AUTOSarZs     **
**  olc . Passing on and copying of this document, and communication          **
**  of its contents is not permitted without prior written authorization.     **
**                                                                            **
********************************************************************************
**                                                                            **
**  FILENAME     : CanIf.c         			                                      **
**                                                                            **
**  VERSION      : 1.0.0                                                      **
**                                                                            **
**  DATE         : 2020-01-26                                                 **
**                                                                            **
**  VARIANT      : Variant PB                                                 **
**                                                                            **
**  PLATFORM     : TIVA C		                                                  **
**                                                                            **
**  AUTHOR       : AUTOSarZs-DevTeam	                                        **
**                                                                            **
**  VENDOR       : AUTOSarZs OLC	                                            **
**                                                                            **
**                                                                            **
**  DESCRIPTION  : CAN Interface source file                                  **
**                                                                            **
**  SPECIFICATION(S) : Specification of CAN Interface, AUTOSAR Release 4.3.1  **
**                                                                            **
**  MAY BE CHANGED BY USER : no                                               **
**                                                                            **
*******************************************************************************/

#include "CanIf.h"
#include "Det.h"
#include "MemMap.h"
#include "CanIf_Cbk.h"

/*private function IDs*/
#define CANIF_CHECK_DLC_API_ID (0xAA)

//Temp canif config variable.
static CanIf_ModuleStateType CanIf_ModuleState = CANIF_UNINT;

//const CanIf_ConfigType *CanIf_ConfigPtr = &CanIf_ConfigObj;

/*****************************************************************************************/
/*                                   Local Function Definition                           */
/*****************************************************************************************/
/******************************************************************************/
/*
 * Brief               	The received Data Length value is compared with the configured
 * 						Data Length value of the received L-PDU.
 * Param-Name[in]      	pPduCfg: Pointer to configured PDU struct.
 * 						pPduInfo: Pointer to recieved L-PDU from lower layer CanDrv.
 * Return              	Std_ReturnType
 *  */
/******************************************************************************/
#if (CANIF_PRIVATE_DATA_LENGTH_CHECK == STD_ON)
static Std_ReturnType CanIf_CheckDLC(const CanIfRxPduCfgType * const ConfigPdu_Ptr, const PduInfoType *pPduInfo)
{
    Std_ReturnType return_val = E_NOT_OK;

    /*
     * [SWS_CANIF_00026] d CanIf shall accept all received L-PDUs
		(see [SWS_CANIF_00390]) with a Data Length value equal or greater then the
		configured Data Length value (see ECUC_CanIf_00599). c(SRS_Can_01005)
     */
    if (ConfigPdu_Ptr->CanIfRxPduDataLength <= pPduInfo->SduLength)
    {
        /*Check success*/
        return_val = E_OK;
    }

    return return_val;
}
#endif

/******************************************************************************/
/*
 * Brief				Performs Software filtering on recieved Pdus.
 *						it is recommended to offer several search algorithms like
 *						linear search, table search and/or hash search variants to provide the most optimal
 *						solution for most use cases.
 * Param-Name[in]      	pPduCfg: Pointer to configured PDU struct.
 * 						Mailbox: Revcieved Pdu from CanDrv.
 * Return              	index of configured recieve pdu
 *  */
/******************************************************************************/
static sint64 CanIf_SW_Filter(const Can_HwType *Mailbox, const CanIfHrhCfgType * const Hrhcfg_Ptr)
{
    CanIfRxPduCfgType *PduCfg_ptr = CanIf_ConfigObj.CanIfInitCfgRef->CanIfRxPduCfgRef;
    sint64 ret_val = -1;
    Can_IdType temp_CanId = (Mailbox->CanId) & 0x3FFFFFFF;
    // Local variable hold Message type , Extended , standard ...
#if (CANIF_PRIVATE_SOFTWARE_FILTER_TYPE == LINEAR)
    //Configured sw algorithm is Linear
    for (uint64 PduCfg_index = 0; PduCfg_index < RX_CAN_L_PDU_NUM; PduCfg_index++)
    	{
    		//configured recieve pdu has same reference to hrh
    		if (PduCfg_ptr[PduCfg_index].CanIfRxPduHrhIdRef == Hrhcfg_Ptr)
    		{
    			//check mask
    			if ((temp_CanId & PduCfg_ptr[PduCfg_index].CanIfRxPduCanId) ==
                   (temp_CanId & PduCfg_ptr[PduCfg_index].CanIfRxPduCanIdMask))
    			{
    				ret_val = (sint64)PduCfg_index;
    				break;
    			}
    			else
    			{

    			}
    		}
    	}
#endif
    return ret_val;
}

#if (CANIF_SET_BAUDRATE_API == STD_ON)
Std_ReturnType CanIf_SetBaudrate(uint8 ControllerId, uint16 BaudRateConfigID)
{
    static uint8 current_ControllerId = -1;
    uint8 return_val;

#if (CANIF_DEV_ERROR_DETECT == STD_ON) /* DET notifications */

    /*  [SWS_CANIF_00869] d If CanIf_SetBaudrate() is called with invalid ControllerId, 
		CanIf shall report development error code CANIF_E_PARAM_CONTROLLERID
		to the Det_ReportError service of the DET module. c(SRS_BSW_00323)*/

    if (ControllerId > USED_CONTROLLERS_NUMBER)
    {
        Det_ReportError(CANIF_MODULE_ID, CANIF_INSTANCE_ID, CANIF_SETBAUDRATE_API_ID,
                        CANIF_E_PARAM_CONTROLLERID);
        return_val = E_NOT_OK;
    }
    else
    {
    }
#endif
    /*  Reentrant for different ControllerIds. Non reentrant for the same ControllerId.
	*/
    if (ControllerId == current_ControllerId)
    {
        /* E_NOT_OK: Service request not accepted */
        return_val = E_NOT_OK;
    }
    else
    {
        current_ControllerId = ControllerId;

        Can_SetBaudrate(ControllerId, BaudRateConfigID);
        /* E_OK: Service request accepted, setting of (new) baud rate started */
        return_val = E_OK;
    }
    return return_val;
}
#endif

#if (CANIF_PUBLIC_READ_RX_PDU_DATA_API == STD_ON)
static void Canif_CopyData(uint8 *dest, uint8 *Src, uint8 Len)
{
    uint8 counter_index;
    for (counter_index = 0; counter_index < Len; counter_index++)
    {
        dest[counter_index] = Src[counter_index];
    }
}
#endif

/*
	[SWS_CANIF_00415] Within the service CanIf_RxIndication() the CanIf
	routes this indication to the configured upper layer target service(s). c()
	*/
void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr)
{
#if (CANIF_DEV_ERROR_DETECT == STD_ON)
    /*[SWS_CANIF_00416] d If parameter Mailbox->Hoh of CanIf_RxIndication()
	has an invalid value, CanIf shall report development error code
	CANIF_E_PARAM_HOH to the Det_ReportError service of the DET module,
	when CanIf_RxIndication() is called.*/
    if (Mailbox->Hoh > HRH_OBj_NUM)
    {
        Det_ReportError(CANIF_MODULE_ID, CANIF_INSTANCE_ID, CANIF_RX_INDCIATION_API_ID,
                        CANIF_E_PARAM_HOH);
    }
    else
    {
    }

    /*SWS_CANIF_00419] d If parameter PduInfoPtr or Mailbox of
	CanIf_RxIndication() has an invalid value, CanIf shall report development
	error code CANIF_E_PARAM_POINTER to the Det_ReportError service of the DET
	module, when CanIf_RxIndication() is called.
	*/

    if ((NULL_PTR == Mailbox) || (NULL_PTR == PduInfoPtr))
    {
        Det_ReportError(CANIF_MODULE_ID, CANIF_INSTANCE_ID, CANIF_RX_INDCIATION_API_ID,
                        CANIF_E_PARAM_POINTER);
    }
    else
    {
    }
#endif
    static CanIf_PduModeType temp_Mode;
    static sint64 temp_CanIfRxPduindex = -1;
    static uint32 temp_CanifHrhindex = 0;

	/*[SWS_CANIF_00421] If CanIf was not initialized before calling
		CanIf_RxIndication(), CanIf shall not execute Rx indication handling, when
		CanIf_RxIndication(), is called.
	*/
    /*
     * The PduMode of a channel defines its transmit or receive activity.
		Communication direction (transmission and/or reception) of the channel can
		be controlled separately or together by upper layers.
     */

    if (CanIf_ModuleState == CANIF_READY && CanIf_GetPduMode(Mailbox->ControllerId, &temp_Mode) == E_OK )
    {
    	//if the current Pdu is (tx & rx) or (rx only)
    	if (temp_Mode == CANIF_ONLINE || temp_Mode == CANIF_TX_OFFLINE)
    	{
    			// pointer for PBcfg hrh configuration
    		    CanIfHrhCfgType *Hrhcfg_Ptr = CanIf_ConfigObj.CanIfInitCfgRef->CanIfInitHohCfgRef->CanIfHrhCfgRef;
    		    // pointer for PBcfg Rx-pdu configuration
    		    CanIfRxPduCfgType *PduCfg_ptr = CanIf_ConfigObj.CanIfInitCfgRef->CanIfRxPduCfgRef;
    		    // Local variable hold Message type , Extended , standard ...
    		    CanIfHrhRangeRxPduRangeCanIdTypeType temp_canIdType = (Mailbox->CanId) >> 29U;
    		    // Local variable hold Can message id
    		    Can_IdType temp_CanId = (Mailbox->CanId) & 0x3FFFFFFF;

    		    // Search for the hrh , which received this message
    		    for (temp_CanifHrhindex = 0; temp_CanifHrhindex < CANIF_HRH_OBj_NUM; temp_CanifHrhindex++)
    		    {
    		        /*
    		        * search for the hrh ID,controller ID == received Hoh,controller ID, type of can msg
    		        * make sure its configured to be receive mode
    		        */
    		        if ((Hrhcfg_Ptr[temp_CanifHrhindex].CanIfHrhIdSymRef->CanObjectId == Mailbox->Hoh) &&
    		            (Hrhcfg_Ptr[temp_CanifHrhindex].CanIfHrhIdSymRef->CanObjectType == RECEIVE) &&
    		            (Hrhcfg_Ptr[temp_CanifHrhindex].CanIfHrhCanCtrlIdRef->CanIfCtrlCanCtrlRef->CanControllerId == Mailbox->ControllerId) &&
						(Hrhcfg_Ptr[temp_CanifHrhindex].CanIfHrhRangeCfgRef->CanIfHrhRangeRxPduRangeCanIdType == temp_canIdType))
    		        {
    		            /*
    		            * check if the received can id in the range of the hrh
    		            * The type of the received can equal the type configured to recieve in this hrh (extended , standard..)
    		            */
    		            if ((temp_CanId & Hrhcfg_Ptr[temp_CanifHrhindex].CanIfHrhRangeCfgRef->CanIfHrhRangeBaseId) ==
    		                 (temp_CanId & Hrhcfg_Ptr[temp_CanifHrhindex].CanIfHrhRangeCfgRef->CanIfHrhRangeMask))
    		            {
    		                /*
    		                * S/w filter is enabled and pdu is basic
    		                */
    		                if ((Hrhcfg_Ptr[temp_CanifHrhindex].CanIfHrhSoftwareFilter == STD_ON) &&
    		                    (Hrhcfg_Ptr[temp_CanifHrhindex].CanIfHrhIdSymRef->CanHandleType == BASIC))
    		                {
    		                	//perform sw filter and get the index if passed (-1 is invalid)
    		                	temp_CanIfRxPduindex = CanIf_SW_Filter(Mailbox, &Hrhcfg_Ptr[temp_CanifHrhindex]);
    		                	//if recieve pdu exists
    		                	if (temp_CanIfRxPduindex > (sint64)-1)
    		                	{

    		                		 /* [SWS_CANIF_00390] d If CanIf accepts an L-PDU received via
										CanIf_RxIndication() during Software Filtering (see [SWS_CANIF_00389]),
										CanIf shall process the Data Length check afterwards, if configured
										(see ECUC_CanIf_00617). c()
    		                		 */
#if (CANIF_PRIVATE_DATA_LENGTH_CHECK == STD_ON)
    		                		if (CanIf_CheckDLC(&PduCfg_ptr[temp_CanIfRxPduindex], PduInfoPtr) == E_NOT_OK)
    		                		{
    		                	        /*[SWS_CANIF_00168] d If the Data Length Check rejects a received LPDU
    		                		 	CanIf shall report runtime error code
    		                			CANIF_E_INVALID_DATA_LENGTH to the Det_ReportRuntimeError() service
    		                			of the DET module.*/
#if (CANIF_DEV_ERROR_DETECT == STD_ON)
    		                	        Det_ReportError(CANIF_MODULE_ID, CANIF_INSTANCE_ID, CANIF_RX_INDCIATION_API_ID,
    		                	                        CANIF_E_INVALID_DATA_LENGTH);
#endif
    		                		}
    		                		else
    		                		{
    		                			//misra
    		                		}
#endif

#if (CANIF_PUBLIC_READ_RX_PDU_DATA_API == STD_ON)
                        // Call copy_Data
                        //Set flag
#endif
    		                	}
    		                	//Software filter failed
    		                	else
    		                	{
#if (CANIF_DEV_ERROR_DETECT == STD_ON)
    		                	        Det_ReportError(CANIF_MODULE_ID, CANIF_INSTANCE_ID, CANIF_RX_INDCIATION_API_ID,
														CANIF_E_PARAM_CANID);
#endif
    		                	}
    		                }
    		                //not basic or sw filter is disabled
    		                else
    		                {
    		                	//search for the configured pdu
    		                	for (uint64 PduCfg_index = 0; PduCfg_index < RX_CAN_L_PDU_NUM; PduCfg_index++)
    		                	    {
    		                	    	//configured recieve pdu has same reference to hrh
    		                	    	if (PduCfg_ptr[PduCfg_index].CanIfRxPduHrhIdRef == Hrhcfg_Ptr)
    		                	    	{
    		                	    		//check mask
    		                	    		if ((temp_CanId & PduCfg_ptr[PduCfg_index].CanIfRxPduCanId) ==
    		                	                   (temp_CanId & PduCfg_ptr[PduCfg_index].CanIfRxPduCanIdMask))
    		                	    		{
    		                	    			temp_CanIfRxPduindex = (sint64)PduCfg_index;
    		                	   				break;
    		                	   			}
    		                	   			else
    		                	   			{
    		                	   				//misra
    		                	    		}
    		                	    	}
    		                	    }
    		                	//if pdu is not found
    		                	if(temp_CanIfRxPduindex == (sint64)-1)
    		                	{
#if (CANIF_DEV_ERROR_DETECT == STD_ON)
    		                	        Det_ReportError(CANIF_MODULE_ID, CANIF_INSTANCE_ID, CANIF_RX_INDCIATION_API_ID,
														CANIF_E_PARAM_CANID);
#endif
    		                	}

    		                	//Get UL user
    	                        switch (PduCfg_ptr[temp_CanIfRxPduindex].CanIfRxPduUserRxIndicationUL)
    	                        {
    	                        case CAN_NM_RX_INDICATION:
    	                            /* code */
    	                            break;

    	                        case CAN_TP_RX_INDICATION:
    	                            /* code */
    	                            break;

    	                        case CAN_TSYN_RX_INDICATION:
    	                            /* code */
    	                            break;

    	                        case CDD_RX_INDICATION:
    	                            /* code */
    	                            break;

    	                        case J1939NM_RX_INDICATION:
    	                            /* code */
    	                            break;

    	                        case J1939TP_RX_INDICATION:
    	                            /* code */
    	                            break;

    	                        case PDUR_RX_INDICATION:
    	                            /* code */
    	                            break;

    	                        case XCP_RX_INDICATION:
    	                            /* code */
    	                            break;

    	                        default:
    	                            break;
    	                        }
    		                }
    		            }
    		            else
    		            {
    		                /*[SWS_CANIF_00416] d If parameter Mailbox->Hoh of CanIf_RxIndication()
							has an invalid value, CanIf shall report development error code
							CANIF_E_PARAM_HOH to the Det_ReportError service of the DET module,
						when CanIf_RxIndication() is called. c(SRS_BSW_00323)
    		                */
#if (CANIF_DEV_ERROR_DETECT == STD_ON) /* DET notifications */
    		                Det_ReportError(CANIF_MODULE_ID, CANIF_INSTANCE_ID, CANIF_RX_INDCIATION_API_ID,
											CANIF_E_PARAM_HOH);
#endif
    		            }

                    }
                    else
                    {
                        //misra
                    }
                }
    		    //if no HRH is found
    		    if(temp_CanifHrhindex == 0)
    		    {
#if (CANIF_DEV_ERROR_DETECT == STD_ON) /* DET notifications */
    		                Det_ReportError(CANIF_MODULE_ID, CANIF_INSTANCE_ID, CANIF_RX_INDCIATION_API_ID,
											CANIF_E_PARAM_HOH);
#endif
    		    }

            }
    	else
    	{
    		/*[SWS_CANIF_00421] d If CanIf was not initialized before calling
    		CanIf_RxIndication(), CanIf shall not execute Rx indication handling, when
    		CanIf_RxIndication(), is called. c()*/
    	}
    }
}
