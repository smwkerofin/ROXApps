"""Handle the options for the video thumbnailler"""

# $Id: options.py,v 1.3 2004/01/11 13:21:47 stephen Exp $

import os

import rox
import rox.options
import rox.OptionsBox

rox.setup_app_options('VideoThumbnail')

tsize=rox.options.Option('tsize', 128)
sprocket=rox.options.Option('sprocket', 1)
ssize=rox.options.Option('ssize', 8)
time_label=rox.options.Option('time', 0)
report=rox.options.Option('report', 0)

rox.app_options.notify()

def install_button_handler(*args):
    import rox.mime_handler
    rox.mime_handler.install_from_appinfo()

def build_install_button(box, node, label):
    #print box, node, label
    button = rox.g.Button(label)
    box.may_add_tip(button, node)
    button.connect('clicked', install_button_handler)
    return [button]
rox.OptionsBox.widget_registry['install-button'] = build_install_button

        
def edit_options():
    rox.edit_options()
