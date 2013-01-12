/*

pidvbip - tvheadend client for the Raspberry Pi

(C) Dave Chapman 2012-2013

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <libcec/cecc.h>

#define CEC_CONFIG_VERSION CEC_CLIENT_VERSION_CURRENT

struct cec_key_queue {
  char keys[10];
  int numkeys;
  pthread_mutex_t mutex;
};

/* Global variables to hold the libcec status */
static libcec_configuration cec_config;
static ICECCallbacks        cec_callbacks;
static struct cec_key_queue key_queue;

static int CecLogMessage(void *(cbParam), const cec_log_message message)
{
  //fprintf(stderr,"LOG (%d): %s\n",message.level,message.message);

  return 0;
}

char cec_get_keypress(void)
{
  int res;

  pthread_mutex_lock(&key_queue.mutex);

  if (key_queue.numkeys == 0) {
    res = -1;
  } else {
    res = key_queue.keys[0];
    key_queue.numkeys--;
    if (key_queue.numkeys > 0) {
      memmove(key_queue.keys,key_queue.keys+1,key_queue.numkeys);
    }
  }

  pthread_mutex_unlock(&key_queue.mutex);

  return res;
}

static void cec_add_keypress(char key)
{
  pthread_mutex_lock(&key_queue.mutex);

  if (key_queue.numkeys < sizeof(key_queue.keys)) {
    fprintf(stderr,"[CEC] Queuing keypress '%c', numkeys=%d\n",key,key_queue.numkeys);
    key_queue.keys[key_queue.numkeys++]=key;
  }

  pthread_mutex_unlock(&key_queue.mutex);
}

static int CecKeyPress(void *(cbParam), const cec_keypress (key))
{
  fprintf(stderr,"Key: %d, duration: %d\n",key.keycode,key.duration);

  if (key.duration == 0) { // Key down event
    switch (key.keycode) {
      case CEC_USER_CONTROL_CODE_SELECT:
      case CEC_USER_CONTROL_CODE_DISPLAY_INFORMATION:
        cec_add_keypress('i'); break;
      case CEC_USER_CONTROL_CODE_CHANNEL_UP:
        cec_add_keypress('n'); break;
      case CEC_USER_CONTROL_CODE_CHANNEL_DOWN:
        cec_add_keypress('p'); break;
      case CEC_USER_CONTROL_CODE_NUMBER0:
        cec_add_keypress('0'); break;
      case CEC_USER_CONTROL_CODE_NUMBER1:
        cec_add_keypress('1'); break;
      case CEC_USER_CONTROL_CODE_NUMBER2:
        cec_add_keypress('2'); break;
      case CEC_USER_CONTROL_CODE_NUMBER3:
        cec_add_keypress('3'); break;
      case CEC_USER_CONTROL_CODE_NUMBER4:
        cec_add_keypress('4'); break;
      case CEC_USER_CONTROL_CODE_NUMBER5:
        cec_add_keypress('5'); break;
      case CEC_USER_CONTROL_CODE_NUMBER6:
        cec_add_keypress('6'); break;
      case CEC_USER_CONTROL_CODE_NUMBER7:
        cec_add_keypress('7'); break;
      case CEC_USER_CONTROL_CODE_NUMBER8:
        cec_add_keypress('8'); break;
      case CEC_USER_CONTROL_CODE_NUMBER9:
        cec_add_keypress('9'); break;
      case CEC_USER_CONTROL_CODE_UP:
        cec_add_keypress('u'); break;
      case CEC_USER_CONTROL_CODE_DOWN:
        cec_add_keypress('d'); break;
      case CEC_USER_CONTROL_CODE_LEFT:
        cec_add_keypress('l'); break;
      case CEC_USER_CONTROL_CODE_RIGHT:
        cec_add_keypress('r'); break;
      case CEC_USER_CONTROL_CODE_PAUSE:
      case CEC_USER_CONTROL_CODE_F2_RED:
        cec_add_keypress(' '); break;
      default:
        break;
    }
  }
  return 0;
}

static int CecCommand(void *(cbParam), const cec_command (command))
{
  switch (command.opcode) {
    case CEC_OPCODE_STANDBY:
      fprintf(stderr,"<Standby> command received\n");
      cec_add_keypress('q');
      break;
    default:
      fprintf(stderr,"Cmd: initiator=%d, destination=%d, opcode=0x%02x\n",command.initiator,command.destination,command.opcode);
      break;
  }
  return 0;
}

static void CecSourceActivated(void *(cbParam),const cec_logical_address logical_address, const uint8_t activated)
{
  fprintf(stderr,"[CEC] Source %sactivated\n",(activated ? "" : "de"));
}

static int CecAlert(void *(cbParam), const libcec_alert type, const libcec_parameter (param))
{
  fprintf(stderr,"CecAlert - type=%d\n",type);
  switch (type)
    {
    case CEC_ALERT_CONNECTION_LOST:
      printf("Connection lost - exiting\n");
      exit(1);
      break;
    default:
      break;
    }
  return 0;
}

static void clear_cec_logical_addresses(cec_logical_addresses* addresses)
{
  int i;

  addresses->primary = CECDEVICE_UNREGISTERED;
  for (i = 0; i < 16; i++)
    addresses->addresses[i] = 0;
}

static void set_cec_logical_address(cec_logical_addresses* addresses, cec_logical_address address)
{
  if (addresses->primary == CECDEVICE_UNREGISTERED)
    addresses->primary = address;

  addresses->addresses[(int) address] = 1;
}

static void clear_config(libcec_configuration* config)
{
  int i;

  config->iPhysicalAddress =                CEC_PHYSICAL_ADDRESS_TV;
  config->baseDevice = (cec_logical_address)CEC_DEFAULT_BASE_DEVICE;
  config->iHDMIPort =                       CEC_DEFAULT_HDMI_PORT;
  config->tvVendor =              (uint64_t)CEC_VENDOR_UNKNOWN;
  config->clientVersion =         (uint32_t)CEC_CLIENT_VERSION_CURRENT;
  config->serverVersion =         (uint32_t)CEC_SERVER_VERSION_CURRENT;
  config->bAutodetectAddress =              0;
  config->bGetSettingsFromROM =             CEC_DEFAULT_SETTING_GET_SETTINGS_FROM_ROM;
  config->bUseTVMenuLanguage =              CEC_DEFAULT_SETTING_USE_TV_MENU_LANGUAGE;
  config->bActivateSource =                 CEC_DEFAULT_SETTING_ACTIVATE_SOURCE;
  config->bPowerOffScreensaver =            CEC_DEFAULT_SETTING_POWER_OFF_SCREENSAVER;
  config->bPowerOffOnStandby =              CEC_DEFAULT_SETTING_POWER_OFF_ON_STANDBY;
  config->bShutdownOnStandby =              CEC_DEFAULT_SETTING_SHUTDOWN_ON_STANDBY;
  config->bSendInactiveSource =             CEC_DEFAULT_SETTING_SEND_INACTIVE_SOURCE;
  config->iFirmwareVersion =                CEC_FW_VERSION_UNKNOWN;
  config->bPowerOffDevicesOnStandby =       CEC_DEFAULT_SETTING_POWER_OFF_DEVICES_STANDBY;
  memcpy(config->strDeviceLanguage,         CEC_DEFAULT_DEVICE_LANGUAGE, 3);
  config->iFirmwareBuildDate =              CEC_FW_BUILD_UNKNOWN;
  config->bMonitorOnly =                    0;
  config->cecVersion =         (cec_version)CEC_DEFAULT_SETTING_CEC_VERSION;
  config->adapterType =                     ADAPTERTYPE_UNKNOWN;
  config->iDoubleTapTimeoutMs =             CEC_DOUBLE_TAP_TIMEOUT_MS;
  config->comboKey =                        CEC_USER_CONTROL_CODE_STOP;
  config->iComboKeyTimeoutMs =              CEC_DEFAULT_COMBO_TIMEOUT_MS;

  memset(config->strDeviceName, 0, 13);

  /* deviceTypes.Clear(); */
  for (i = 0; i < 5; i++)
    config->deviceTypes.types[i] = CEC_DEVICE_TYPE_RESERVED;

  clear_cec_logical_addresses(&config->logicalAddresses);
  clear_cec_logical_addresses(&config->wakeDevices);
  clear_cec_logical_addresses(&config->powerOffDevices);

#if CEC_DEFAULT_SETTING_POWER_OFF_SHUTDOWN == 1
  set_cec_logical_address(&config->powerOffDevices,CECDEVICE_BROADCAST);
#endif

#if CEC_DEFAULT_SETTING_ACTIVATE_SOURCE == 1
  set_cec_logical_address(&config->wakeDevices,CECDEVICE_TV);
#endif

  config->callbackParam = NULL;
  config->callbacks     = NULL;
}

static void clear_callbacks(ICECCallbacks* cec_callbacks)
{
  cec_callbacks->CBCecLogMessage           = NULL;
  cec_callbacks->CBCecKeyPress             = NULL;
  cec_callbacks->CBCecCommand              = NULL;
  cec_callbacks->CBCecConfigurationChanged = NULL;
  cec_callbacks->CBCecAlert                = NULL;
  cec_callbacks->CBCecMenuStateChanged     = NULL;
  cec_callbacks->CBCecSourceActivated      = NULL;
}

int cec_init(int init_video)
{
  int res;

  /* Set the CEC configuration */
  clear_config(&cec_config);
  snprintf(cec_config.strDeviceName, 13, "pidvbip");
  cec_config.clientVersion       = CEC_CONFIG_VERSION;
  cec_config.bActivateSource     = 0;

  /* Say we are a recording/tuner/playback device */
  cec_config.deviceTypes.types[0] = CEC_DEVICE_TYPE_RECORDING_DEVICE;

  /* Set the callbacks */
  clear_callbacks(&cec_callbacks);
  cec_callbacks.CBCecLogMessage  = &CecLogMessage;
  cec_callbacks.CBCecKeyPress    = &CecKeyPress;
  cec_callbacks.CBCecCommand     = &CecCommand;
  cec_callbacks.CBCecAlert       = &CecAlert;
  cec_callbacks.CBCecSourceActivated= &CecSourceActivated;

  cec_config.callbacks           = &cec_callbacks;

  /* Initialise the library */
  if (!cec_initialise(&cec_config)) {
    fprintf(stderr,"Error initialising libcec, aborting\n");
    return 1;
  }

  if (init_video) {
    /* init video on targets that need this */
    cec_init_video_standalone();
  }

  /* Locate CEC device */
  cec_adapter devices[10];
  int nadapters = cec_find_adapters(devices, 10, NULL);

  if (nadapters <= 0) {
    fprintf(stderr,"Error, no CEC adapters found.\n");
    cec_destroy();
    return 2;
  }

  if (nadapters > 1) {
    fprintf(stderr,"WARNING: %d adapters found, using first.\n",nadapters);
  }

  fprintf(stderr,"Using CEC adapter \"%s\", path=\"%s\"\n",devices[0].comm,devices[0].path);

  /* Open device with a 10000ms (10s) timeout */
  if (!cec_open(devices[0].comm, CEC_DEFAULT_CONNECT_TIMEOUT)) {
    fprintf(stderr,"Error, cannot open device %s\n",devices[0].comm);
    cec_destroy();
    return 3;
  }

  /* Enable callbacks, first parameter is the callback data passed to every callback */
  cec_enable_callbacks(NULL, &cec_callbacks);

  /* Get the menu language of the TV */
  cec_menu_language language;
  cec_get_device_menu_language(CEC_DEFAULT_BASE_DEVICE, &language);
  fprintf(stderr,"TV menu language: \"%c%c%c\"\n",language.language[0],language.language[1],language.language[2]);

  /* Get the power status of the TV */
  cec_power_status power_status = cec_get_device_power_status(CEC_DEFAULT_BASE_DEVICE);
  fprintf(stderr,"TV Power Status:  %d\n",power_status);

  /* Select ourselves as the source - this will also power-on the TV if needed */
  fprintf(stderr,"Setting ourselves as the source\n");
  cec_set_active_source(CEC_DEVICE_TYPE_RECORDING_DEVICE);

  /* Clear the keypress queue */
  key_queue.numkeys = 0;
  pthread_mutex_init(&key_queue.mutex,NULL);

  return 0;
}

int cec_done(int poweroff)
{
  if (poweroff) {
    /* Power-off the TV */
    cec_standby_devices(CEC_DEFAULT_BASE_DEVICE);
  }

  /* Cleanup */
  cec_destroy();

  return 0;
}
