# $Id: gui.py,v 1.5 2005/05/27 10:14:40 stephen Exp $

import findrox; findrox.version(2, 0, 0)

import rox
import rox.choices
import rox.basedir
import rox.filer
import rox.i18n
import DesktopFile

import os
import sys
import stat

__builtins__._ = rox.i18n.translation(os.path.join(rox.app_dir, 'Messages'))

cdir=rox.basedir.save_config_path('kerofin.demon.co.uk', 'Desktop2App', 'Apps')

if len(sys.argv)<2:
    if os.access(cdir, os.R_OK)==0:
        rox.alert(_("""No apps have been generated.  First drop a
        .desktop file or directory onto the icon."""))
        sys.exit(1)
    rox.filer.open_dir(cdir)
    sys.exit(0)
    
path=sys.argv[1]

try:
    locale=os.environ['LANG']
except KeyError:
    locale=None

def do_file(path):
    desk=DesktopFile.DesktopFile(path)
    if os.access(cdir, os.R_OK)==0:
        os.makedirs(cdir)
    desk.makeAppDirs(cdir, locale)

def do_object(path):
    try:
        info=os.stat(path)
        if stat.S_ISDIR(info.st_mode):
            do_dir(path)
        elif path[-8:]=='.desktop':
            do_file(path)
    except Exception, exc:
        print exc
        rox.report_exception()

def do_dir(path):
    entries=os.listdir(path)
    for entry in entries:
        do_object(os.path.join(path, entry))

do_object(path)
if os.access(cdir, os.R_OK)!=0:
    rox.filer.open_dir(cdir)
