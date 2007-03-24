import os, sys

import rox, rox.options, rox.OptionsBox

rox.setup_app_options('ROX-Samba', site='kerofin.demon.co.uk')

fuse_device=rox.options.Option('fuse_device', '/dev/fuse')
mount_point=rox.options.Option('mount_point', '~/smb')

def_domain=rox.options.Option('domain', 'TUX-NET')

fusesmb=rox.options.Option('fusesmb_cmd', 'fusesmb')
fusermount=rox.options.Option('fusermount_cmd', 'fusermount')
smbclient=rox.options.Option('smbclient_cmd', 'smbclient')

rox.app_options.notify()

def getMountPoint():
    return os.path.expanduser(mount_point.value)

def getFuseDevice():
    return fuse_device.value

def getDomain():
    return def_domain.value

def getFusesmbCommand():
    return fusesmb.value
def getFusermountCommand():
    return fusermount.value
def getSmbclientCommand():
    return smbclient.value

def show():
    rox.edit_options()
    rox.mainloop()

def addNotify(fn):
    rox.app_options.add_notify(fn)

def build_edit_button(box, node, label):
    button = rox.g.Button(label)
    box.may_add_tip(button, node)
    def edit_button_handler(*args):
        t=rox.templates.load(root='main_window')
        t.getWindow('main_window', ConfigEdit).show()
    button.connect('clicked', edit_button_handler)
    return [button]
rox.OptionsBox.widget_registry['edit-button'] = build_edit_button

import rox.templates
import ConfigParser

dpath='~/.smb/fusesmb.conf'

class ConfigEdit(rox.templates.ProxyWindow):
    def __init__(self, window, widgets, path=dpath):
        rox.templates.ProxyWindow.__init__(self, window, widgets)

        self.widgets=widgets

        self.path=path
        
        self.set_title(_('Editing %s') % self.path)

        vbox=self.widgets['main_vbox']

        self.ignore_server_view=self.widgets['ignore_server_view']
        self.ignore_remove_server=self.widgets['ignore_remove_server']
        self.ignore_server=self.make_list(self.ignore_server_view,
                                           self.ignore_remove_server)

        self.ignore_wg_view=self.widgets['ignore_wg_view']
        self.ignore_remove_wg=self.widgets['ignore_remove_wg']
        self.ignore_wg=self.make_list(self.ignore_wg_view,
                                           self.ignore_remove_wg)

        self.server_name_combo=self.widgets['server_list']
        store=rox.g.ListStore(str)
        self.server_name=store
        self.server_name_combo.set_model(store)
        cell = rox.g.CellRendererText()
        self.server_name_combo.pack_start(cell, True)
        self.server_name_combo.add_attribute(cell, 'text', 0)

        vbox.show_all()

        self.widgets['button2'].hide()
        #self.widgets['button1'].hide()

        self.read_config()

    def on_cancel_clicked(self, *ignored):
        self.hide()
        self.destroy()

    def make_list(self, view, remove):
        store=rox.g.ListStore(str)
        view.set_model(store)

        cell = rox.g.CellRendererText()
        column = rox.g.TreeViewColumn(_('Name'), cell, text = 0)
        view.append_column(column)
        cell.set_property('editable', True)
        cell.connect('edited', self.cell_edited, store, view)

        if remove:
            remove.set_sensitive(False)

        sel=view.get_selection()
        sel.set_mode(rox.g.SELECTION_SINGLE)
        def changed(sel, button):
            model, row=sel.get_selected()
            if row is None:
                button.set_sensitive(False)
            else:
                button.set_sensitive(True)
        if remove:
            sel.connect('changed', changed, remove)
        
        return store

    def cell_edited(self, render, path, new_text, store, view):
        #print self, render, path, new_text, store, view

        it=store.get_iter(path)
        store.set(it, 0, new_text)

    def read_config(self):
        d=os.path.dirname(os.path.expanduser(self.path))
        if not os.path.isdir(d):
            os.makedirs(d)
        self.parser=ConfigParser.ConfigParser()
        self.optionxform=str
        self.parser.read(os.path.expanduser(self.path))

        self.load_widgets()

    def load_widgets(self):

        if self.parser.has_option('global', 'username'):
            self.widgets['uname'].set_text(self.parser.get('global',
                                                              'username'))
        else:
            self.widgets['uname'].set_text('')

        if self.parser.has_option('global', 'password'):
            self.widgets['passwd'].set_text(self.parser.get('global',
                                                              'password'))
        else:
            self.widgets['passwd'].set_text('')

        if self.parser.has_option('global', 'interval'):
            self.widgets['interval'].set_value(self.parser.getint('global',
                                                                  'interval'))
        else:
            self.widgets['interval'].set_value(1)

        if self.parser.has_option('global', 'timeout'):
            self.widgets['timeout'].set_value(self.parser.getint('global',
                                                                  'timeout'))
        else:
            self.widgets['timeout'].set_value(1)

        if self.parser.has_option('global', 'showhiddenshares'):
            self.widgets['show_hidden'].set_active(self.parser.getboolean(
                'global', 'showhiddenshares'))
        else:
            self.widgets['show_hidden'].set_active(False)

        self.ignore_server.clear()
        if self.parser.has_option('ignore', 'servers'):
            servers=self.parser.get('ignore', 'servers').split(',')

            for serv in servers:
                it=self.ignore_server.append()
                self.ignore_server.set(it, 0, serv.strip())

        self.ignore_wg.clear()
        if self.parser.has_option('ignore', 'workgroups'):
            groups=self.parser.get('ignore', 'workgroups').split(',')

            for wg in groups:
                it=self.ignore_wg.append()
                self.ignore_wg.set(it, 0, wg.strip())

        self.current_server=None
        self.server_name.clear()
        for sect in self.parser.sections():
            if sect in ('global', 'ignore'):
                continue
            if sect[0]!='/':
                continue

            if '/' in sect[1:]:
                # A share, ignore for now
                pass

            else:
                # A server

                sname=sect[1:]
                it=self.server_name.append()
                self.server_name.set(it, 0, sname)
        self.server_name_combo.set_active(0)

    def on_server_list_changed(self, combo):
        #print 'on_server_list_changed', self, combo

        if self.current_server:
            self.update_server()

        current_server=combo.child.get_text()
        if current_server:
            sname='/'+current_server.upper()

            if self.parser.has_section(sname):
                self.current_server=current_server
                if self.parser.has_option(sname, 'username'):
                    self.widgets['server_uname'].set_text(
                        self.parser.get(sname, 'username'))
                else:
                    self.widgets['server_uname'].set_text('')
            
                if self.parser.has_option(sname, 'password'):
                    self.widgets['server_passwd'].set_text(
                        self.parser.get(sname, 'password'))
                else:
                    self.widgets['server_passwd'].set_text('')
            
                if self.parser.has_option(sname, 'ignore'):
                    self.widgets['server_ignore'].set_active(
                        self.parser.getboolean(sname, 'ignore'))
                else:
                    self.widgets['server_ignore'].set_active(False)
            
                if self.parser.has_option(sname, 'showhiddenshares'):
                    self.widgets['server_show_hidden'].set_active(
                        self.parser.getboolean(sname, 'showhiddenshares'))
                else:
                    self.widgets['server_ignore'].set_active(False)
                    
            else:
                self.current_server=None

    def update_server(self):
        if not self.current_server:
            return
        sname='/'+self.current_server.upper()
        if not self.parser.has_section(sname):
            self.parser.add_section(sname)

            v=self.widgets['server_uname'].get_text()
            if v:
                self.parser.set(sname, 'username', v)
            v=self.widgets['server_passwd'].get_text()
            if v:
                self.parser.set(sname, 'password', v)
            v=self.widgets['server_ignore'].get_active()
            self.parser.set(sname, 'ignore', str(v).lower())
            v=self.widgets['server_show_hidden'].get_active()
            self.parser.set(sname, 'showhiddenshares', str(v).lower())
        
    def on_server_list_editing_done(self, editable):
        print self, editable

    def on_server_data_changed(self, *ignored):
        print self, ignored
        self.current_server=self.widgets['server_list'].child.get_text()
        found=False
        for s in self.server_name:
            if s[0].upper()==self.current_server.upper():
                found=True
                break
        if not found:
            it=self.server_name.append()
            self.server_name.set(it, 0, self.current_server)

    def on_ok_clicked(self, *ignore):
        self.write_config()

        self.hide()
        self.destroy()

    def write_config(self):
        self.update_config()

        path=os.path.expanduser(self.path)
        ofname=path+'.%05d' % os.getpid()
        back=path+'.bak'

        f=file(ofname, 'w')
        self.parser.write(f)
        f.close()

        os.chmod(ofname, 0600)

        os.rename(path, back)
        os.rename(ofname, path)

    def update_config(self):
        if not self.parser.has_section('global'):
            self.parser.add_section('global')
        self.parser.set('global', 'username', self.widgets['uname'].get_text())
        self.parser.set('global', 'password',
                        self.widgets['passwd'].get_text())
        self.parser.set('global', 'interval',
                        '%d' % self.widgets['interval'].get_value())
        self.parser.set('global', 'timeout',
                        '%d' % self.widgets['timeout'].get_value())
        self.parser.set('global', 'showhiddenshares',
                        str(self.widgets['show_hidden'].get_active()).lower())

        if not self.parser.has_section('ignore'):
            self.parser.add_section('ignore')
        servers=[]
        for ent in self.ignore_server:
            servers.append(ent[0].upper())
        if servers:
            self.parser.set('ignore', 'servers', ','.join(servers))
        wg=[]
        for ent in self.ignore_wg:
            wg.append(ent[0].upper())
        if wg:
            self.parser.set('ignore', 'workgroups', ','.join(wg))

        self.update_server()

    def on_ignore_server_add_clicked(self, button):
        #print 'on_ignore_server_add_clicked'
        it=self.ignore_server.append()
        self.ignore_server.set(it, 0, 'SERVER')
        
    def on_ignore_remove_server_clicked(self, button):
        #print 'on_ignore_remove_server_clicked'
        sel=self.ignore_server_view.get_selection()
        model, row=sel.get_selected()
        if row:
            model.remove(row)
        
    def on_ignore_wg_add_clicked(self, button):
        it=self.ignore_wg.append()
        self.ignore_wg.set(it, 0, 'WORKGROUP')
        
    def on_ignore_remove_wg_clicked(self, button):
        sel=self.ignore_wg_view.get_selection()
        model, row=sel.get_selected()
        if row:
            model.remove(row)
        
