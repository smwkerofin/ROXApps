"""Handle the options for the video thumbnailler"""

# $Id: options.py,v 1.2 2003/11/08 12:19:46 stephen Exp $

import os

import rox
import rox.options
import rox.OptionsBox

rox.setup_app_options('VideoThumbnail')

tsize=rox.options.Option('tsize', 128)
sprocket=rox.options.Option('sprocket', 1)
ssize=rox.options.Option('ssize', 8)
install=rox.options.Option('install', 0)

process_install=False

def check_changed():
    if install.has_changed and process_install:
        try:
            do_install()
        except:
            rox.report_exception()

rox.app_options.add_notify(check_changed)
rox.app_options.notify()

options_box=None

class Options(rox.OptionsBox.OptionsBox):
    """Extension to the OptionsBox to provide a button widget"""
    def build_button(self, node, label, option):
        "<button name='...' label='...'>Tooltip</sectryentry>"

        button=rox.g.Button(label)

        self.may_add_tip(button, node)

        def noop():
            pass

        button.connect('clicked', lambda e: self.check_widget(option))
        self.handlers[option] = (noop, noop)

        return [button]
        
def edit_options():
    global options_box, process_install
    if options_box:
        options_box.present()
        return

    options_file = os.path.join(rox.app_dir, 'Options.xml')

    try:
        options_box=Options(rox.app_options, options_file)
        options_box.connect('destroy', options_closed)
        process_install=True
        options_box.open()
    except:
        rox.report_exception()
        options_box = None

def options_closed(widget):
    global options_box
    assert options_box == widget
    options_box = None

def do_install():
    import rox.mime_handler
    rox.mime_handler.install_from_appinfo()
    
