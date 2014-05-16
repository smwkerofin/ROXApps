#!/usr/bin/env python

import os
import sys
import fcntl, termios, struct

import findrox
findrox.version(2, 0, 5)
import rox
import rox.processes
import rox.filer

import gobject

def main():
    assert len(sys.argv)==3

    work_dir=sys.argv[1]
    progid=sys.argv[2]

    controller=ControllerWindow(progid, work_dir)
    controller.start()
    rox.mainloop()

def bytes_ready(s):
    try:
        d=struct.pack('i', 0)
        x=fcntl.ioctl(s, termios.FIONREAD, d)
        n=struct.unpack('i', x)[0]
        #print 'm=%d on %d %s' % (n, s, x) 
    except:
        n=-1
        print 'm=%s: %s' % tuple(sys.exc_info()[:2])
    return n


class Controller(object):
    def __init__(self, progid, workdir):
        self.progid=str(progid)
        self.workdir=workdir
        self.fetcher=Fetch(self, progid, workdir)
        self.status=None
        self.description='Program %s' % progid
        self.file_name=''

    def start(self):
        p=self.fetcher.get_pipe()
        #print p
        print gobject.io_add_watch(p, gobject.IO_IN, self.read_line)
        
        self.fetcher.start()
        #print 'waiting for fetcher'
        #self.fetcher.wait()

        #print 'Done'
        #print self.fetcher.dst, self.fetcher.tmp_stream

        rox.toplevel_ref()
        rox.mainloop()

    def fetch_done(self, status):
        print 'Done', status
        #print 'Errors', repr(self.fetcher.errors)
        rox.toplevel_unref()
        rox.filer.open_dir(self.workdir)

    def read_line(self, source, condition, *unused):
        #print repr(source)
        nready=bytes_ready(source)
        #print nready
        full=source.read(nready)
        #print 'stdout=', repr(full)

        for line in full.split('\n'):

            if line.startswith('INFO: '):
                if line.startswith('INFO: Recorded '):
                    ofname=line[len('INFO: Recorded '):].strip()
                    self.set_file_name(ofname)

            elif line.startswith(self.progid+':\t'):
                pref=self.progid+':\t'
                desc=line[len(pref):].strip()
                self.set_description(desc)

        return True

    def progress(self, stage, per):
        print 'Progress: %s %4.1f%%' % (stage, per)

    def set_file_name(self, fname):
        self.file_name=fname
        print 'File name %r' % self.file_name

    def set_description(self, desc):
        self.description=desc
        print 'Description %r' % self.description

class ControllerWindow(Controller, rox.Window):
    def __init__(self, progid, workdir):
        rox.Window.__init__(self)
        Controller.__init__(self, progid, workdir)

        self.set_title('Program %s' % progid)

        vbox=rox.g.VBox(spacing=2)
        self.add(vbox)

        self.desc_lbl=rox.g.Label(self.description)
        vbox.pack_start(self.desc_lbl)
        
        self.fname_lbl=rox.g.Label(self.file_name)
        vbox.pack_start(self.fname_lbl)
        
        self.state_lbl=rox.g.Label('')
        vbox.pack_start(self.state_lbl)
        
        self.prog=rox.g.ProgressBar()
        vbox.pack_start(self.prog)

        self.show_all()

        #print self.desc_lbl, self.state_lbl

    def start(self):
        p=self.fetcher.get_pipe()
        #print p
        gobject.io_add_watch(p, gobject.IO_IN, self.read_line)
        
        self.fetcher.start()
        
    def progress(self, stage, per):
        self.state_lbl.set_text(stage)
        #print self.desc_lbl, self.state_lbl
        self.prog.set_fraction(per/100. if per<100 else 1.)

    def set_file_name(self, fname):
        self.file_name=fname
        self.fname_lbl.set_text(self.file_name)
        #print 'File name %r' % self.file_name

    def set_description(self, desc):
        self.description=desc
        #print 'Description %r' % self.description
        self.desc_lbl.set_text(self.description)

    def fetch_done(self, status):
        rox.filer.open_dir(self.workdir)
        self.progress('Done', 100.)
        
class Fetch(rox.processes.PipeThroughCommand):
    def __init__(self, controller, progid, workdir):
        self.controller=controller
        self.progid=progid
        self.workdir=workdir
        self.err_data=''
        inpipe, outpipe=os.pipe()
        self.from_child=os.fdopen(inpipe, 'r', 1)
        rox.processes.PipeThroughCommand.__init__(self, ('get_iplayer',
                                  '--type', 'radio',
                                  '--modes', 'flashaac',
                                  '--aactomp3',
                                  '--force',
                                  '--get', str(self.progid)), None,
                                                  os.fdopen(outpipe, 'w',
                                                            1))

        self.size=None

    def get_pipe(self):
        return self.from_child

    def child_post_fork(self):
        print 'change to', self.workdir
        os.chdir(self.workdir)

    def child_died(self, status):
        self.controller.fetch_done(status)
        rox.processes.PipeThroughCommand.child_died(self, status)

    def got_error_output(self, data):
        rox.processes.PipeThroughCommand.got_error_output(self, data)

        #print 'got_error_output', len(data)
        self.err_data+=data
        if '\r' in self.err_data:
            #print 'found some \\r'
            lines=self.err_data.split('\r')
            #print len(lines), repr(lines[-1])
            self.err_data=lines[-1]
            full=lines[-1]
            if not full and len(lines)>1:
                full=lines[-2]
            #print repr(full)

            if full.endswith('%)'):
                # download?
                p=full.rfind('(')
                #print p
                if p>=0:
                    tok=full[p+1:-2]
                    try:
                        per=float(tok)
                    except ValueError:
                        print 'oops', tok
                        return

                    #print 'Download %f%%' % per
                    self.controller.progress('Download', per)

                    toks=full.split()
                    kb=float(toks[0])
                    self.size=kb
                
            elif full.startswith('size='):
                # encode?
                toks=full.split()
                val=toks[1]
                if val.endswith('kB'):
                    try:
                        kb=float(val[:-2])
                    except ValueError:
                        return

                    #print 'Encode', kb, self.size
                    if self.size is not None:
                        per=100*kb/self.size
                        self.controller.progress('Encode', per)

            
    
if __name__=='__main__':
    main()
