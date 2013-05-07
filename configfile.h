#ifndef _CONFIGFILE_H
#define _CONFIGFILE_H

struct configfile_parameters
{
  char *host;
  int  port;
  char *username;
  char *password;
  int  avahi;
  int  initial_channel;
  int  startup_stopped;
  char *audio_dest;
  char *configfile;
#if 0
  /* Not yet implemented */
  char key_0[MAX_CONF_LEN];
  char key_1[MAX_CONF_LEN];
  char key_2[MAX_CONF_LEN];
  char key_3[MAX_CONF_LEN];
  char key_4[MAX_CONF_LEN];
  char key_5[MAX_CONF_LEN];
  char key_6[MAX_CONF_LEN];
  char key_7[MAX_CONF_LEN];
  char key_8[MAX_CONF_LEN];
  char key_9[MAX_CONF_LEN];
  char key_h[MAX_CONF_LEN];
  char key_i[MAX_CONF_LEN];
  char key_q[MAX_CONF_LEN];
  char key_n[MAX_CONF_LEN];
  char key_p[MAX_CONF_LEN];
  char key_u[MAX_CONF_LEN];
  char key_d[MAX_CONF_LEN];
  char key_l[MAX_CONF_LEN];
  char key_r[MAX_CONF_LEN];
  char key_space[MAX_CONF_LEN];
  char key_c[MAX_CONF_LEN];
#endif
};


void parse_args(int argc, char* argv[]);
void dump_settings(void);

#endif
