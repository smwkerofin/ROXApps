/*
 * $Id: pkg.c,v 1.7 2006/03/11 12:10:28 stephen Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>

#include <glib.h>

#define LIB_IS_FINAL
#include "rox-clib.h"

static int do_cflags(const char *app_dir, const char *platform,
		     char **args);
static int do_libs(const char *app_dir, const char *platform,
		     char **args);
static int do_runtime(const char *app_dir, const char *platform,
		     char **args);
static int do_env(const char *app_dir, const char *platform,
		     char **args);
static int do_pkgconfig(const char *app_dir, const char *platform,
		     char **args);

static int do_help(const char *app_dir, const char *platform,
		     char **args);
static int do_version(const char *app_dir, const char *platform,
		     char **args);

static int run_pkgconfig(const char *app_dir, const char *platform,
			  const char *flag);

typedef int (*action)(const char *app_dir, const char *platform, char **args);

typedef struct handler {
  char sopt;
  const char *lopt;
  action func;
  const char *help;
} Handler;

static Handler handlers[]={
  {'c', "cflags",  do_cflags,  N_("Flags to pass to C compiler")},
  {'l', "libs",    do_libs,    N_("Flags to pass to linker")},
  {'r', "runtime", do_runtime, N_("Directories to search for shared libs")},
  {'e', "env",     do_env,
    N_("echo environment settings to use for run-time linking (sh syntax)")},
  {'p', "pkg-config", do_pkgconfig,
    N_( "Execute pkg-config with the trailing arguments")},
  
  {'h', "help",    do_help,    N_("Print help message")},
  {'v', "version", do_version, N_("Print short version information")},

  {0, NULL, NULL, NULL}
};

static const char *argv0;

int main(int argc, char *argv[])
{
  int i;
  int state=0;
  const char *app_dir=getenv("APP_DIR");
  const char *platform=getenv("PLATFORM");

  argv0=argv[0];
  
  if(!app_dir) {
    fprintf(stderr, _("%s: APP_DIR not set!  Are you running AppRun?\n"),
	    argv0);
    exit(1);
  }
  if(!platform) {
    fprintf(stderr, _("%s: PLATFORM not set!  Are you running AppRun?\n"),
	    argv0);
    exit(1);
  }

  for(i=1; i<argc; i++) {
    if(argv[i][0]!='-') {
      /* Ignore it */
    } else {
      if(argv[i][1]=='-') {
	Handler *h;

	for(h=handlers; h->lopt; h++) {
	  if(strcmp(argv[i]+2, h->lopt)==0) {
	    state+=h->func(app_dir, platform, argv+i+1);
	    break;
	  }
	}
	if(!h->lopt) {
	  fprintf(stderr, _("%s: option %s not recognised\n"), argv0, argv[i]);
	  state++;
	}
      } else {
	int j, l;
	l=strlen(argv[i]);
	for(j=1; j<l; j++) {
	  Handler *h;

	  for(h=handlers; h->sopt; h++) {
	    if(argv[i][j]==h->sopt) {
	      state+=h->func(app_dir, platform, argv+i+1);
	      break;
	    }
	  }
	  if(!h->sopt) {
	    fprintf(stderr, _("%s: option -%c not recognised\n"), argv0,
		    argv[i][j]);
	    state++;
	  }
	}
      }
    }
  }

  return state;
}

static int run_pkgconfig(const char *app_dir, const char *platform,
			  const char *flag)
{
  gchar *cmd;
  int stat;
  
  cmd=g_strdup_printf("pkg-config --define-variable=APP_DIR=%s %s ROX-CLib",
		      app_dir, flag);
  stat=system(cmd);
  g_free(cmd);

  return stat;
}

static int do_cflags(const char *app_dir, const char *platform, char **args)
{
  return run_pkgconfig(app_dir, platform, "--cflags");
}

static int do_libs(const char *app_dir, const char *platform, char **args)
{
  return run_pkgconfig(app_dir, platform, "--libs");
}

static int do_runtime(const char *app_dir, const char *platform, char **args)
{
  printf("%s/%s/lib", app_dir, platform);

  return 0;
}

static int do_help(const char *app_dir, const char *platform, char **args)
{
  Handler *h;
  
  
  printf(_("%s %s by %s\n"), LIB_PROJECT, LIB_VERSION, LIB_AUTHOR);
  printf(_("Usage: %s/AppRun [OPTIONS]\n"), app_dir);
  printf(_("Options are:\n"));
  for(h=handlers; h->lopt; h++)
    printf(" -%c --%s\t%s\n", h->sopt, h->lopt, _(h->help));

  printf("    --%s\t%s\n", "compile", _("Build the library"));
  printf("    --%s\t%s\n", "xterm-compile",
	 _("Build the library, first executing an xterm window to "
	   "show the output"));
  printf("    --%s\t%s\n", "location", _("Print the location of ROX-CLib"));

  return 0;
}

static int do_version(const char *app_dir, const char *platform, char **args)
{
  printf("%s %s\n", LIB_PROJECT, LIB_VERSION);

  return 0;
}

static int do_env(const char *app_dir, const char *platform,
		     char **args)
{
  printf("LD_LIBRARY_PATH=%s/%s/lib:$LD_LIBRARY_PATH "
	 "export LD_LIBRARY_PATH\n", app_dir, platform);
  return 0;
}

static int do_pkgconfig(const char *app_dir, const char *platform,
		     char **args)
{
  int narg, i;
  gchar **argv;

  for(narg=0; args[narg]; narg++)
    ;

  narg+=3;
  argv=g_new(gchar *, narg);
  argv[0]="pkg-config";
  argv[1]=g_strdup_printf("--define-variable=APP_DIR=%s", app_dir);
  for(i=0; args[i]; i++)
    argv[2+i]=args[i];
  argv[2+i]=NULL;

  execvp("pkg-config", argv);
  fprintf(stderr, _("%s: failed to execute pkg-config: %s"), argv0,
	  strerror(errno));
  exit(1);

  return 1;
}

/*
 * $Log: pkg.c,v $
 * Revision 1.7  2006/03/11 12:10:28  stephen
 * Added options --env and --pkg-config and made strings in pkg.c translatable.
 *
 * Revision 1.6  2004/08/04 18:20:37  stephen
 * Use pkg-config
 *
 * Revision 1.5  2004/03/10 22:41:16  stephen
 * Change default location of include files
 *
 * Revision 1.4  2002/04/29 08:17:24  stephen
 * Fixed applet menu positioning (didn't work if program was managing more than
 * one applet window)
 * Some work for GTK+ 2
 *
 * Revision 1.3  2002/02/25 09:50:23  stephen
 * Fixed bug in determining XML config
 *
 * Revision 1.2  2002/02/13 11:00:37  stephen
 * Better way of accessing web site (by running a URI file).  Improvement to
 * language checking in rox_resources.c.  Visit by the const fairy in choices.h.
 * Updated pkg program to check for libxml2.  Added rox.c to access ROX-CLib's
 * version number.
 *
 * Revision 1.1.1.1  2001/05/29 14:09:59  stephen
 * Initial version of the library
 *
 */
