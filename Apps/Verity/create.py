import os
import sys
import popen2

import rox
import rox.tasks

def cmd_line(wdir, title, titles, lang, ispal, isws):
    cmd='dvdwizard -T "%s"' % title
    cmd+=' -A %s' % lang
    i=1
    for t in titles:
        cmd+=' -t "%s" "%s"' % (t, os.path.join(wdir, 'Title%02d.mpeg2' % i))
        i+=1

    return cmd
