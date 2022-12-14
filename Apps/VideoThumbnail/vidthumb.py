#!/usr/bin/env python
#
# $Id: vidthumb.py,v 1.24 2007/11/24 17:28:19 stephen Exp $

"""Generate thumbnails for video files.  This must be called as
      vidthumb.py source_file destination_thumbnail maximum_size
where maximum_size is the maximum width and height.

This uses mplayer to grab a single frame as a .png file, then gdk-pixbuf (via
pygtk 1.99.x) to generate a thumbnail of that frame.  A film strip effect is
added to mark the thumbnail as from a video file.

To use this in ROX-Filer, get a recent version then run VideoThumbnail and
click on "Install handlers".  Generating a video thumbnail takes a lot longer
than a simple image file, but ROX-Filer remains responsive while it is
being processed.
"""

import os, sys
import md5
import rox, rox.mime, rox.thumbnail
import pango

#import thumb
import mpslave

# Defaults
import options

outname=None
rsize=options.tsize.int_value
take_first=options.take_first.int_value

# Width of the film strip effect to put at each side
bwidth=options.ssize.int_value

# How best to select the frame for a given type.  Worked out by trial and error
first_by_types={
    'video/mpeg': True,
    'video/x-ms-wmv': False
}

def binaryInPath(b):
    for path in os.environ["PATH"].split(":"):
        if os.access(os.path.join(path, b), os.R_OK | os.X_OK):
            return True
    return False

def execute_return_err(cmd):
    errmsg=""
    cin, cout=os.popen4(cmd, 'r')
    cin.close()
    for l in cout:
        errmsg+=l
    cout.close()
    #cerr.close()
    if debug: print cmd, errmsg
    if errmsg:
        return errmsg
        
debug=os.environ.get('VIDTHUMB_DEBUG', 0)

class VidThumbNail(rox.thumbnail.Thumbnailer):
    """Generate thumbnail for video files"""
    def __init__(self, debug=False):
        """Initialize Video thumbnailer"""
        rox.thumbnail.Thumbnailer.__init__(self, 'VideoThumbnail', 'vidthumb',
                                    True, debug)

    def failed_image(self, rsize, tstr):
        if debug: print 'failed_image', self, rsize, tstr
        if not tstr:
            tstr=_('Error!')
        w=rsize
        h=rsize/4*3
        try:
            p=rox.g.gdk.Pixbuf(rox.g.gdk.COLORSPACE_RGB, False, 8, w, h)
        except:
            sys.exit(2)
        if debug: print p

        pixmap, mask=p.render_pixmap_and_mask()
        cmap=pixmap.get_colormap()
        gc=pixmap.new_gc(foreground=cmap.alloc_color('black'))
        gc.set_foreground(gc.background)

        if debug: print gc, gc.foreground
        pixmap.draw_rectangle(gc, True, 0, 0, w, h)
        
        gc.set_foreground(cmap.alloc_color('red'))
        dummy=rox.g.Window()
        layout=dummy.create_pango_layout(tstr)
        if w>40:
            layout.set_width((w-10)*pango.SCALE)
            #layout.set_wrap(pango.WRAP_CHAR)
        pixmap.draw_layout(gc, 10, 4, layout)
        if debug: print pixmap

        self.add_time=False
        
        return p.get_from_drawable(pixmap, cmap, 0, 0, 0, 0, -1, -1)

    def check_executable(cls):
        try:
            return binaryInPath(cls._binary)
        except Exception, e:
            if debug:
                print e
            return False
    check_executable = classmethod(check_executable)
        
        

class VidThumbTotem(VidThumbNail):
    """Generate thumbnail for video files understood by totem"""
    _binary = "totem-video-thumbnailer"
    def __init__(self, debug=False):
        """Initialize Video thumbnailler"""
        VidThumbNail.__init__(self, debug)


    def get_image(self, inname, rsize, frac_length=None):
        outfile = os.path.join(self.work_dir, "out.png")
        if options.scale.int_value:
            cmd = 'totem-video-thumbnailer -s %i \"%s\" %s' % (rsize, inname, outfile)
        else:
            cmd = 'totem-video-thumbnailer \"%s\" %s' % (inname, outfile)
        errmsg=execute_return_err(cmd)
        if not os.path.exists(outfile):
            return self.failed_image(rsize, errmsg)

        # Now we load the raw image in
        return rox.g.gdk.pixbuf_new_from_file(outfile)


class VidThumbMPlayer(VidThumbNail):
    """Generate thumbnail for video files understood by mplayer"""
    _binary = "mplayer"
    def __init__(self, debug=False):
        """Initialize Video thumbnailler"""
        VidThumbNail.__init__(self, debug)

        self.add_time=options.time_label.int_value
        self.right_align=options.right_align.int_value

        self.slave=None

    def post_process_image(self, img, w, h, time_to_add=None):
        """Add the optional film strip effect"""

        if not options.sprocket.int_value and not self.add_time:
            return img
        
        pixmap, mask=img.render_pixmap_and_mask()
        cmap=pixmap.get_colormap()
        gtk=rox.g
        gc=pixmap.new_gc(foreground=cmap.alloc_color('black'))
            
        if options.sprocket.int_value:
            # Draw the film strip effect
            pixmap.draw_rectangle(gc, True, 0, 0, 8, h)
            pixmap.draw_rectangle(gc, True, w-8, 0, 8, h)

            gc.set_foreground(cmap.alloc_color('#DDD'))
            for y in range(1, h, 8):
                pixmap.draw_rectangle(gc, True, 2, y, 4, 4)
                pixmap.draw_rectangle(gc, True, w-8+2, y, 4, 4)

        if time_to_add is None:
            time_to_add=self.total_time

        if self.add_time and time_to_add:
            secs=time_to_add
            hours=int(secs/3600)
            if hours>0:
                secs-=hours*3600
            mins=int(secs/60)
            if mins>0:
                secs-=mins*60
            tstr='%d:%02d:%02d' % (hours, mins, secs)
            if debug: print tstr
            dummy=gtk.Window()
            layout=dummy.create_pango_layout(tstr)

            xpos=10
            if self.right_align:
                lw, lh=layout.get_pixel_size()
                xpos=w-xpos-lw
            gc.set_foreground(cmap.alloc_color('black'))
            pixmap.draw_layout(gc, xpos+1, 5, layout)
            gc.set_foreground(cmap.alloc_color('white'))
            pixmap.draw_layout(gc, xpos, 4, layout)
            if debug: print layout
                               
        return img.get_from_drawable(pixmap, cmap, 0, 0, 0, 0, -1, -1)


    def get_image(self, inname, rsize, frac_length=None):
        """Generate the raw image from the file.  We run mplayer as a slave
        to do the hard work."""

        if debug: print self.slave
        try:
            if not self.slave:
                self.slave=mpslave.MPlayer(inname)
            else:
                self.slave.load_file(inname)
        except Exception, exc:
            if debug: print 'slave process failed', exc
            return None
            
        self.total_time=self.slave.length
        if debug: print 'slave says length=',self.total_time

        try:
            pbuf=self.slave.make_frame(frac_length)
        except Exception, exc:
            if debug: print 'slave.make_frame failed', exc
            pbuf=None
        if debug:
            if pbuf is not None:
                print 'pbuf', pbuf, pbuf.get_width(), pbuf.get_height()
            else:
                print 'pbuf', pbuf
            
        #if not pbuf:
        #    return self.failed_image(rsize, _('Could not get frame'))
        return pbuf

thumbnailers = {"mplayer": VidThumbMPlayer,
                "totem" : VidThumbTotem }

def get_generator(debug=None):
    """Return the current generator of thumbnails"""
    copy=dict(thumbnailers)
    thumbC = copy.pop(options.generator.value)
    if not thumbC.check_executable():
        origbin = thumbC._binary
        for (name, cls) in copy.iteritems():
            if cls.check_executable():
                thumbC = cls
                msg = _("""VideoThumbnail could not find the program "%s", but another thumbnail generator is available.
                
Should "%s" be used from now on?""")
                if rox.confirm(msg % (origbin, thumbC._binary), rox.g.STOCK_YES):
                    options.generator.value = name
                    rox.app_options.save()
                    pass
                break
        else:
            msg = _("""VideoThumbnail could not find any usable thumbnail generator.
            
You need to install either MPlayer (http://www.mplayerhq.hu)
or Totem (http://www.gnome.org/projects/totem/).""")
            rox.croak(msg)

    if debug is None:
        debug=options.report.int_value
    #print debug
    return thumbC(debug)

def main(argv):
    """Process command line args.  Although the filer always passes three args,
    let the last two default to something sensible to allow use outside
    the filer."""
    global rsize, outname
    
    #print argv
    inname=argv[0]
    if inname.endswith('.dtapart'):
        # Still being downloaded, don't bother
        return
    try:
        outname=argv[1]
    except:
        pass
    if debug: print 'save to', outname
    try:
        rsize=int(argv[2])
    except:
        pass

    orig=inname
    #if os.path.islink(inname):
    #    inname=os.readlink(inname)
    if not os.path.isabs(inname):
        inname=os.path.join(os.path.dirname(orig), inname)

    # Out file name is based on MD5 hash of the URI
    if not outname:
        uri='file://'+inname
        tmp=md5.new(uri).digest()
        leaf=''
        for c in tmp:
            leaf+='%02x' % ord(c)
        outname=os.path.join(os.environ['HOME'], '.thumbnails', 'normal',
                         leaf+'.png')
    elif not os.path.isabs(outname):
        outname=os.path.abspath(outname)
    if debug: print 'save to', outname
    #print inname, outname, rsize

    try:
        thumb=get_generator()
        thumb.run(inname, outname, rsize)
    except IOError, exc:
        sys.stderr.write(str(exc))

        
def configure():
    """Configure the app"""
    options.edit_options()
    rox.mainloop()

if __name__=='__main__':
    main(sys.argv[1:])
