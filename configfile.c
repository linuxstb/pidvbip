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

/* Command-line parsing routines based on those written by Adam Sutton for tvheadend */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "common.h"
#include "configfile.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

/* Command line option struct */
typedef struct {
  const char  sopt;
  const char *lopt;
  const char *desc;
  enum {
    OPT_NULL,
    OPT_STR,
    OPT_INT,
    OPT_BOOL
  }          type;
  void       *param;
  int        default_int;
  char       *default_str;
  int        setbyarg;
} cmdline_opt_t;

static int opt_help = 0,
           opt_version = 0;

/* The global settings struct */
struct configfile_parameters global_settings;

/* Definitions of all command-line and configfile options */
cmdline_opt_t cmdline_opts[] = {
  {   0, NULL,                "Generic Options",                         OPT_NULL, NULL, 0, NULL         },
  { '?', "help",              "Show this page",                          OPT_BOOL, &opt_help, 0, NULL    },
  { 'v', "version",           "Show version infomation",                 OPT_BOOL, &opt_version, 0, NULL },

  {   0, NULL,                "Configuration",                           OPT_NULL, NULL, 0, NULL         },
  { 'c', "config",            "Alternate config file location",          OPT_STR,  &global_settings.configfile, 0, NULL  },

  {   0, NULL,                "Server options",                          OPT_NULL, NULL, 0, NULL         },
#if ENABLE_AVAHI
  { 'a', "enable-avahi",      "Use AVAHI to search for tvh servers",     OPT_BOOL, &global_settings.avahi, 0, NULL },
#endif
  { 'h', "host",              "Hostname or IP address",                  OPT_STR,  &global_settings.host, 0, NULL },
  { 'p', "port",              "Port",                                    OPT_INT,  &global_settings.port, 9982, NULL },
  { 'U', "username",          "HTSP username",                           OPT_STR,  &global_settings.username, 0, NULL },
  { 'P', "password",          "HTSP password",                           OPT_STR,  &global_settings.password, 0, NULL },

  {   0, NULL,                "Startup options",                         OPT_NULL, NULL, 0, NULL         },
  {  'i', "channel",          "Number of initial channel to tune to",    OPT_INT,  &global_settings.initial_channel, -1, NULL },
  {   0, "startup-stopped",   "Immediately stream a channel on startup", OPT_BOOL, &global_settings.startup_stopped, 0, NULL },

  {   0, NULL,                "Hardware configuration",                  OPT_NULL, NULL, 0, NULL         },
  {  'o', "audio-output",     "Audio output destination: hdmi,local",    OPT_STR,  &global_settings.audio_dest, -1, "hdmi" },
#if ENABLE_LIBCEC
  {   0, "no-cec",            "Disable CEC support",                     OPT_BOOL, &global_settings.nocec, 0, NULL },
#endif

#if ENABLE_LIBAVFORMAT
  {   0, NULL,                "Experimental features",                   OPT_NULL, NULL, 0, NULL         },
  {   0, "avplay",            "Filename of video file to play",          OPT_STR,  &global_settings.avplay, -1, NULL },
#endif

};

void dump_settings(void)
{
  int i;

  fprintf(stderr,"*** Global Settings: ***\n");
  for (i = 0; i < ARRAY_SIZE(cmdline_opts); i++) {
    if (cmdline_opts[i].lopt) {
      fprintf(stderr,"%s=",cmdline_opts[i].lopt);
      if (cmdline_opts[i].type == OPT_STR) {
        char* s = *(char**)cmdline_opts[i].param;
        if (s == NULL) { printf("(null)"); } else { printf("%s",s); }
      } else {
        printf("%d",*(int*)cmdline_opts[i].param);
      }
      if (cmdline_opts[i].setbyarg) {
        printf(" (set by arg)");
      }
      printf("\n");
    }
  }
  fprintf(stderr,"************************\n");
}

static cmdline_opt_t* cmdline_opt_find
( cmdline_opt_t *opts, int num, const char *arg, int bare )
{
  int i;
  int isshort = 0;

  if (!bare) {
    if (strlen(arg) < 2 || *arg != '-')
      return NULL;
    arg++;

    if (strlen(arg) == 1)
      isshort = 1;
    else if (*arg == '-')
      arg++;
    else
      return NULL;
  }

  for (i = 0; i < num; i++) {
    if (!opts[i].lopt) continue;
    if (isshort && opts[i].sopt == *arg)
      return &opts[i];
    if (!isshort && !strcmp(opts[i].lopt, arg))
      return &opts[i];
  }

  return NULL;
}

static void show_error(const char* argv0, const char* err, ...)
{
  va_list args;
  va_start(args, err);

  fprintf(stderr,"%s: ",argv0);
  vfprintf(stderr, err, args);
  fprintf(stderr,"\nTry `%s --help' for more information.\n", argv0);

  va_end(args);
  exit(1);
}

static void
show_usage
  (const char *argv0, cmdline_opt_t *opts, int num)
{
  int i;
  char buf[256];

  printf("Usage: %s [OPTIONS]\n", argv0);
  for (i = 0; i < num; i++) {

    /* Section */
    if (!opts[i].lopt) {
      printf("\n%s\n\n",
            opts[i].desc);

    /* Option */
    } else {
      char sopt[4];
      char *desc, *tok;
      if (opts[i].sopt)
        snprintf(sopt, sizeof(sopt), "-%c,", opts[i].sopt);
      else
        strcpy(sopt, "   ");
      snprintf(buf, sizeof(buf), "  %s --%s", sopt, opts[i].lopt);
      desc = strdup(opts[i].desc);
      tok  = strtok(desc, "\n");
      while (tok) {
        printf("%-30s%s\n", buf, tok);
        tok = buf;
        while (*tok) {
          *tok = ' ';
          tok++;
        }
        tok = strtok(NULL, "\n");
      }
      free(desc);
    }
  }
  printf("\n");
  printf("For more information please visit the pidvbip website:\n");
  printf("  http://www.pidvbip.org/\n");
  printf("\n");
  exit(0);
}

static void
parse_config (char* argv0, struct configfile_parameters * parms)
{
  char *s, buff[256];
  FILE* fp = NULL;

  if (parms->configfile) {
    fp = fopen (parms->configfile, "r");
    if (fp == NULL) {
      fprintf(stderr,"No config file found in %s\n",parms->configfile);
      return;
    }
  }

  if (fp == NULL) {
    char* home = getenv("HOME");
    if (home) {
      snprintf(buff,sizeof(buff),"%s/.pidvbip",home);
      fprintf(stderr,"Looking for config file %s\n",buff);
      fp = fopen (buff, "r");
      if (fp == NULL) {
        fprintf(stderr,"No config file found in %s\n",buff);
      }
    }
  }

  if (fp == NULL) {
    return;
  }

  /* Read next line */
  while ((s = fgets (buff, sizeof buff, fp)) != NULL)
  {
    /* Skip blank lines and comments */
    if (buff[0] == '\n' || buff[0] == '#' || buff[0] == '[')
      continue;

    /* Parse name/value pair from line */
    s = index(buff, '=');
    if (s==NULL)
      continue;
    *s = 0; // Replace = with NUL
    s++;

    /* Trim trailing space/newline from value */
    char* p = s + strlen(s) - 1;
    while ((p >= s) && (isspace(*p)))
      *p-- = 0;

    /* Trim leading space from value */
    while (isspace(*s))
      s++;

    /* Find option */
    cmdline_opt_t *opt
      = cmdline_opt_find(cmdline_opts, ARRAY_SIZE(cmdline_opts), buff, 1);
    if (!opt)
      show_error(argv0, "unrecognised option '%s'", buff);

    /* Process only if it wasn't set by a command-line arg */
    if (!opt->setbyarg) {
      if (opt->type == OPT_STR)
        *((char**)opt->param) = strdup(s);
      else
        *((int*)opt->param) = atoi(s);
    }
  }

  /* Close file */
  fclose (fp);
}

void parse_args(int argc, char* argv[])
{
  int i;

  /* Set defaults */
  for (i = 0; i < ARRAY_SIZE(cmdline_opts); i++) {
    if (cmdline_opts[i].lopt) {
      if (cmdline_opts[i].type == OPT_STR) {
        *(char**)cmdline_opts[i].param = cmdline_opts[i].default_str;
      } else {
        *(int*)cmdline_opts[i].param = cmdline_opts[i].default_int;
      }
      cmdline_opts[i].setbyarg = 0;
    }
  }

  /* Process command line */
  for (i = 1; i < argc; i++) {
    /* Find option */
    cmdline_opt_t *opt
      = cmdline_opt_find(cmdline_opts, ARRAY_SIZE(cmdline_opts), argv[i], 0);
    if (!opt)
      show_error(argv[0], "unrecognised option '%s'", argv[i]);

    /* Process */
    if (opt->type == OPT_BOOL)
      *((int*)opt->param) = 1;
    else if (++i == argc)
      show_error(argv[0], "option %s requires a value", opt->lopt);
    else if (opt->type == OPT_INT)
      *((int*)opt->param) = atoi(argv[i]);
    else
      *((char**)opt->param) = argv[i];

    opt->setbyarg = 1;
    /* Stop processing */
    if (opt_help)
      show_usage(argv[0], cmdline_opts, ARRAY_SIZE(cmdline_opts));

//    if (opt_version)
//      show_version(argv[0]);
  }

  parse_config(argv[0], &global_settings);
}

