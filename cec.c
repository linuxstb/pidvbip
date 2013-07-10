/*
 *  CEC code taken from Showtime:
 *
 *
 *  Showtime mediacenter
 *  Copyright (C) 2007-2012 Andreas Öman
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <pthread.h>
#include <bcm_host.h>
#include <IL/OMX_Core.h>
#include <interface/vmcs_host/vc_cecservice.h>
#include <interface/vchiq_arm/vchiq_if.h>

#include "cec.h"

int display_status = 0;

#define DISPLAY_STATUS_OFF             0
#define DISPLAY_STATUS_ON              1
#define DISPLAY_STATUS_ON_NOT_VISIBLE  2

#if 0
/* TODO: Figure this one out */
      case CEC_USER_CONTROL_CODE_AN_CHANNELS_LIST:
        msgqueue_add(msgqueue,'c'); break;
#endif

const static int btn_to_action[256] = {
  [CEC_User_Control_Select]             = 'i',
  [CEC_User_Control_DisplayInformation] = 'i',
  [CEC_User_Control_Left]               = 'l',
  [CEC_User_Control_Up]                 = 'u',
  [CEC_User_Control_Right]              = 'r',
  [CEC_User_Control_Down]               = 'd',
  [CEC_User_Control_ChannelUp]          = 'n',
  [CEC_User_Control_ChannelDown]        = 'p',
  [CEC_User_Control_SoundSelect]        = 'a',

  [CEC_User_Control_Pause]              = ' ',
  [CEC_User_Control_F2Red]              = ' ',

  [CEC_User_Control_Rewind]             = 'l',
  [CEC_User_Control_FastForward]        = 'r',
  [CEC_User_Control_Number0]            = '0',
  [CEC_User_Control_Number1]            = '1',
  [CEC_User_Control_Number2]            = '2',
  [CEC_User_Control_Number3]            = '3',
  [CEC_User_Control_Number4]            = '4',
  [CEC_User_Control_Number5]            = '5',
  [CEC_User_Control_Number6]            = '6',
  [CEC_User_Control_Number7]            = '7',
  [CEC_User_Control_Number8]            = '8',
  [CEC_User_Control_Number9]            = '9',
  
  [CEC_User_Control_Exit]               = 'e',
  [CEC_User_Control_EPG]                = 'c',
};


const uint32_t myVendorId = CEC_VENDOR_ID_BROADCOM;
uint16_t physical_address;
CEC_AllDevices_T logical_address;



static void
SetStreamPath(const VC_CEC_MESSAGE_T *msg)
{
    uint16_t requestedAddress;

    requestedAddress = (msg->payload[1] << 8) + msg->payload[2];
    if (requestedAddress != physical_address)
        return;
    vc_cec_send_ActiveSource(physical_address, VC_FALSE);
}


static void
give_device_power_status(const VC_CEC_MESSAGE_T *msg)
{
    // Send CEC_Opcode_ReportPowerStatus
    uint8_t response[2];
    response[0] = CEC_Opcode_ReportPowerStatus;
    response[1] = CEC_POWER_STATUS_ON;
    vc_cec_send_message(msg->initiator, response, 2, VC_TRUE);
}


static void
give_device_vendor_id(const VC_CEC_MESSAGE_T *msg)
 {
  uint8_t response[4];
  response[0] = CEC_Opcode_DeviceVendorID;
  response[1] = (uint8_t) ((myVendorId >> 16) & 0xff);
  response[2] = (uint8_t) ((myVendorId >> 8) & 0xff);
  response[3] = (uint8_t) ((myVendorId >> 0) & 0xff);
  vc_cec_send_message(msg->initiator, response, 4, VC_TRUE);
}


static void
send_cec_version(const VC_CEC_MESSAGE_T *msg)
 {
  uint8_t response[2];
  response[0] = CEC_Opcode_CECVersion;
  response[1] = 0x5;
  vc_cec_send_message(msg->initiator, response, 2, VC_TRUE);
}


static void
vc_cec_report_physicalAddress(uint8_t dest)
{
    uint8_t msg[4];
    msg[0] = CEC_Opcode_ReportPhysicalAddress;
    msg[1] = (uint8_t) ((physical_address) >> 8 & 0xff);
    msg[2] = (uint8_t) ((physical_address) >> 0 & 0xff);
    msg[3] = CEC_DeviceType_Tuner;
    vc_cec_send_message(CEC_BROADCAST_ADDR, msg, 4, VC_TRUE);
}

static void
send_deck_status(const VC_CEC_MESSAGE_T *msg)
{
  uint8_t response[2];
  response[0] = CEC_Opcode_DeckStatus;
  response[1] = CEC_DECK_INFO_NO_MEDIA;
  vc_cec_send_message(msg->initiator, response, 2, VC_TRUE);
}


static void
send_osd_name(const VC_CEC_MESSAGE_T *msg, const char *name)
{
  uint8_t response[15];
  int l = strlen(name);
  if (l > 14) l = 14;
  response[0] = CEC_Opcode_SetOSDName;
  memcpy(response + 1, name, l);
  vc_cec_send_message(msg->initiator, response, l+1, VC_TRUE);
}


static void
cec_callback(void *callback_data, uint32_t param0, uint32_t param1,
	     uint32_t param2, uint32_t param3, uint32_t param4)
{
  VC_CEC_NOTIFY_T reason  = (VC_CEC_NOTIFY_T) CEC_CB_REASON(param0);
  struct msgqueue_t *msgqueue = callback_data;

#if 0
  uint32_t len     = CEC_CB_MSG_LENGTH(param0);
  uint32_t retval  = CEC_CB_RC(param0);
  printf("cec_callback: debug: "
	 "reason=0x%04x, len=0x%02x, retval=0x%02x, "
	 "param1=0x%08x, param2=0x%08x, param3=0x%08x, param4=0x%08x\n",
	 reason, len, retval, param1, param2, param3, param4);
#endif

  VC_CEC_MESSAGE_T msg;
  CEC_OPCODE_T opcode;
  if(vc_cec_param2message(param0, param1, param2, param3, param4, &msg))
    return;


  switch(reason) {
  default:
    break;
  case VC_CEC_BUTTON_PRESSED:
    fprintf(stderr,"[CEC] - 0x%02x button pressed\n",msg.payload[1]);
    msgqueue_add(msgqueue,btn_to_action[msg.payload[1]]);
    break;


  case VC_CEC_RX:

    opcode = CEC_CB_OPCODE(param1);
#if 1
    printf("opcode = %x (from:0x%x to:0x%x)\n", opcode,
	   CEC_CB_INITIATOR(param1), CEC_CB_FOLLOWER(param1));
#endif
    switch(opcode) {
    case CEC_Opcode_GiveDevicePowerStatus:
      give_device_power_status(&msg);
      break;

    case CEC_Opcode_GiveDeviceVendorID:
      give_device_vendor_id(&msg);
      break;

    case CEC_Opcode_SetStreamPath:
      SetStreamPath(&msg);
      break;

    case CEC_Opcode_GivePhysicalAddress:
      vc_cec_report_physicalAddress(msg.initiator);
      break;

    case CEC_Opcode_GiveOSDName:
      send_osd_name(&msg, "pidvbip");
      break;

    case CEC_Opcode_GetCECVersion:
      send_cec_version(&msg);
      break;

    case CEC_Opcode_GiveDeckStatus:
      send_deck_status(&msg);
      break;

    default:
      //      printf("\nDon't know how to handle status code 0x%x\n\n", opcode);
      vc_cec_send_FeatureAbort(msg.initiator, opcode,
			       CEC_Abort_Reason_Unrecognised_Opcode);
      break;
    }
    break;
  }
}


/**
 *
 */
static void
tv_service_callback(void *callback_data, uint32_t reason,
		    uint32_t param1, uint32_t param2)
{
  struct msgqueue_t *msgqueue = callback_data;

  fprintf(stderr,"[CEC] tv_service_callback - reason=0x%08x\n",reason);

  if(reason & 1) {
    display_status = DISPLAY_STATUS_OFF;
  } else {
    display_status = DISPLAY_STATUS_ON;
  }
}


/**
 * We deal with CEC and HDMI events, etc here
 */
static void *
cec_thread(void *aux)
{
  TV_DISPLAY_STATE_T state;
  struct msgqueue_t *msgqueue = aux;

  vc_tv_register_callback(tv_service_callback, msgqueue);
  vc_tv_get_display_state(&state);

  vc_cec_set_passive(1);

  vc_cec_register_callback(((CECSERVICE_CALLBACK_T) cec_callback), msgqueue);
  vc_cec_register_all();

 restart:
  while(1) {
    if(!vc_cec_get_physical_address(&physical_address) &&
       physical_address == 0xffff) {
    } else {
      fprintf(stderr,"[CEC]: Got physical address 0x%04x\n", physical_address);
      break;
    }
    
    sleep(1);
  }


  const int addresses = 
    (1 << CEC_AllDevices_eRec1) |
    (1 << CEC_AllDevices_eRec2) |
    (1 << CEC_AllDevices_eRec3) |
    (1 << CEC_AllDevices_eFreeUse);

  for(logical_address = 0; logical_address < 15; logical_address++) {
    if(((1 << logical_address) & addresses) == 0)
      continue;
    if(vc_cec_poll_address(CEC_AllDevices_eRec1) > 0)
      break;
  }

  if(logical_address == 15) {
    printf("Unable to find a free logical address, retrying\n");
    sleep(1);
    goto restart;
  }

  vc_cec_set_logical_address(logical_address, CEC_DeviceType_Rec, myVendorId);

  while(1) {
    sleep(1);
  }

  vc_cec_set_logical_address(0xd, CEC_DeviceType_Rec, myVendorId);
  return NULL;
}



int cec_init(struct msgqueue_t* msgqueue)
{
  static pthread_t thread;

  pthread_create(&thread,NULL,(void * (*)(void *))cec_thread,msgqueue);
}

int cec_done(int poweroff)
{
  /* TODO */
#if 0
  if (poweroff) {
    /* Power-off the TV */
    cec_standby_devices(CEC_DEFAULT_BASE_DEVICE);
  }

  /* Cleanup */
  cec_destroy();
#endif

  return 0;
}
