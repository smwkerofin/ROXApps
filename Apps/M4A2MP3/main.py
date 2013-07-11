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

def respace(s):
    return s.replace('_', ' ')

class Window(rox.Window):
    def __init__(self, fname):
        rox.Window.__init__(self)
        self.set_title('FLV to MP3')

        self.fname=fname
        self.make_gui()

        dname, leaf=os.path.split(os.path.abspath(self.fname))

        artist=respace(os.path.basename(dname))
        words=leaf.split('.')
        title=respace(words[0])
        self.artist.set_text(artist)
        self.ftitle.set_text(title)

    def make_gui(self):
        vbox=rox.g.VBox()
        self.add(vbox)

        lbl=rox.g.Label(self.fname)
        vbox.pack_start(lbl, False, False, 2)

        table=rox.g.Table(4, 2)
        vbox.pack_start(table, True, True, 2)
        self.artist=self.add_row(table, 0, 'Artist', rox.g.Entry())
        self.album=self.add_row(table, 1, 'Album', rox.g.Entry())
        adj=rox.g.Adjustment(0, 0, 99, 1)
        self.track=self.add_row(table, 2, 'Track', rox.g.SpinButton(adj))
        self.ftitle=self.add_row(table, 3, 'Title', rox.g.Entry())

        bbar=rox.g.HButtonBox()
        vbox.pack_end(bbar, False, False, 2)
        bbar.set_layout(rox.g.BUTTONBOX_END)
        but=rox.ButtonMixed(rox.g.STOCK_SAVE_AS, '_Convert')
        bbar.pack_start(but, False, False, 2)
        but.connect('clicked', self.do_convert)
        but=rox.g.Button(stock=rox.g.STOCK_CANCEL)
        but.connect('clicked', self.do_cancel)
        bbar.pack_start(but, False, False, 2)

        vbox.show_all()

    def add_row(self, table, row, text, widget):
        lbl=rox.g.Label(text)
        table.attach(lbl, 0, 1, row, row+1)
        table.attach(widget, 1, 2, row, row+1)
        return widget

    def do_cancel(self, button):
        self.hide()
        rox.toplevel_unref()

    def do_convert(self, button):
        #print self.build_command()
        self.pipe=os.pipe()
        print self.pipe
        proc=ConvertProcess(self, self.build_command(),
                            os.fdopen(self.pipe[1], 'w'))
        self.out_win=OutWindow(self.pipe[0], self.fname)
        proc.start()

    def build_command(self):
        self.tfile=tempfile.NamedTemporaryFile()
        cmd='flv2mp3 --file "%s" "%s"' % (self.tfile.name, self.fname)
        
        def store(fout, arg, widget):
            txt=widget.get_text()
            #txt=txt.replace("'", "\\'")
            
            if txt:
                fout.write('%s=%s\n' % (arg, txt))
        store(self.tfile, 'artist', self.artist)
        store(self.tfile, 'album', self.album)
        store(self.tfile, 'title', self.ftitle)
        track=self.track.get_value_as_int()
        if track>0:
            self.tfile.write('track=%d\n' % track)
        self.tfile.flush()

        return cmd

    def convert_finished(self, status):
        print status
        #self.tfile=None
        self.out_win.destroy()
        del self.out_win
        self.destroy()
        #rox.toplevel_unref()
        #print 'convert finished'
        #rox.toplevel_unref()
        #print 'convert finished'
        

class ConvertProcess(rox.processes.PipeThroughCommand):
    def __init__(self, window, command, out):
        print command
        rox.processes.PipeThroughCommand.__init__(self, command, None, out)
        self.window=window
        self.command=command

    def child_died(self, status):
        self.window.convert_finished(status)

class OutWindow(rox.Window):
    def __init__(self, in_fd, fname):
        rox.Window.__init__(self)
        self.set_title(fname)
        self.inf=os.fdopen(in_fd, 'r')
        gobject.io_add_watch(in_fd, gobject.IO_IN, self.child_out)

        swin=rox.g.ScrolledWindow()
        swin.set_size_request(640, 480)
        self.add(swin)

        self.view=rox.g.TextView()
        self.buf=self.view.get_buffer()
        swin.add(self.view)
        self.view.set_editable(False)

        self.show_all()

    def child_out(self, source, condition):
        if condition==gobject.IO_IN:
            buf=struct.pack('i', 0)
            res=fcntl.ioctl(self.inf, termios.FIONREAD, buf)
            nb=struct.unpack('i', res)[0]
            print nb
            if nb<1:
                return True
            out=self.inf.read(nb)
            print len(out)
            assert len(out)==nb
            pos=self.buf.get_end_iter()
            self.buf.insert(pos, out)
            self.view.scroll_to_iter(pos, 0.1)
        return True
       
def run():
    win=Window(sys.argv[1])
    win.show()
    rox.mainloop()



