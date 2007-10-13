import os, sys
import errno

import rox
import gtk.glade as glade

def _get_templates_file_name(fname):
    if not fname:
        fname='templates.glade'
    if not os.path.isabs(fname):
        fname=os.path.join(rox.app_dir, fname)
    return fname

def _wrap_window(win):
    if not win.get_data('rox_toplevel_ref'):
        rox.toplevel_ref()
        win.connect('destroy', rox.toplevel_unref)
        win.set_data('rox_toplevel_ref', True)
    return win

class Templates:
    def __init__(self, name=None):
        fname=_get_templates_file_name(name)

        self.xml=file(fname, 'r').read()
        self.connect_to=None
        self.signals={}

    def autoConnect(self, dict_or_instance):
        self.connect_to=dict_or_instance

    def connect(self, handler_name, func):
        self.signals[handler_name]=func

    def getWidgetSet(self, root=''):
        widgets=WidgetSet(self.xml, root)
        if self.connect_to:
            widgets.autoConnect(self.connect_to)
        for name in self.signals:
            widgets.connect(name, self.signals[name])
        return widgets

class WidgetSet:
    def __init__(self, xml, root=''):
        self.widgets=glade.xml_new_from_buffer(xml, len(xml), root)

    def autoConnect(self, dict_or_instance):
        self.widgets.signal_autoconnect(dict_or_instance)

    def connect(self, name, func):
        self.widgets.signal_connect(name, func)

    def getWidget(self, name):
        return self.widgets.get_widget(name)

    def getWindow(self, name):
        return _wrap_window(self.getWidget(name))

    def __getitem__(self, key):
        widget=self.widgets.get_widget(key)
        if not widget:
            raise IndexError
        return widget

def load(fname=None, root='', dict_or_instance=None):
    template=Templates(fname)
    if dict_or_instance:
        template.autoConnect(dict_or_instance)
    return template.getWidgetSet(root)
