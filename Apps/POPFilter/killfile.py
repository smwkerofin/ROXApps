# $Id$

import sys, os
import re

import rox
import rox.choices
from xml.dom import Node, minidom

ADD=1
DELETE=2
EDIT=3
SAVE=4

g=rox.g

class KillFileEdit(rox.Dialog):
    def __init__(self, control):
        rox.Dialog.__init__(self)
        self.set_title('Edit kill file for %s' % control.account)
        self.set_default_size(400, 300)
        
        self.control=control
        self.editing=None

        self.add_button(g.STOCK_ADD, ADD)
        self.add_button(g.STOCK_DELETE, DELETE)
        self.add_button(g.STOCK_PROPERTIES, EDIT)
        self.add_button(g.STOCK_CLOSE, g.RESPONSE_CANCEL)
        self.add_button(g.STOCK_SAVE, SAVE)

        self.set_default_response(SAVE)

        swin = g.ScrolledWindow()
        swin.set_border_width(4)
        swin.set_policy(g.POLICY_NEVER, g.POLICY_ALWAYS)
        swin.set_shadow_type(g.SHADOW_IN)
        self.vbox.pack_start(swin, True, True, 0)

        self.model = g.ListStore(str,)
        view = g.TreeView(self.model)
        self.view = view
        swin.add(view)
        view.set_search_column(1)

        cell = g.CellRendererText()
        column = g.TreeViewColumn('Pattern', cell, text = 0)
        view.append_column(column)
        column.set_sort_column_id(0)

        self.pattern=g.Entry()
        self.pattern.connect('changed', self.update_entry)
        self.vbox.pack_start(self.pattern, g.FALSE, g.FALSE, 0)

        self.vbox.show_all()

        def response(self, resp):
            if resp == EDIT:
                self.edit_selected(view.get_selection())
            elif resp == DELETE:
                self.delete_selected(view)
            elif resp == ADD:
                self.add_new()
            elif resp==SAVE:
                self.save_and_update()
            else:
                self.destroy()
            
        self.connect('response', response)

        def changed(selection):
            model, iter = selection.get_selected()
            self.set_response_sensitive(EDIT, iter != None)
            self.set_response_sensitive(DELETE, iter != None)
        selection = view.get_selection()
        selection.connect('changed', changed)

        self.load_from(self.control)
        changed(selection)

    def delete_selected(self, view):
        model, iter = view.get_selection().get_selected()
        if not iter:
            rox.alert('Nothing selected')
            return

        self.editing=None
        self.model.remove(iter)

    def edit_selected(self, selection):
        model, iter = selection.get_selected()
        if not iter:
            rox.alert('You need to select a pattern to edit first')
            return

        self.editing=iter
        txt=self.model.get_value(iter, 0)
        self.pattern.set_text(txt)

    def add_new(self):
        iter=self.model.append()
        self.model.set(iter, 0, 'pattern')
        selection=self.view.get_selection()
        selection.select_iter(iter)
        self.edit_selected(selection)

    def update_entry(self, wifget):
        if self.editing:
            txt=self.pattern.get_text()
            self.model.set(self.editing, 0, txt)

    def get_patterns(self):
        patterns=[]
        iter=self.model.get_iter_first()
        while iter:
            p=self.model.get_value(iter, 0)
            patterns.append(p)
            
            iter=self.model.iter_next(iter)

        return patterns

    def save_and_update(self):
        patterns=self.get_patterns()
        kills=[]
        for p in patterns:
            kills.append(re.compile(p, re.IGNORECASE))

        save('POPFilter', kills)

        self.control.set_killfile(kills)

        self.destroy()
    
    def load_from(self, control):
        #print 'load_from', self, control
        kills=control.get_killfile()
        #print kills

        self.model.clear()
        if kills is None:
            return
        for k in kills:
            iter=self.model.append()
            #print iter, k, k.pattern
            self.model.set(iter, 0, k.pattern)

def data(node):
	"""Return all the text directly inside this DOM Node."""
	return ''.join([text.nodeValue for text in node.childNodes
			if text.nodeType == Node.TEXT_NODE])
    
def load(prog_name):
    fname=rox.choices.load(prog_name, "kill.xml")
    #print fname
    if not fname:
        return []

    doc=minidom.parse(fname)
    #print doc
    if not doc:
        return []

    assert doc.documentElement.localName == 'KillFile'

    kills=[]
    for node in doc.childNodes:
        #print node, node.nodeType, Node.ELEMENT_NODE
        if node.nodeType!=Node.ELEMENT_NODE:
            continue
        if node.localName=='KillFile':
            for subnode in node.childNodes:
                #print subnode
                if subnode.nodeType!=Node.ELEMENT_NODE:
                    continue
                if subnode.localName=='Kill':
                    #print data(subnode)
                    kills.append(re.compile(data(subnode), re.IGNORECASE))

    return kills

def save(prog_name, kills):
    fname=rox.choices.save(prog_name, "kill.xml")
    if not fname:
        return None

    doc=minidom.Document()
    root=doc.createElement('KillFile')
    doc.appendChild(root)

    for k in kills:
        node=doc.createElement('Kill')
        node.appendChild(doc.createTextNode(k.pattern))
        root.appendChild(node)

    f=file(fname, 'w')
    doc.writexml(f)
    f.close()

if __name__=='__main__':
    kills=load('POPFilter')
    for k in kills:
        print k.pattern
    save('POPFilter', kills)
