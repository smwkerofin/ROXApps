"""Handle the options for the video thumbnailler"""

# $Id: options.py,v 1.8 2005/10/08 10:57:22 stephen Exp $

import os

import rox
import rox.options
import rox.OptionsBox
import rox.mime_handler
import rox.AppInfo

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
    try:
        if rox.roxlib_version>=(2, 0, 3):
            rox.mime_handler.install_from_appinfo(injint='http://www.kerofin.demon.co.uk/2005/interfaces/VideoThumbnail')
        else:
            try:
                from zeroinstall.injector import basedir
                have_zeroinstall=True
            except:
                have_zeroinstall=False

            in_zeroinstall=False
            if have_zeroinstall:
                for d in basedir.xdg_cache_dirs:
                    if rox._roxlib_dir.find(d)==0 or __file__.find(d)==0:
                        in_zeroinstall=True
                        break

            if in_zeroinstall:
                app_info_path = os.path.join(rox.app_pdir, 'AppInfo.xml')
                ainfo = rox.AppInfo.AppInfo(app_info_path)
                can_thumbnail = ainfo.getCanThumbnail()

                win=rox.mime_handler.InstallList(rox.app_dir,
                                                "thumbnail handler",
                                'MIME-thumb', can_thumbnail,
                                """Thumbnail handlers provide support for creating thumbnail images of types of file.  The filer can generate thumbnails for most types of image (JPEG, PNG, etc.) but relies on helper applications for the others.""")

                if win.run()!=rox.g.RESPONSE_ACCEPT:
                    win.destroy()
                    return

                try:
                    types=win.get_active()

                    for tname in types:
                        mime_type = mime.lookup(tname)

                        sname=rox.mime_handler.save_path('rox.sourceforge.net',
                                                         'MIME-thumb',
			      '%s_%s' % (mime_type.media, mime_type.subtype))
                        
                        tmp=sname+'.tmp%d' % os.getpid()
                        f=file(tmp, 'w')
                        f.write('#!/bin/sh\n\n')
                        f.write('0launch http://www.kerofin.demon.co.uk/2005/interfaces/VideoThumbnail\n')
                        f.close()
                        os.chmod(tmp, 0755)
                        os.symlink(app_dir, tmp)
                        os.rename(tmp, path)

                    types=win.get_uninstall()

                    for tname in types:
                        mime_type = mime.lookup(tname)
                        
                        sname=rox.mime_handler.save_path('rox.sourceforge.net',
                                                         'MIME-thumb',
                                        '%s_%s' % (mime_type.media, mime_type.subtype))
                        os.remove(sname)
                finally:
                    win.destroy()
                    
            else:
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
