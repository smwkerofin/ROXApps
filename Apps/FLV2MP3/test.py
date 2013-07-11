import os
import sys
import tempfile
import fcntl
import termios
import struct

import findrox
findrox.version(2, 0, 5)
import rox
import rox.processes
import gobject

def nread(fd):
    buf=struct.pack('i', 0)
    res=fcntl.ioctl(fd, termios.FIONREAD, buf)
    nb=struct.unpack('i', res)[0]
    print nb
    return nb


class ConvertProcess(rox.processes.PipeThroughCommand):
    def __init__(self, window, command, out):
        print command
        rox.processes.PipeThroughCommand.__init__(self, command, None, out)
        self.window=window
        self.command=command

    def child_died(self, status):
        if self.window:
            self.window.convert_finished(status)

def child_out(source, condition):
    print source, condition
    if condition==gobject.IO_IN:
        nb=nread(source)

        print repr(os.read(source, nb))
        return False
    return True
  
pipe=os.pipe()
print pipe
proc=ConvertProcess(None, 'ls',
                    os.fdopen(pipe[1], 'w'))
gobject.io_add_watch(pipe[0], gobject.IO_IN, child_out)
proc.start()
main=gobject.MainLoop()
main.run()
