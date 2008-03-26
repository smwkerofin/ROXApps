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
        rox.loading.XDSLoader.__init__(self, ['text/x-moz-url'])

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

    def xds_load_from_stream(self, name, mimetype, stream):
        #print name, mimetype, stream
        if mimetype=='text/x-moz-url':
            l=stream.readline()
            self.controller.url_dropped(self, l.strip().decode('UTF-16'))
        else:
            rox.loading.XDSLoader.xds_load_from_stream(self, name,
                                                       mimetype, stream)

class TypeDropTarget(DropTarget):
    def __init__(self, parent):
        DropTarget.__init__(self, parent, _('Type of file to manage'))
        self.mime_type=None
        self.test_file=None

    def set_from_type(self, mtype):
        self.mime_type=mtype
        #print mtype.get_icon()
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
        self.isurl=False
        
    def set_from_file(self, path):
        self.handler=path
        self.isurl=False
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
        return self.handler, self.isurl

    def set_from_url(self, url):
        self.handler=url
        self.isurl=True
        self.msg.set_text(url)
        mtype=rox.mime.lookup('text', 'x-uri')
        icon=mtype.get_icon(img_size)
        if icon:
            self.img.set_from_pixbuf(icon)
        else:
            self.img.set_from_stock(rox.g.STOCK_DIALOG_QUESTION,
                                    rox.g.ICON_SIZE_DIALOG)

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

    def url_dropped(self, target, url):
        #print url
        if target==self.handler_drop:
            self.handler_drop.set_from_url(url)
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
        handler, isurl=self.handler_drop.get_handler()

        try:
            cdir=rox.basedir.save_config_path('rox.sourceforge.net',
                                              'MIME-thumb')
            path=os.path.join(cdir, base)
            tmp=path+'.tmp%d' % os.getpid()

            if isurl:
                out=file(tmp, 'w')
                out.write('#!/bin/sh\n')
                out.write('exec 0launch %s "$@"\n' % handler)
                out.close()
                os.chmod(tmp, 0755)
            else:
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

        msg=rox.g.Label(_('Testing %s') % self.handler[0])
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
        ex, isurl=self.handler
        if isurl:
            self.pid=os.spawnlp(os.P_NOWAIT,
                           '0launch', '0launch', ex, 
                           self.test_file,
                           self.output, '%d' % 128)
        else:
            if rox.isappdir(ex):
                ex=os.path.join(ex, 'AppRun')
            self.pid=os.spawnl(os.P_NOWAIT,
                               ex, os.path.basename(self.handler[0]),
                               self.test_file,
                               self.output, '%d' % 128)
        gobject.timeout_add(1000, self.check_child)

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
