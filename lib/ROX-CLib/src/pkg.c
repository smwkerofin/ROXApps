/*
 * $Id: pkg.c,v 1.4 2002/04/29 08:17:24 stephen Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#define LIB_IS_FINAL
#include "rox-clib.h"

static int do_cflags(const char *app_dir, const char *platform);
static int do_libs(const char *app_dir, const char *platform);
static int do_runtime(const char *app_dir, const char *platform);
static int do_help(const char *app_dir, const char *platform);
static int do_version(const char *app_dir, const char *platform);

typedef int (*action)(const char *app_dir, const char *platform);

typedef struct handler {
  char sopt;
  const char *lopt;
  action func;
  const char *help;
} Handler;

static Handler handlers[]={
  {'c', "cflags",  do_cflags,  "Flags to pass to C compiler"},
  {'l', "libs",    do_libs,    "Flags to pass to linker"},
  {'r', "runtime", do_runtime, "Directories to search for shared libs"},
  {'h', "help",    do_help,    "Print help message"},
  {'v', "version", do_version, "Print short version information"},

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
    fprintf(stderr, "%s: APP_DIR not set!  Are you running AppRun?\n",
	    argv0);
    exit(1);
  }
  if(!platform) {
    fprintf(stderr, "%s: PLATFORM not set!  Are you running AppRun?\n",
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
	    state+=h->func(app_dir, platform);
	    break;
	  }
	}
	if(!h->lopt) {
	  fprintf(stderr, "%s: option %s not recognised\n", argv0, argv[i]);
	  state++;
	}
      } else {
	int j, l;
	l=strlen(argv[i]);
	for(j=1; j<l; j++) {
	  Handler *h;

	  for(h=handlers; h->sopt; h++) {
	    if(argv[i][j]==h->sopt) {
	      state+=h->func(app_dir, platform);
	      break;
	    }
	  }
	  if(!h->sopt) {
	    fprintf(stderr, "%s: option -%c not recognised\n", argv0,
		    argv[i][j]);
	    state++;
	  }
	}
      }
    }
  }

  return state;
}

static void nl_to_space(gchar *text)
{
  for(; *text; text++)
    if(*text=='\n')
      *text=' ';
}

static gchar *capture_output(const char *cmd)
{
  GString *outp=NULL;
  FILE *child;
  char buf[256], *line;

  child=popen(cmd, "r");
  if(!child) {
    fprintf(stderr, "%s: command \"%s\" failed\n", argv0, cmd);
    return NULL;
  }

  do {
    line=fgets(buf, sizeof(buf), child);
    if(!line)
      break;
    if(!outp)
      outp=g_string_new(buf);
    else
      outp=g_string_append(outp, buf);
  } while(line);

  pclose(child);

  line=outp->str;
  g_string_free(outp, FALSE);
  nl_to_space(line);

  return line;
}

static GString *run_command(GString *prev, const char *cmd)
{
  GString *str;
  gchar *outp;

  outp=capture_output(cmd);
  if(!outp)
    return prev;
  str=g_string_append_c(prev, ' ');
  str=g_string_append(str, outp);
  g_free(outp);

  return str;
}

static int do_cflags(const char *app_dir, const char *platform)
{
  int state=0;
  GString *line;
  gchar *tmp;
  char cmd[256];

  tmp=g_strdup_printf("-I%s/%s/include -I%s/%s/include/rox ",
		      app_dir, platform, app_dir, platform);
  line=g_string_new(tmp);
  g_free(tmp);
  line=run_command(line, GTK_CFLAGS);
#ifdef HAVE_XML
  sprintf(cmd, "%s --cflags", XML_CONFIG);
  line=run_command(line, cmd);
#endif
  puts(line->str);
  g_string_free(line, TRUE);

  return state;
}

static int do_libs(const char *app_dir, const char *platform)
{
  int state=0;
  GString *line;
  gchar *tmp;
  char cmd[256];

  tmp=g_strdup_printf("-L%s/%s/lib -l%s ", app_dir, platform, LIBNAME);
  line=g_string_new(tmp);
  g_free(tmp);
  line=run_command(line, GTK_LIBS);
#ifdef HAVE_XML
  sprintf(cmd, "%s --libs", XML_CONFIG);
  line=run_command(line, cmd);
#endif
  puts(line->str);
  g_string_free(line, TRUE);

  return state;
}

static int do_runtime(const char *app_dir, const char *platform)
{
  printf("%s/%s/lib", app_dir, platform);

  return 0;
}

static int do_help(const char *app_dir, const char *platform)
{
  Handler *h;
  
  printf("%s %s by %s\n", LIB_PROJECT, LIB_VERSION, LIB_AUTHOR);
  printf("Usage: %s [OPTIONS]\n", argv0);
  printf("Options are:\n");
  for(h=handlers; h->lopt; h++)
    printf(" -%c --%s\t%s\n", h->sopt, h->lopt, h->help);

  printf("\nCompile time options:\n");
#ifdef GTK2
  printf("\tGTK+ 2.x, using %s\n", PKG_CONFIG);
#else
  printf("\tGTK+ 1.2.x, using %s\n", GTK_CONFIG);
#endif
#ifdef HAVE_XML
  printf("\tHave XML, using %s\n", XML_CONFIG);
#else
  printf("\tNo XML\n");
#endif

  return 0;
}

static int do_version(const char *app_dir, const char *platform)
{
  printf("%s %s\n", LIB_PROJECT, LIB_VERSION);

  return 0;
}

/*
 * $Log: pkg.c,v $
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
