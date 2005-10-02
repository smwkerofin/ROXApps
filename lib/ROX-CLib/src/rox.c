/*
 * $Id: rox.c,v 1.10 2005/08/14 16:07:00 stephen Exp $
 *
 * rox.c - General stuff
 */

/** \file rox.c
 * @brief General ROX functions.
 */

/**
 * @mainpage A library for ROX applications written in C.
 *
 * @section user_sec For users
 * @subsection install_sec Installation
 *
 * Copy to a suitable place such as <code>~/lib</code> 
 *     or make sure it is on 
 *    your <code>$LIBDIRPATH</code>.
 *  If you use 
 * <a href="http://rox.sourceforge.net/phpwiki/index.php/Archive">Archive</a>
 * to extract it from the downloaded archive then make sure you rename
 * it to <code>ROX-CLib</code>.
 * 
 * Compile using <code>ROX-CLib/AppRun --compile</code> or running
 * ROX-CLib from the filer.
 *
 * You will need
 * <ul>
 * <li>GTK+ 2.x</li>
 * <li>libxml 2.4.x </li>
 * <li>GNU <code>make</code>.</li>
 * </ul>
 * Linux users should already have GNU make installed <code>/usr/bin</code>.
 * Other OS's may require you
 * use a different directory (<code>/usr/bin/local</code>) or program name
 * (<code>gmake</code>).
 * Set the environment variable MAKE to the correct value before
 * compiling, e.g.
 * <pre>
$ MAKE=gmake ROX-CLib/AppRun --compile
</pre>
 *
 * @section prog_sec For Programmers
 *
 * @subsection lib_sec Compiling and linking against the library
 * 
 * Compile the library first as above.
 *
 * Copy the <a href="../libdir.html">libdir</a> script from 
 * <code>ROX-CLib/<var>$PLATFORM</var>/bin/libdir</code>
 * into your app dir.
 * 
 * Add the output of
 *  <pre>   ROX-CLib --cflags</pre>
 * to your compile line, e.g.
 * <pre>   ROX_CLIB="`$APP_DIR/libdir ROX-CLib`/AppRun"</pre>
 * <pre>   gcc `$ROX_CLIB --cflags` -c main.c </pre>
 *
 * You should include the main file:
<pre>
   #include &lt;rox/rox.h&gt;
</pre>
 * and any of the others you need.
 *
 * Add the output of
<pre>   ROX-CLib --cflags
   ROX-CLib --libs</pre>
 * to your link line, e.g.
<pre>   ROX_CLIB="`$APP_DIR/libdir ROX-CLib`/AppRun"</pre>
    <pre>   gcc `$ROX_CLIB --cflags` -o main main.o `$ROX_CLIB --libs`</pre>
 *
 * If you are using autoconf and building a ROX program then:
 * <ul>
 * <li>In src/configure.in add:
 * <pre>ROX_CLIB(2, 0, 0)</pre></li>
 * <li>Copy ROX-CLib/Help/rox-clib.m4 to your programs src directory then
 * execute
 * <pre>aclocal -I .</pre></li>
 * 
 * <li>In src/Makefile.in add:
 * <pre>  ROX_CLIB = @@ROX_CLIB_PATH@/AppRun</pre>
 *     and to CFLAGS add: <code>`${ROX_CLIB} --cflags`</code>
 *     and to LDFLAGS add: <code>`${ROX_CLIB} --libs`</code>
 * </li>
 * <li>In AppRun (and AppletRun if it is an applet) ensure you have:
<pre>
   APP_DIR=`dirname $0`
   APP_DIR=`cd $APP_DIR;pwd`; export APP_DIR
   if [ -z "$LD_LIBRARY_PATH" ]; then
       LD_LIBRARY_PATH=`$APP_DIR/libdir ROX-CLib --runtime`
   else
       LD_LIBRARY_PATH=`$APP_DIR/libdir ROX-CLib --runtime`:$LD_LIBRARY_PATH
   fi
   export LD_LIBRARY_PATH
</pre>
 * </li>
 * </ul>
 *
 * @subsection zero_sec Zero Install
 *
 * To use ROX-CLib through Zero Install change the use of <a href="libdir.html">libdir</a> to:
 * <pre>    ROX_CLIB="`$APP_DIR/libdir --0install www.kerofin.demon.co.uk ROX-CLib`/AppRun"</pre>
 *
 */

#include "rox-clib.h"

#include <stdlib.h>
#include <glib.h>
#include <gtk/gtk.h>

#include <unistd.h>

#include "rox.h"
#include "options.h"

static gchar *program_name=NULL;
static gchar *domain_name=NULL;
static GdkPixbuf *program_icon=NULL;
const gchar *app_dir=NULL;

/** Initialize the library.  Equivalent to
 * rox_init_with_domain(program, NULL, argc, argv).
 * @deprecated Use rox_init_with_domain() instead.
 *
 * @param[in] program name of program
 * @param[in,out] argc pointer to argc passed to main()
 * @param[in,out] argv pointer to argv passed to main()
 */
void rox_init(const char *program, int *argc, char ***argv)
{
  rox_init_with_domain(program, NULL, argc, argv);
}

/** Initialize the library.  This calls:
 * - gtk_init()
 * - rox_debug_init()
 * - choices_init()
 * - options_init_with_domain() (if @c Options.xml in <var>$APP_DIR</var>)
 * - rox_dnd_init()
 * - mime_init()
 *
 * If @c .DirIcon exists in <var>$APP_DIR</var> it is set as the default icon for all windows
 *
 * @param[in] program name of program
 * @param[in] domain domain under the control of the programmer.  Used in options and choices
 * @param[in,out] argc pointer to argc passed to main()
 * @param[in,out] argv pointer to argv passed to main()
 */
void rox_init_with_domain(const char *program, const char *domain,
			  int *argc, char ***argv)
{
  
  gtk_init(argc, argv);

  program_name=g_strdup(program);
  if(domain)
    domain_name=g_strdup(domain);
  rox_debug_init(program); 

  choices_init();

  app_dir=g_getenv("APP_DIR");
  if(app_dir) {
    gchar *options_file;
    gchar *dir_icon;
    
    options_file=g_build_filename(app_dir, "Options.xml", NULL);
    if(access(options_file, R_OK)==0) {
      options_init_with_domain(program, domain);
    }
    g_free(options_file);
    
    dir_icon=g_build_filename(app_dir, ".DirIcon", NULL);
    program_icon=gdk_pixbuf_new_from_file(dir_icon, NULL);
    if(program_icon) {
      GList *list=g_list_append(NULL, program_icon);
      
      gtk_window_set_default_icon_list(list);
      g_list_free(list);
    }
    g_free(dir_icon);
  }
  
  rox_dnd_init();
  mime_init();
}

/** Returns the program name as passed to rox_init_with_domain()
 *
 * @return the program name
 */
const gchar *rox_get_program_name(void)
{
  return program_name;
}

/** Returns the application directory.
 *
 * @return the application directory, which may be NULL or "" if the AppRun
 * script wasn't called
 */
const gchar *rox_get_app_dir(void)
{
  if(!app_dir)
    app_dir=g_getenv("APP_DIR");
  return app_dir;
}

/** Returns the program icon used for the windows
 *
 * @return the program icon, or NULL if there isn't one
 */
GdkPixbuf *rox_get_program_icon(void)
{
  if(program_icon)
    g_object_ref(program_icon);
  return program_icon;
}

/** Returns the version number of the library encoded as an int
 *
 * @return 10000*major+100*minor+micro
 */
int rox_clib_version_number(void)
{
  int ver=0;
  gchar **words;

  words=g_strsplit(ROXCLIB_VERSION, " ", 2);
  if(words) {
    gchar **els;

    els=g_strsplit(words[0], ".", 3);
    if(els) {

      ver=atoi(els[2]) + 100*atoi(els[1]) + 10000*atoi(els[0]);

      g_strfreev(els);
    }
    g_strfreev(words);
  }

  return ver;
}

/** Returns the version number of the library as a string
 *
 * @return "major.minor.micro"
 */
const char *rox_clib_version_string(void)
{
  return ROXCLIB_VERSION;
}

/** Returns the version number of the GTK+ library encoded as an int
 *
 * @return 10000*major+100*minor+micro
 */
int rox_clib_gtk_version_number(void)
{
  return GTK_MAJOR_VERSION*10000+GTK_MINOR_VERSION*100+GTK_MICRO_VERSION;
}

/** Returns the version number of the GTK+ library as a string
 *
 * @return "major.minor.micro"
 */
const char *rox_clib_gtk_version_string(void)
{
  static char buf[32];

  sprintf(buf, "%d.%d.%d", GTK_MAJOR_VERSION, GTK_MINOR_VERSION,
	  GTK_MICRO_VERSION);
  
  return buf;
}

/* Window counting */
static int num_windows=0;
static int in_mainloops=0;

static void window_destroy(GtkWidget *win, gpointer udata)
{
  if(num_windows<1)
    rox_debug_printf(1, "num_windows=%d when destroying %p", num_windows,
		     win);
  num_windows--;
  if(num_windows==0 && in_mainloops)
    gtk_main_quit();
}

/** Add this window to the ones which are being counted.  When it is destroyed
 * this will be noted and the window count decreased.
 *
 * @param window Window or other widget to count
 */
void rox_add_window(GtkWidget *window)
{
  num_windows++;
  g_signal_connect(window, "destroy", G_CALLBACK(window_destroy), NULL);
}

/** Return the number of windows ROX-CLib is tracking
 *
 * @return the number of windows passed to rox_add_window(), minus the number
 * of those destroyed
 */
int rox_get_n_windows(void)
{
  return num_windows;
}

static int exit_now=FALSE;

/** Call gtk_main() repeatedly until either rox_get_n_windows() returns
 * less than one, or rox_main_quit() is called
 */
void rox_main_loop(void)
{
  in_mainloops++;
  while(num_windows>0 && !exit_now)
    gtk_main();
  in_mainloops--;
  exit_now=FALSE;
}

/** Cause rox_main_loop() to exit.  If rox_main_loop() has been called
 * recursivly only the innermost is exited.
 */
void rox_main_quit(void)
{
  exit_now=TRUE;
  gtk_main_quit();
}

/*
 * $Log: rox.c,v $
 * Revision 1.10  2005/08/14 16:07:00  stephen
 * Added rox_resources_find_with_domain().
 * More doxygen additions.
 *
 * Revision 1.9  2005/06/07 10:24:52  stephen
 * Using doxygen to generate documentation
 *
 * Revision 1.8  2004/10/29 13:36:07  stephen
 * Added rox_choices_load()/save()
 *
 * Revision 1.7  2004/10/23 11:50:12  stephen
 * Added window counting
 *
 * Revision 1.6  2004/06/21 21:09:02  stephen
 * rox_init calls mime_init
 *
 * Revision 1.5  2004/05/22 17:03:57  stephen
 * Added AppInfo parser
 *
 * Revision 1.4  2004/03/10 22:40:35  stephen
 * Get the .DirIcon in rox_init and set it as default icon for windows
 *
 * Revision 1.3  2003/10/22 17:17:01  stephen
 * Added rox_init
 *
 * Revision 1.2  2002/04/29 08:17:24  stephen
 * Fixed applet menu positioning (didn't work if program was managing more than
 * one applet window)
 * Some work for GTK+ 2
 *
 * Revision 1.1  2002/02/13 11:00:37  stephen
 * Better way of accessing web site (by running a URI file).  Improvement to
 * language checking in rox_resources.c.  Visit by the const fairy in choices.h.
 * Updated pkg program to check for libxml2.  Added rox.c to access ROX-CLib's
 * version number.
 *
 */
