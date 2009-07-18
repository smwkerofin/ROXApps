import os
import sys
import popen2

import rox
import rox.tasks

def cmd_line(ifile, ofile, ispal, isws):
    if isws:
        aspect='16/9'
    else:
        aspect='4/3'

    if ispal:
        vframerate='25'
        ofps='25'
        keyint='15'

    else:
        ofps='30000/1001'
        ofps='30'
        keyint='18'

    cmd='mencoder -of mpeg -mpegopts'
    cmd+=' format=dvd:vaspect=%s:vframerate=%s' % (aspect, vframerate)
    cmd+=' -srate 48000 -ofps %s' % ofps
    cmd+=' -ovc lavc -oac lavc '
    cmd+=' -lavcopts vcodec=mpeg2video:vrc_buf_size=1835:'
    cmd+='keyint=%s:vrc_maxrate=9800:vbitrate=4900:' % keyint
    cmd+='aspect=%s:acodec=ac3:abitrate=192' % aspect
    cmd+=' "%s" -o "%s"' % (ifile, ofile)

    return cmd

def start(ifile, ofile, ispal, isws):
    cmd=cmd_line(ifile, ofile, ispal, isws)
    print cmd

    stdin, stdout_and_err=os.popen4(cmd, 't', 0)
    stdin.close()

    return stdout_and_err
