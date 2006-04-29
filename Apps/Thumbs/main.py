# $Id: main.py,v 1.4 2005/05/07 11:37:22 stephen Exp $

import findrox; findrox.version(2, 0, 0)

import os, sys
import stat
import time
import urlparse
import rox, rox.filer
import gobject

__builtins__._ = rox.i18n.translation(os.path.join(rox.app_dir, 'Messages'))

NAME=0
COUNT=1
SIZE=2
PATH=3

thumbdir=os.path.join(os.environ['HOME'], '.thumbnails')

def size_str(size):
    if size>4<<20:
        return '%d M' % (size>>20)
    elif size>4<<10:
        return '%d K' % (size>>10)
    return '%d' % size

def get_thumb_state(path):
    thumb=rox.g.gdk.pixbuf_new_from_file(path)
    uri=thumb.get_option('tEXt::Thumb::URI')
    mtime=thumb.get_option('tEXt::Thumb::MTime')
    #print uri
    return (uri, float(mtime))

def local_name(path):
    scheme, loc, path, query, frag=urlparse.urlsplit(path, 'file', 0)
    #print scheme, loc, path
    if scheme=='file' or loc=='':
        return path

class Win(rox.Window):
    def __init__(self):
        rox.Window.__init__(self)

        self.dirs=[]
        self.sbuttons=[]

        self.set_title(_('Thumbnails'))

        vbox=rox.g.VBox()
        self.add(vbox)

        vbox.pack_start(rox.g.Label(_('Thumbnail directories')))

        swin=rox.g.ScrolledWindow()
        swin.set_size_request(256, 128)
        swin.set_policy(rox.g.POLICY_NEVER, rox.g.POLICY_AUTOMATIC)
        vbox.pack_start(swin, True, True, 0)

        self.store=rox.g.ListStore(str, str, str, str)
        self.view=rox.g.TreeView(self.store)

        cell=rox.g.CellRendererText()
        col=rox.g.TreeViewColumn(_('Name'), cell, text=NAME)
        col.set_sort_column_id(NAME)
        self.view.append_column(col)

        cell=rox.g.CellRendererText()
        col=rox.g.TreeViewColumn(_('Items'), cell, text=COUNT)
        col.set_sort_column_id(COUNT)
        self.view.append_column(col)

        cell=rox.g.CellRendererText()
        col=rox.g.TreeViewColumn(_('Size'), cell, text=SIZE)
        col.set_sort_column_id(SIZE)
        self.view.append_column(col)

        sel=self.view.get_selection()
        sel.set_mode(rox.g.SELECTION_MULTIPLE)
        def do_check(sel, self):
            self.check_selection()
        sel.connect('changed', self.check_selection)

        swin.add(self.view)

        hbox=rox.g.HBox()
        vbox.pack_start(hbox)
        
        but=rox.ButtonMixed(rox.g.STOCK_OPEN, _('Show'))
        but.connect('clicked', self.show_dirs)
        hbox.pack_start(but, False, False)
        self.sbuttons.append(but)

        hbox=rox.g.HBox()
        vbox.pack_start(hbox)
        
        hbox.pack_start(rox.g.Label(_('Age (days)')))

        adj=rox.g.Adjustment(7, 1, 365, 1, 10, 10)
        self.age=rox.g.SpinButton(adj)
        hbox.pack_start(self.age)

        but=rox.ButtonMixed(rox.g.STOCK_DELETE, _('Purge old'))
        but.connect('clicked', self.purge_by_age)
        hbox.pack_start(but)
        self.sbuttons.append(but)

        hbox=rox.g.HBox()
        vbox.pack_start(hbox)
        
        but=rox.ButtonMixed(rox.g.STOCK_DELETE, _('Purge missing'))
        but.connect('clicked', self.purge_missing)
        hbox.pack_start(but)
        self.sbuttons.append(but)

        but=rox.ButtonMixed(rox.g.STOCK_DELETE, _('Purge changed'))
        but.connect('clicked', self.purge_changed)
        hbox.pack_start(but)
        self.sbuttons.append(but)

        hbox=rox.g.HBox()
        vbox.pack_start(hbox)
        
        self.stat=rox.g.Label('')
        hbox.pack_start(self.stat)

        vbox.show_all()

        def on_show(widget):
            gobject.idle_add(widget.scan_dirs)

        self.connect('show', on_show)

    def scan_dir(self, dir):
        files=os.listdir(dir)
        l=len(files)
        size=0
        for f in files:
            try:
                s=os.stat(os.path.join(dir, f))
                size+=s.st_size
            except:
                pass
        return l, size

    def scan_dirs(self):
        self.stat.set_text(_('Scanning directories'))
        self.store.clear()
        items=os.listdir(thumbdir)
        items.sort()
        self.dirs=[]
        for item in items:
            path=os.path.join(thumbdir, item) 
            try:
                s=os.lstat(path)
                if stat.S_ISDIR(s.st_mode):
                    self.dirs.append(path)
            except:
                pass

        for dir in self.dirs:
            l, size=self.scan_dir(dir)
            iter=self.store.append()
            self.store.set(iter, NAME, os.path.basename(dir),
                           COUNT, '%5d' % l, SIZE, size_str(size),
                           PATH, dir)
        self.check_selection()
        self.stat.set_text('')
            
    def check_selection(self, ignored=None):
        sel=self.view.get_selection()
        self.count=0
        def sinc(model, path, iter, obj):
            obj.count+=1
        sel.selected_foreach(sinc, self)
        for but in self.sbuttons:
            but.set_sensitive(self.count)

    def catalog_selected(self):
        sel=self.view.get_selection()
        dirs=[]
        def get_dirs(model, path, iter, dirs):
            #print model, iter
            d=model.get_value(iter, PATH)
            dirs.append(d)
        sel.selected_foreach(get_dirs, dirs)
        paths=[]
        for d in dirs:
            files=os.listdir(d)
            for f in files:
                paths.append(os.path.join(d, f))
        return paths


    def show_dirs(self, ignore=None):
        sel=self.view.get_selection()
        def get_dirs(model, path, iter, dat):
            #print model, iter
            d=model.get_value(iter, PATH)
            #os.system('rox %s' % d)
            rox.filer.open_dir(d)
        sel.selected_foreach(get_dirs, None)

    def scan_selected(self):
        sel=self.view.get_selection()
        def rescan(model, path, iter, ignored):
            #print model, iter
            dir=model.get_value(iter, PATH)
            l, size=self.scan_dir(dir)
            model.set(iter, COUNT, l, SIZE, size_str(size))
        sel.selected_foreach(rescan, None)

    def do_delete(self, path, uri, mtime):
        self.stat.set_text(uri)
        try:
            os.remove(path)
        except:
            return 0
        return 1

    def purge(self, test, dat=None):
        paths=self.catalog_selected()
        #print len(paths), paths[0]
        count=0
        failed=0
        i=0
        for p in paths:
            i+=1
            if i%20==0:
                while rox.g.events_pending():
                    rox.g.main_iteration_do(0)
            try:
                uri, mtime=get_thumb_state(p)
            except:
                #print i, p, sys.exc_info()[:2]
                rox.report_exception()
                failed+=1
                continue
            if test(uri, mtime, dat):
                if self.do_delete(p, uri, mtime):
                    count+=1
                else:
                    failed+=1
        self.scan_selected()
        if failed==0:
            self.stat.set_text(_('%d checked, %d purged') % (i, count))
        else:
            self.stat.set_text(_('%d checked, %d purged (%d failed)') %
                               (i, count, failed))

    def purge_by_age(self, ignored=None):
        age=self.age.get_value()*24*60*60
        now=time.time()
        safe=now-age
        def age_test(uri, mtime, safe):
            if mtime<safe:
                #print mtime, safe, safe-mtime
                return 1
            return 0
        self.purge(age_test, safe)
        
    def purge_missing(self, ignored=None):
        def missing_test(uri, mtime, dat):
            path=local_name(uri)
            #print path
            if path is None or os.access(path, os.F_OK)==0:
                return 1
            return 0
        self.purge(missing_test)
        
    def purge_changed(self, ignored=None):
        def changed_test(uri, mtime, dat):
            path=local_name(uri)
            #print path
            if path is None or os.access(path, os.F_OK)==0:
                return 1
            s=os.stat(path)
            if s is None or s.st_mtime>mtime:
                return 1
            return 0
        self.purge(changed_test)
        
if __name__=='__main__':
    try:
        Win().show()
        rox.mainloop()
    except:
        rox.show_exception()
        
