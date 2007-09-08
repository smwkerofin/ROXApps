# $Id$

import os
import rox

icon_path=os.path.join(rox.app_dir, '.DirIcon')
icon=rox.g.gdk.pixbuf_new_from_file(icon_path)
sicon=icon.scale_simple(16, 16, rox.g.gdk.INTERP_BILINEAR)

class StatusIcon(rox.g.StatusIcon):
    def __init__(self, icon_img=None, icon_menu=None):
        rox.g.StatusIcon.__init__(self)

        if not icon_img:
            icon_img=sicon
        self.set_from_pixbuf(icon_img)
        self.set_visible(False)

        self.icon_menu=icon_menu

        rox.toplevel_ref()
        #self.connect('destroy', rox.toplevel_unref)
        self.connect('activate', self.on_activate)
        self.connect('popup-menu', self.on_popup_menu)


    def on_activate(self, icon):
        pass

    def on_popup_menu(self, icon, button, act_time):
        #print 'do_menu', icon, button, act_time
        def pos_menu(menu):
            return rox.g.status_icon_position_menu(menu, self)
        if self.icon_menu:
            self.icon_menu.popup(self, None, pos_menu)

