#!/usr/bin/env python

import os
import sys
import fcntl

import findrox; findrox.version(1, 9, 9)
#print sys.path
import rox
import rox.choices
import rox.mime
import rox.loading
import rox.saving

import gobject

if not rox.choices.paths[0]:
    choices_locked=True
else:
    choices_locked=False

class Window(rox.Window, rox.saving.Saveable, rox.loading.XDSLoader):
    def __init__(self, src_dir=None):
        rox.Window.__init__(self)
	if src_dir:
		self.dir=os.path.abspath(src_dir)
		vname=os.path.basename(self.dir)
	else:
		self.dir=''
		vname='cd'

        self.set_title('Make ISO CD image')

        vbox=rox.g.VBox()
        self.add(vbox)
        vbox.show()

        self.src=rox.g.Label(self.dir)
        vbox.pack_start(self.src)
	self.src.set_line_wrap(True)
        self.src.show()
        
        s=rox.g.HSeparator()
        vbox.pack_start(s)
        s.show()

	hbox=rox.g.HBox()
        vbox.pack_start(hbox)
        hbox.show()

	l=rox.g.Label('Name')
        hbox.pack_start(l)
        l.show()

	self.vname=rox.g.Entry()
	self.vname.set_text(vname)
        hbox.pack_start(self.vname)
        self.vname.show()	

	hbox=rox.g.HBox()
        vbox.pack_start(hbox)
        hbox.show()

	self.follow=rox.g.CheckButton('Follow symlinks')
	self.follow.set_active(True)
        hbox.pack_start(self.follow)
        self.follow.show()

	self.save_area=rox.saving.SaveArea(self, self.dir+'.iso',
					   'application/x-iso-cdrom')
        vbox.pack_start(self.save_area)
        self.save_area.show()	

        self.msg=rox.g.Label(self.dir)
        vbox.pack_start(self.msg)
	self.msg.set_line_wrap(True)
        #self.src.show()
        
	self.prog=rox.g.ProgressBar()
        vbox.pack_start(self.prog)
        #self.prog.show()	

        s=rox.g.HSeparator()
        vbox.pack_start(s)
        s.show()

        self.buttons=rox.g.HButtonBox()
        vbox.pack_start(self.buttons)

        self.close=rox.g.Button(stock=rox.g.STOCK_CLOSE)
        self.close.connect('clicked', self.do_close)
        self.buttons.pack_end(self.close, False, True)

        self.ok=rox.g.Button(stock=rox.g.STOCK_OK)
        self.ok.connect('clicked', self.do_ok)
        self.buttons.pack_end(self.ok, False, True)
        
        self.buttons.set_layout(rox.g.BUTTONBOX_END)
        self.buttons.show_all()

        self.set_position(rox.g.WIN_POS_CENTER)

        rox.loading.XDSLoader.__init__(self, ['inode/directory'])

    def set_source(self, src_dir):
        self.dir=os.path.abspath(src_dir)
        vname=os.path.basename(self.dir)
	self.vname.set_text(vname)

        # This should be part of SaveArea...
        self.save_area.entry.set_text(self.dir+'.iso')

    def do_close(self, ignored):
        self.destroy()
        rox.g.main_quit()

    def do_ok(self, ignored):
        self.save_area.save_to_file_in_entry()

    def message(self, text):
	    self.msg.show()
	    self.msg.set_text(text)

    def can_save_to_file(self): return True
    def can_save_to_selection(self): return False

    def save_to_file(self, path):
	    self.ok.set_sensitive(False)
	    self.set_progress(0)

	    cmd='mkisofs'
	    if self.follow.get_active():
		    cmd+=' -r'
	    else:
		    cmd+=' -R'
	    cmd+=' -V "%s"' % self.vname.get_text()
	    cmd+=' -o "%s"' % path
	    cmd+=' "%s"' % self.dir
	    #print cmd

	    chin, chout, cherr=os.popen3(cmd, 'r', 1)
	    #self.child_in=chout
	    #self.child_error=cherr

	    #rox.g.timeout_add(500, self.update)
	    gobject.io_add_watch(cherr.fileno(), gobject.IO_IN, self.update, cherr, 2112)
	    gobject.io_add_watch(cherr.fileno(), gobject.IO_HUP, self.done, cherr, 2112)

    def done(self, fno, cond, f, *args):
	    self.ok.set_sensitive(True)
	    self.set_progress(100)

    def set_uri(self, uri):
	    self.save_area.entry.set_text(uri)

    def update(self, fno, cond, f, *args):
	    #print args

	    #line=self.child_in.readline()
	    #print line
	    line=f.readline()
	    #print line
	    if not line:
		    #self.done()
		    return False
	    if line[6]=='%':
		    perc=int(line[:3])
		    self.set_progress(perc)
		    self.message('')
	    else: 
		    self.message(line.strip())
		    
	    return True

    def set_progress(self, perc=0):
	    self.prog.show()
	    self.prog.set_fraction(perc/100.)
	    self.prog.set_text('%d %%' % perc)

    def xds_load_from_file(self, path):
        if not os.path.isdir(path):
            rox.alert('%s is not a directory' % path)
            return
        self.set_source(path)

try:
    win=Window(sys.argv[1])
except IndexError:
    rox.alert('%s needs a directory as an argument' % 'MakeISO')
    sys.exit(2)
except:
    rox.report_exception()
    sys.exit(1)

win.show()
try:
    rox.mainloop()
except:
    rox.report_exception()
    sys.exit(3)
