"""Show help files"""

# $Id: helper.py,v 1.1 2008/01/28 17:02:52 stephen Exp $

import os, sys
import rox, rox.filer, rox.mime, rox.AppInfo, rox.uri_handler

text_html=rox.mime.lookup('text/html')
inode_directory=rox.mime.lookup('inode/directory')

class HelpWindow(rox.Dialog):
    def __init__(self, app, fname, parent=None):
        rox.Dialog.__init__(self, _('Help for %s') % app,
                            parent, rox.g.DIALOG_DESTROY_WITH_PARENT,
                            buttons=(rox.g.STOCK_OK, rox.g.RESPONSE_ACCEPT))

        try:
            ainfo=rox.AppInfo.AppInfo(os.path.join(rox.app_dir, 'AppInfo.xml'))
        except:
            ainfo=None

        lbl=rox.g.Label('<b>%s</b>' % app)
        lbl.set_use_markup(True)
        self.vbox.pack_start(lbl, False, False)

        if ainfo:
            lbl=rox.g.Label(ainfo.getSummary())
            self.vbox.pack_start(lbl, False, False)

            vers=ainfo.getAbout('Version')
            if vers:
                lbl=rox.g.Label(vers[1])
                self.vbox.pack_start(lbl, False, False)

        swin=rox.g.ScrolledWindow()
        self.vbox.pack_start(swin)

        swin.set_policy(rox.g.POLICY_NEVER, rox.g.POLICY_AUTOMATIC)
        swin.set_size_request(-1, 200)

        contents=file(fname, 'r').read()
        msg=rox.g.TextView()
        msg.set_editable(False)
        buf=msg.get_buffer()
        buf.set_text(contents)
        swin.add(msg)

        if ainfo:
            home=ainfo.getAbout('Homepage')
            if home:
                but=rox.ButtonMixed(rox.g.STOCK_HOME, home[1])
                but.connect('clicked', self.show_homepage, home[1])
                self.vbox.pack_start(but, False, False)

        self.connect('response', self.get_response)
        self.vbox.show_all()

    def get_response(self, window, response):
        self.hide()
        self.destroy()

    def show_homepage(self, widget, url):
        print self, widget, url
        rox.uri_handler.launch(url)

def run_file(fname):
    rox.filer.spawn_rox((fname,))

def run_dir(fname):
    rox.filer.open_dir(fname)

def run_text(fname, parent):
    #print rox._toplevel_windows, rox._in_mainloops
    HelpWindow(os.path.basename(rox.app_dir), fname, parent).show()
    #print rox._toplevel_windows, rox._in_mainloops

def find_file(fname, lang):
    hdir=os.path.join(rox.app_dir, 'Help')
    
    if lang:
        base, ext=os.path.splitext(fname)
        p=os.path.join(hdir, '%s-%s%s' % (base, lang, ext))
        if os.access(p, os.R_OK):
            return p, rox.mime.get_type(p)

        if '.' in lang:
            rlang, chars=lang.split('.', 1)
            p=os.path.join(hdir, '%s-%s%s' % (base, rlang, ext))
            if os.access(p, os.R_OK):
                return p, rox.mime.get_type(p)

        if '_' in lang:
            blang, tail=lang.split('_', 1)
            p=os.path.join(hdir, '%s-%s%s' % (base, blang, ext))
            if os.access(p, os.R_OK):
                return p, rox.mime.get_type(p)

    p=os.path.join(hdir, base)
    if os.access(p, os.R_OK):
        return p, rox.mime.get_type(p)

    return None, None

def show_help(fname='README', lang=None, parent=None):
    if lang is None:
        lang=os.getenv('LANG')

    path, mime_type=find_file(fname, lang)
    #print path, mime_type

    if mime_type==text_html:
        run_file(path)
    elif mime_type==inode_directory:
        run_dir(path)
    elif mime_type.media=='text':
        run_text(path, parent)
    else:
        run_file(path)

if __name__=='__main__':
    show_help()
