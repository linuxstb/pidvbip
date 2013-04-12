/*

pidvbip - tvheadend client for the Raspberry Pi

(C) Dave Chapman 2012-2013
(C) Andy Brown 2013

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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <configfile.h>

char *
trim (char * s)
{
  /* Initialize start, end pointers */
  char *s1 = s, *s2 = &s[strlen (s) - 1];

  /* Trim and delimit right side */
  while ( (isspace (*s2)) && (s2 >= s1) )
    s2--;
  *(s2+1) = '\0';

  /* Trim left side */
  while ( (isspace (*s1)) && (s1 < s2) )
    s1++;

  /* Memmove (safer) finished string */
  memmove (s, s1, strlen(s1) + 1);
  return s;
};

void
parse_config (struct configfile_parameters * parms)
{
  char *s, buff[256];
  FILE *fp = fopen ("pidvbip.conf", "r");
  if (fp == NULL) {
    FILE *fp = fopen ("/flash/pidvbip.conf", "r");
    if (fp == NULL) {
      FILE *fp = fopen ("$HOME/.pidvbip", "r");
      if (fp == NULL) {
        return;
      };
    };
  };

  /* Read next line */
  while ((s = fgets (buff, sizeof buff, fp)) != NULL)
  {
    /* Skip blank lines and comments */
    if (buff[0] == '\n' || buff[0] == '#')
      continue;

    /* Parse name/value pair from line */
    char name[MAX_CONF_LEN], value[MAX_CONF_LEN];
    s = strtok (buff, "=");
    if (s==NULL)
      continue;
    else
      strncpy (name, s, MAX_CONF_LEN);
    s = strtok (NULL, "=");
    if (s==NULL)
      continue;
    else
      strncpy (value, s, MAX_CONF_LEN);
    trim (value);

   /* Copy into correct entry in parameters struct */
    if (strcmp(name, "host")==0)
      strncpy (parms->host, value, MAX_CONF_LEN);
    else if (strcmp(name, "port")==0)
      parms->port = atoi(value);
    else if (strcmp(name, "username")==0)
      strncpy (parms->username, value, MAX_CONF_LEN);
    else if (strcmp(name, "password")==0)
      strncpy (parms->password, value, MAX_CONF_LEN);
    else if (strcmp(name, "initial_channel")==0)
      parms->initial_channel = atoi(value);
    else if (strcmp(name, "startup_streaming")==0)
      parms->startup_streaming = atoi(value);
/* TODO future add keycode mappings */
    else if (strcmp(name, "key0")==0)
      strncpy (parms->key_0, value, MAX_CONF_LEN);
    else if (strcmp(name, "key1")==0)
      strncpy (parms->key_1, value, MAX_CONF_LEN);
    else if (strcmp(name, "key2")==0)
      strncpy (parms->key_2, value, MAX_CONF_LEN);
    else if (strcmp(name, "key3")==0)
      strncpy (parms->key_3, value, MAX_CONF_LEN);
    else if (strcmp(name, "key4")==0)
      strncpy (parms->key_4, value, MAX_CONF_LEN);
    else if (strcmp(name, "key5")==0)
      strncpy (parms->key_5, value, MAX_CONF_LEN);
    else if (strcmp(name, "key6")==0)
      strncpy (parms->key_6, value, MAX_CONF_LEN);
    else if (strcmp(name, "key7")==0)
      strncpy (parms->key_7, value, MAX_CONF_LEN);
    else if (strcmp(name, "key8")==0)
      strncpy (parms->key_8, value, MAX_CONF_LEN);
    else if (strcmp(name, "key9")==0)
      strncpy (parms->key_9, value, MAX_CONF_LEN);
    else if (strcmp(name, "keyh")==0)
      strncpy (parms->key_h, value, MAX_CONF_LEN);
    else if (strcmp(name, "keyi")==0)
      strncpy (parms->key_i, value, MAX_CONF_LEN);
    else if (strcmp(name, "keyq")==0)
      strncpy (parms->key_q, value, MAX_CONF_LEN);
    else if (strcmp(name, "keyn")==0)
      strncpy (parms->key_n, value, MAX_CONF_LEN);
    else if (strcmp(name, "keyp")==0)
      strncpy (parms->key_p, value, MAX_CONF_LEN);
    else if (strcmp(name, "keyu")==0)
      strncpy (parms->key_u, value, MAX_CONF_LEN);
    else if (strcmp(name, "keyd")==0)
      strncpy (parms->key_d, value, MAX_CONF_LEN);
    else if (strcmp(name, "keyl")==0)
      strncpy (parms->key_l, value, MAX_CONF_LEN);
    else if (strcmp(name, "keyr")==0)
      strncpy (parms->key_r, value, MAX_CONF_LEN);
    else if (strcmp(name, "keyspace")==0)
      strncpy (parms->key_space, value, MAX_CONF_LEN);
    else if (strcmp(name, "keyc")==0)
      strncpy (parms->key_c, value, MAX_CONF_LEN);
    else
      printf ("WARNING: %s/%s: Unknown name/value pair!\n",
        name, value);
  }

  /* Close file */
  fclose (fp);
}
