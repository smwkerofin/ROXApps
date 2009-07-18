import os
import sys
import popen2

import rox
import rox.tasks

def cmd_line(idir, ofile):
    return 'genisoimage -dvd-video -o %s %s' % (ofile, idir)

def start(ifile, ofile, ispal, isws):
    cmd=cmd_line(ifile, ofile, ispal, isws)
    print cmd

    stdin, stdout_and_err=os.popen4(cmd, 't', 0)
    stdin.close()

    return stdout_and_err
