# $Id$

import os, sys
import rox, rox.choices, rox.options, rox.basedir, rox.templates
import rox.OptionsBox
import gobject

import xml.dom, xml.dom.minidom

def _data(node):
	"""Return all the text directly inside this DOM Node."""
	return ''.join([text.nodeValue for text in node.childNodes
			if text.nodeType == xml.dom.Node.TEXT_NODE])

def get_passwords():
    fname=rox.basedir.load_first_config('kerofin.demon.co.uk', 'Fetch',
                                        'passwords.xml')
    if not fname:
        fname=rox.choices.load('Fetch', 'passwords.xml')
    if not fname:
        return {}

    pwds={}
    doc=xml.dom.minidom.parse(fname)
    #print doc.getElementsByTagName('Entry')
    for entry in doc.getElementsByTagName('Entry'):
        host=entry.getAttribute('host')
        realm=entry.getAttribute('realm')

        node=entry.getElementsByTagName('User')[0]
        user=_data(node)
        node=entry.getElementsByTagName('Password')[0]
        password=_data(node)

        pwds[host, realm]=(user, password)

    return pwds

def save_passwords(pwds):
    path=rox.basedir.save_config_path('kerofin.demon.co.uk', 'Fetch')
    fname=os.path.join(path, 'passwords.xml')

    doc=xml.dom.minidom.Document()
    root=doc.createElement('Passwords')
    doc.appendChild(root)

    for key, value in pwds.iteritems():
        host, realm=key
        user, password=value

        node=doc.createElement('Entry')
        node.setAttribute('host', host)
        node.setAttribute('realm', realm)
        
        snode=doc.createElement('User')
        snode.appendChild(doc.createTextNode(user))
        node.appendChild(snode)
        snode=doc.createElement('Password')
        snode.appendChild(doc.createTextNode(password))
        node.appendChild(snode)
        
        root.appendChild(node)

    f=file(fname+'.tmp', 'w')
    doc.writexml(f)
    f.close()
    os.chmod(fname+'.tmp', 0600)
    os.rename(fname+'.tmp', fname)
        
def add_password(pwds, host, realm, user, password):
    pwds[host, realm]=(user, password)
    save_passwords(pwds)

class EditPasswords(rox.templates.ProxyWindow):
    def __init__(self, window, widgets):
        rox.templates.ProxyWindow.__init__(self, window, widgets)
        
        self.pwds=get_passwords()

        for name in ('host_entry', 'realm_entry', 'user_entry',
                     'pw_entry', 'passwds', 'show_pw'):
            w=widgets[name]

            setattr(self, name, w)

        store=rox.g.ListStore(str, str, str, str)
        self.passwds.set_model(store)

        titles=('Host', 'Realm', 'User name', 'Password')
        for i in range(len(titles)):
            cell=rox.g.CellRendererText()
            column=rox.g.TreeViewColumn(titles[i], cell, text=i)
            self.passwds.append_column(column)
        self.pw_col=column

        select=self.passwds.get_selection()
        select.set_mode(rox.g.SELECTION_SINGLE)
        select.connect('changed', self.sel_changed)

        self.pw_col.set_visible(False)

        self.load_passwords()
        self.editing=None

    def on_passwd_edit_response(self, window, response):
        print response, rox.g.RESPONSE_CANCEL, rox.g.RESPONSE_OK
        if response==int(rox.g.RESPONSE_CANCEL):
            self.hide()
            self.destroy()

        elif response==int(rox.g.RESPONSE_OK):
            print 'no save'
            
    def on_add_pw_clicked(self, *unused):
        pass

    def on_change_clicked(self, *unused):
        pass

    def on_show_pw_toggled(self, widget, *unused):
        show=widget.get_active()

        self.pw_col.set_visible(show)
        self.pw_entry.set_visibility(show)

    def load_passwords(self):
        store=self.passwds.get_model()

        for (host, realm), (user, pw) in self.pwds.iteritems():
            #print host, realm, user, pw
            it=store.append()
            store.set(it, 0, host, 1, realm, 2, user, 3, pw)

    def sel_changed(self, select, *unused):
        model, it=select.get_selected()
        self.editing=it

        self.host_entry.set_text(model[it][0])
        self.realm_entry.set_text(model[it][1])
        self.user_entry.set_text(model[it][2])
        self.pw_entry.set_text(model[it][3])


def build_edit_button(box, node, label):
    button = rox.g.Button(label)
    box.may_add_tip(button, node)
    def edit_button_handler(*args):
        edit_passwords()
    button.connect('clicked', edit_button_handler)
    return [button]
rox.OptionsBox.widget_registry['edit-button'] = build_edit_button

def edit_passwords():
    t=rox.templates.load(root='passwd_edit')
    t.get_window('passwd_edit', EditPasswords).show()

