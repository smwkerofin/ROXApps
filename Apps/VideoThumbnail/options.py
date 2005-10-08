"""Handle the options for the video thumbnailler"""

# $Id: options.py,v 1.7 2005/04/22 15:24:46 stephen Exp $

import os

import rox
import rox.options
import rox.OptionsBox

rox.setup_app_options('VideoThumbnail', site='kerofin.demon.co.uk')


tsize=rox.options.Option('tsize', 128)
sprocket=rox.options.Option('sprocket', 1)
ssize=rox.options.Option('ssize', 8)
time_label=rox.options.Option('time', 0)
report=rox.options.Option('report', 0)
take_first=rox.options.Option('take_first', False)
generator=rox.options.Option('generator', 'mplayer')
scale=rox.options.Option('scale', False)

rox.app_options.notify()

def install_button_handler(*args):
    import rox.mime_handler
    try:
        rox.mime_handler.install_from_appinfo()
    except:
        rox.report_exception()

def build_install_button(box, node, label):
    #print box, node, label
    button = rox.g.Button(label)
    box.may_add_tip(button, node)
    button.connect('clicked', install_button_handler)
    return [button]
rox.OptionsBox.widget_registry['install-button'] = build_install_button

        
def edit_options():
    rox.edit_options()
