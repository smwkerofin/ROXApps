/*
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIB_IS_FINAL
#include "rox-clib.h"

int main(int argc, char *argv[])
{
  int i;
  int state=0;
  const char *app_dir=getenv("APP_DIR");
  const char *platform=getenv("PLATFORM");

  if(!app_dir) {
    fprintf(stderr, "%s: APP_DIR not set!  Are you running AppRun?\n");
    exit(1);
  }
  if(!platform) {
    fprintf(stderr, "%s: PLATFORM not set!  Are you running AppRun?\n");
    exit(1);
  }

  for(i=1; i<argc; i++) {
    if(strcmp(argv[i], "--version")==0) {
      printf("%s %s\n", LIB_PROJECT, LIB_VERSION);
      
    } else if(strcmp(argv[i], "--cflags")==0) {
      printf("-I%s/%s/include ", app_dir, platform);
      fflush(stdout);
      state=system("gtk-config --cflags");
      if(state<0) {
	fprintf(stderr, "%s: gtk-config failed");
	perror("");
	state=1;
      }
      
    } else if(strcmp(argv[i], "--libs")==0) {
      printf("-L%s/%s/lib -l%s ", app_dir, platform, LIBNAME);
      fflush(stdout);
      state=system("gtk-config --libs");
      if(state<0) {
	fprintf(stderr, "%s: gtk-config failed");
	perror("");
	state=1;
      }

    } else if(strcmp(argv[i], "--runtime")==0) {
      printf("%s/%s/lib", app_dir, platform);

    } else if(strcmp(argv[i], "--help")==0) {
      printf("%s %s by %s\n", LIB_PROJECT, LIB_VERSION, LIB_AUTHOR);
      printf("Usage: %s [OPTIONS]\n", argv[0]);
      printf("Options are:\n");
      printf("\t--help\n");
      printf("\t--version\n");
      printf("\t--cflags\n");
      printf("\t--libs\n");
      printf("\nHome page is at %s\n", LIB_WEBSITE);
    }
  }

  return state;
}

/*
 * $Log$
 */
