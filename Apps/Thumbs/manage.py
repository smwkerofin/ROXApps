import os, sys
import tempfile

import rox
import rox.mime
import rox.basedir
import rox.loading
import rox.processes
import gobject

import helper

img_size=rox.mime.ICON_SIZE_LARGE

class ThumbnailError(Exception):
    def __init__(self, reason):
        self.why=reason

    def __str__(self):
        return self.why
    def __repr__(self):
        return '<ThumbnailError "%s">' % self.why

class DropTarget(rox.loading.XDSLoader, rox.g.Frame):
	def __init__(self, parent, caption):
		self.controller=parent
		rox.g.Frame.__init__(self, caption)
		rox.loading.XDSLoader.__init__(self, None)

		self.set_size_request(100, 100)
		self.set_shadow_type(rox.g.SHADOW_IN)
		self.set_border_width(4)

		vbox=rox.g.VBox(False, 8)
		self.add(vbox)
		self.img=rox.g.Image()
		vbox.pack_start(self.img, True, True)
		self.msg=rox.g.Label()
		align=rox.g.Alignment(0.5, 0.5, 1.0, 0.0)
		vbox.pack_start(align, False, False)
		align.add(self.msg)

        def set_message(self, msg):
            self.msg.set_text(msg)

        def set_image(self, pix):
            self.img.set_from_pixbuf(pix)

	def clear_image(self):
		self.img.set_from_stock(rox.g.STOCK_DIALOG_QUESTION,
					rox.g.ICON_SIZE_DIALOG)

	def xds_load_from_file(self, path):
		self.controller.file_dropped(self, path)

class TypeDropTarget(DropTarget):
    def __init__(self, parent):
        DropTarget.__init__(self, parent, _('Type of file to manage'))
        self.mime_type=None
        self.test_file=None

    def set_from_type(self, mtype):
        self.mime_type=mtype
        self.set_image(mtype.get_icon())
        txt=mtype.get_comment() or str(mtype)
        self.set_message(txt)
        
    def set_from_file(self, path):
        self.test_file=path
        self.set_from_type(rox.mime.get_type(path))

    def get_type(self):
        return self.mime_type
    def get_test(self):
        return self.test_file

class HandlerDropTarget(DropTarget):
    def __init__(self, parent):
        DropTarget.__init__(self, parent, _('Handler'))
        self.handler=None
        
    def set_from_file(self, path):
        self.handler=path
        if rox.isappdir(path):
            try:
                p=os.path.join(path, '.DirIcon')
                self.img.set_from_file(p)
            except:
                self.img.set_from_stock(rox.g.STOCK_DIALOG_QUESTION,
                                            rox.g.ICON_SIZE_DIALOG)
        else:
            try:
                mtype=rox.mime.get_type(path)
                icon=mtype.get_icon(img_size)
                self.img.set_from_pixbuf(icon)
            except:
                self.img.set_from_stock(rox.g.STOCK_DIALOG_QUESTION,
                                        rox.g.ICON_SIZE_DIALOG)
				
        if os.path.islink(path):
            p=os.readlink(path)
            self.msg.set_text(os.path.basename(p))
        elif rox.isappdir(path):
            self.msg.set_text(os.path.basename(path))
        else:
            self.msg.set_text('')

    def get_handler(self):
        return self.handler
               

class SetWindow(rox.Dialog):
    def __init__(self, path=None):
        rox.Dialog.__init__(self, title=_('Define thumbnail helper'),
                            buttons=(rox.g.STOCK_CLOSE, rox.g.RESPONSE_ACCEPT))
        
        self.ttips=rox.g.Tooltips()

        ebox=rox.g.EventBox()
        self.vbox.pack_start(ebox, False, False)

        self.type_drop=TypeDropTarget(self)
        ebox=rox.g.EventBox()
        ebox.add(self.type_drop)
        self.vbox.pack_start(ebox, True, True)

        self.handler_drop=HandlerDropTarget(self)
        ebox=rox.g.EventBox()
        ebox.add(self.handler_drop)
        self.vbox.pack_start(ebox, True, True)

        self.vbox.show_all()
        but=rox.g.Button(stock=rox.g.STOCK_HELP)
        but.connect('clicked', self.do_help)
        self.action_area.pack_start(but, False, False)
        self.action_area.reorder_child(but, 0)
        but.show()
        
        self.connect('response', self.get_response)
        
        if path:
            self.set_file(path)

    def get_response(self, window, response):
        self.hide()
        self.destroy()

    def do_help(self, widget):
        helper.show_help(parent=self)

    def file_dropped(self, target, path):
        if target==self.type_drop:
            self.set_file(path)
            self.set_type(self.type_drop.get_type())

        elif target==self.handler_drop:
            self.handler_drop.set_from_file(path)
            if self.type_drop.get_type():
                gobject.idle_add(self.start_test)

    def set_type(self, mtype):
        pass # for future expansion

    def set_file(self, path):
        self.type_drop.set_from_file(path)
        handler=find_handler(self.type_drop.get_type())
        if handler:
            self.handler_drop.set_from_file(handler)
        else:
            self.handler_drop.clear_image()
            self.handler_drop.set_message('')

    def start_test(self):
        tester=TestWindow(self.type_drop.get_test(),
                              self.handler_drop.get_handler())
        try:
            resp=tester.run()
            #print resp
        except:
            rox.report_exception()
            resp=rox.g.RESPONSE_CANCEL

        tester.destroy()
        if resp==rox.g.RESPONSE_ACCEPT:
            if self.set_handler():
                self.response(rox.g.RESPONSE_ACCEPT)

    def set_handler(self):
        mtype=self.type_drop.get_type()
        base='%s_%s' % (mtype.media, mtype.subtype)
        handler=self.handler_drop.get_handler()
        rox.alert('would set handler for %s to %s' % (mtype, handler))

        try:
            cdir=rox.basedir.save_config_path('rox.sourceforge.net',
                                              'MIME-thumb')
            path=os.path.join(cdir, base)
            tmp=path+'.tmp%d' % os.getpid()
            os.symlink(handler, tmp)
            if os.access(path, os.F_OK):
                os.remove(path)
            os.rename(tmp, path)
        except:
            rox.report_exception()
            return False
        
        return True
                                                      

class TestWindow(rox.Dialog,):
    def __init__(self, test_file, handler):
        rox.Dialog.__init__(self, title=_('Testing handler'),
                            buttons=(
            rox.g.STOCK_CANCEL, rox.g.RESPONSE_CANCEL))

        self.test_file=test_file
        self.handler=handler
        fd, name=tempfile.mkstemp('.png')
        os.close(fd)
        self.output=name
        self.errors=[]

        vbox=self.vbox

        msg=rox.g.Label(_('Testing %s') % self.handler)
        msg.set_line_wrap(True)
        vbox.pack_start(msg, False, False, 2)

        icon=rox.g.Image()
        icon.set_size_request(128, 128)
        vbox.pack_start(icon, True, True, 2)
        self.icon=icon

        lbl=rox.g.Label('')
        vbox.pack_start(lbl, False, False, 2)
        self.message=lbl

        vbox.show_all()
        self.connect('show', self.on_show)

        self.ok_button=self.add_button(rox.g.STOCK_OK, rox.g.RESPONSE_ACCEPT)
        self.ok_button.set_sensitive(False)

    def on_show(self, widget):
        ex=self.handler
        if rox.isappdir(ex):
            ex=os.path.join(ex, 'AppRun')
        #print ex, os.path.basename(self.handler),
        #print self.test_file,
        #print self.output, 128
        self.pid=os.spawnl(os.P_NOWAIT,
                           ex, os.path.basename(self.handler),
                           self.test_file,
                           self.output, '%d' % 128)
        gobject.timeout_add(1000, self.check_child)

    def on_show_old(self, widget):
        # USing rox.processes causes mplayer to enter STOP state!
        self.thumb=Thumbnail(self, 128)
        self.thumb.start()
        gobject.timeout_add(60*1000, self.timed_out)

    def child_died(self, status):
        if status==0:
            self.icon.set_from_file(self.output)
            self.ok_button.set_sensitive(True)
            self.message.set_text(_('Click OK to set this handler'))
        else:
            raise ThumbnailError(''.join(self.errors))
        os.remove(self.output)

    def got_error_output(self, data):
        self.message.set_text(data)
        self.errors.append(data)
        
    def timed_out(self):
        self.thumb.kill()
        self.message.set_text(_('Thumbnail process timed out'))

    def check_child(self):
        pid, status=os.waitpid(self.pid, os.WNOHANG)
        #print pid, status
        if pid==self.pid:
            self.child_died(status)
            return False
        return True
    
class Thumbnail(rox.processes.Process):
    def __init__(self, parent, size):
        self.parent=parent
        self.size=size
        rox.processes.Process.__init__(self)
        
    def child_run(self):
        ex=self.parent.handler
        if rox.isappdir(ex):
            ex=os.path.join(ex, 'AppRun')
        print ex, os.path.basename(self.parent.handler)
        print self.parent.test_file
        print self.parent.output, '%d' % self.size
        os.execl(ex, os.path.basename(self.parent.handler),
                 self.parent.test_file,
                 self.parent.output, '%d' % self.size)
        os._exit(99)

    def child_died(self, status):
        self.parent.child_died(status)

    def got_error_output(self, data):
        self.parent.got_error_output(data)

    def parent_post_fork(self):
        print 'in parent_post_fork'


def find_handler(mtype):
    base='%s_%s' % (mtype.media, mtype.subtype)
    handler=rox.basedir.load_first_config('rox.sourceforge.net',
                                          'MIME-thumb', base)
    if not handler:
        handler=rox.basedir.load_first_config('rox.sourceforge.net',
                                          'MIME-thumb', mtype.media)
    return handler

def clean_up(dia, resp):
    dia.destroy()

def show(files=None):
    if files:
        for f in files:
            win=SetWindow(os.path.realpath(f))
            win.connect('response', clean_up)
            win.show()
    else:
        win=SetWindow()
        win.connect('response', clean_up)
        win.show()
