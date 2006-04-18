# $Id: DepWin.py,v 1.3 2004/05/12 18:48:26 stephen Exp $
"""Window"""

import rox
import rox.loading
import rox.filer

import os, stat
import webbrowser

import appinfo, depend

TYPE=0
NAME=1
VERSION=2
STATUS=3
PATH=4
ACTUAL=5
COLOUR=6
DEPEND=7
LINK=8

colours=('white', 'goldenrod', 'red')

class DepWin(rox.Window, rox.loading.XDSLoader):
    def __init__(self, fname=None):
        rox.Window.__init__(self)

        self.set_title(_('Dependencies'))
        self.set_default_size(400, 300)

        vbox=rox.g.VBox()
        self.add(vbox)

        swin = rox.g.ScrolledWindow()
        swin.set_border_width(4)
        swin.set_policy(rox.g.POLICY_NEVER, rox.g.POLICY_ALWAYS)
        swin.set_shadow_type(rox.g.SHADOW_IN)
        vbox.pack_start(swin, True, True, 0)

        self.model = rox.g.ListStore(str, str, str, str, str, str, str,
                                     object, str)
        view = rox.g.TreeView(self.model)
        self.view = view
        swin.add(view)
        view.set_search_column(1)
        view.connect('row-activated', self.activate_row)

        cell = rox.g.CellRendererText()
        column = rox.g.TreeViewColumn(_('Type'), cell, text = TYPE)
        view.append_column(column)
        column.set_sort_column_id(TYPE)
        
        cell = rox.g.CellRendererText()
        column = rox.g.TreeViewColumn(_('Name'), cell, text = NAME)
        view.append_column(column)
        column.set_sort_column_id(NAME)

        cell = rox.g.CellRendererText()
        column = rox.g.TreeViewColumn(_('Version'), cell, text = VERSION)
        view.append_column(column)
        column.set_sort_column_id(VERSION)

        cell = rox.g.CellRendererText()
        column = rox.g.TreeViewColumn(_('Status'), cell, text = STATUS,
                                      background=COLOUR)
        view.append_column(column)
        column.set_sort_column_id(STATUS)

        cell = rox.g.CellRendererText()
        column = rox.g.TreeViewColumn('Path', cell, text = PATH)
        view.append_column(column)
        column.set_sort_column_id(PATH)

        cell = rox.g.CellRendererText()
        column = rox.g.TreeViewColumn(_('Installed version'), cell,
                                      text = ACTUAL)
        view.append_column(column)
        column.set_sort_column_id(ACTUAL)

        cell = rox.g.CellRendererText()
        column = rox.g.TreeViewColumn(_('Web site'), cell, text = LINK)
        view.append_column(column)
        column.set_sort_column_id(LINK)

        vbox.show_all()
        
        rox.loading.XDSLoader.__init__(self, ['text/xml', 'inode/directory'])
        
        if fname:
            self.xds_load_from_file(fname)

    def xds_load_from_file(self, fname):
        try:
            s=os.stat(fname)
            #print fname, s
        except OSError, err:
            #err=sys.exc_info()[1]
            import errno
            if err.errno==errno.ENOENT:
                rox.alert(_('%s not found, cannot check') % fname)
            elif err.errno==errno.EPERM:
                rox.alert(_('%s is not readable') % fname)
            else:
                rox.report_exception()
            return
        except:
            rox.report_exception()
            return

        if stat.S_ISDIR(s.st_mode):
            return self.xds_load_from_file(os.path.join(fname, 'AppInfo.xml'))
        
        try:
            f=file(fname, 'r')
            self.xds_load_from_stream(fname, None, f)
        except:
            rox.report_exception()

    def xds_load_from_stream(self, name, mtype, stream):
        #print name, mtype, stream
        if name:
            self.set_title(_('Dependencies of %s') % name)
        else:
            self.set_title(_('Dependencies'))

        app=appinfo.read_app_info(name, stream)
        #print app
        if app is None:
            rox.alert(_('Failed to read %s') % name)
            self.set_title(_('Dependencies'))
            return

        self.load_app(name, app)

    def load_app(self, pname, app):
        self.model.clear()

        #print app
        #print app.getDependencies()
        deps=app.getDependencies()
        if deps is None:
            rox.info(_('%s has not declared any dependencies') % pname)
            #sys.exit(0)
            return
        elif len(deps)<1:
            rox.info(_('%s claims not to depend on anything') % pname)
            return
        for dep in deps:
            #print dep
            iter=self.model.append()
            type, name, version, state, path, actual, link=dep.info()
            #print type, name, version, state
            #print type, name, version, depend.state_msgs[state]
            self.model.set(iter, TYPE, type, NAME, name, VERSION, version,
                           STATUS, depend.stat_msgs[state],
                           COLOUR, colours[state], DEPEND, dep)
            if path:
                self.model.set(iter, PATH, path)
            if actual:
                self.model.set(iter, ACTUAL, actual)
            if link:
                self.model.set(iter, LINK, link)
            
    def activate_row(self, view, path, col, udata=None):
        #print 'activate_row', view, path, col, udata
        iter=self.model.get_iter(path)
        dep=self.model.get_value(iter, DEPEND)

        where=dep.getLocation()
        if dep.status==depend.OK:
            if where:
                rox.filer.show_file(where)
        elif dep.status==depend.VERSION:
            rox.alert(_('%s is installed on your system at %s, but is an old version (%s < %s).') % (dep.name, where, dep.actual_version, dep.version))
            if where:
                rox.filer.show_file(where)
            link=dep.getLink()
            if link and rox.confirm(_("Do you want to visit the web site '%s'?") %
                                    link, rox.g.STOCK_JUMP_TO):
                webbrowser.open_new(link)
            
        else:
            rox.alert(_("""%s could not be found on your system.  You may need
to set some environment variables to locate it, install it from
your distribution discs or download it from a web site.""") %
                      dep.name)
            link=dep.getLink()
            if link and rox.confirm(_("Do you want to visit the web site '%s'?") %
                                    link, rox.g.STOCK_JUMP_TO):
                webbrowser.open_new(link)
