#!/usr/bin/env python
#
# $Id: vidthumb.py,v 1.10 2005/02/19 11:54:32 stephen Exp $

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
import rox, rox.mime

import thumb

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

debug=os.environ.get('VIDTHUMB_DEBUG', 0)

class VidThumb(thumb.Thumbnailler):
    """Generate thumbnail for video files understood by mplayer"""
    def __init__(self, debug=False):
        """Initialize Video thumbnailler"""
        thumb.Thumbnailler.__init__(self, 'VideoThumbnail', 'vidthumb',
                                    True, debug)

    def post_process_image(self, img, w, h):
        """Add the optional film strip effect"""

        if not options.sprocket.int_value and not options.time_label.int_value:
            return img
        
        pixmap, mask=img.render_pixmap_and_mask()
        cmap=pixmap.get_colormap()
        gtk=rox.g
        gc=pixmap.new_gc(foreground=cmap.alloc_color('black'))
            
        if options.sprocket.int_value:
            # Draw the film strip effect
            pixmap.draw_rectangle(gc, gtk.TRUE, 0, 0, 8, h)
            pixmap.draw_rectangle(gc, gtk.TRUE, w-8, 0, 8, h)

            gc.set_foreground(cmap.alloc_color('#DDD'))
            for y in range(1, h, 8):
                pixmap.draw_rectangle(gc, gtk.TRUE, 2, y, 4, 4)
                pixmap.draw_rectangle(gc, gtk.TRUE, w-8+2, y, 4, 4)

        if options.time_label.int_value and self.total_time:
            secs=self.total_time
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
            
            gc.set_foreground(cmap.alloc_color('black'))
            pixmap.draw_layout(gc, 11, 5, layout)
            gc.set_foreground(cmap.alloc_color('white'))
            pixmap.draw_layout(gc, 10, 4, layout)
            if debug: print layout
                               
        return img.get_from_drawable(pixmap, cmap, 0, 0, 0, 0, -1, -1)

    def failed_image(self, rsize, tstr):
        #print 'failed_image', self, rsize
        w=rsize
        h=rsize/4*3
        try:
            p=rox.g.gdk.Pixbuf(rox.g.gdk.COLORSPACE_RGB, False, 8, w, h)
        except:
            sys.exit(2)
        #print p

        pixmap, mask=p.render_pixmap_and_mask()
        cmap=pixmap.get_colormap()
        gc=pixmap.new_gc(foreground=cmap.alloc_color('black'))
            
        pixmap.draw_rectangle(gc, rox.g.TRUE, 0, 0, w, h)
        
        gc.set_foreground(cmap.alloc_color('red'))
        dummy=rox.g.Window()
        layout=dummy.create_pango_layout(tstr)
        if w>40:
            layout.set_width(w-20)
        pixmap.draw_layout(gc, 10, 4, layout)
        
        return img.get_from_drawable(pixmap, cmap, 0, 0, 0, 0, -1, -1)

    def get_image(self, inname, rsize):
        """Generate the raw image from the file.  We run mplayer (twice)
        to do the hard work."""
        def get_length(fname):
            """Get the length in seconds of the source. """
            # -frames 0 might be needed on debian systems
            unused, inf, junk=os.popen3(
                'mplayer -frames 0 -vo null -ao null -identify "%s"' % fname,
                'r')

            for l in inf.readlines():
                # print l[:10]
                if l[:10]=='ID_LENGTH=':
                    return float(l.strip()[10:])

        def write_frame(fname, pos):
            """Return filename of a single frame from the source, taken 
            from pos seconds into the video"""

            # Ask for 2 frames.  Seems to work better
            cmd='mplayer -really-quiet -vo png -z 5 -ss %f -frames 2 -nosound -noloop "%s"' % (pos, fname)
            cmd+=' > /dev/null 2>&1'

            # If we have 2 frames ignore the first and return the second, else
            # if we have 1 return it.  Otherwise mplayer couldn't cope and we
            # return None
            def frame_ok(ofile):
                try:
                    os.stat(ofile)
                except:
                    return False
                return True
                
            os.system(cmd)

            mtype=rox.mime.get_type(fname)
            #print >>sys.stderr, mtype, first_by_types
            try:
                #print >>sys.stderr, str(mtype)
                first=first_by_types[str(mtype)]
            except:
                #print >>sys.stderr, 'oops'
                first=take_first
            #print >>sys.stderr, first
            
            if first:
                id=1
            else:
                id=2
            ofile='%08d.png' % id
            if not frame_ok(ofile):
                if not first:
                    id=1
                    ofile='%08d.png' % id
                    if not frame_ok(ofile):
                        ofile=None
                else:
                    ofile=None
                
            return ofile

        try:
            vlen=get_length(inname)
        except:
            self.report_exception()
            return self.failed_image(rsize, 'Bad length')

        self.total_time=vlen
        if debug: print vlen
    
        # Select a frame 5% of the way in, but not more than 60s  (Long files
        # usually have a fade in).
        pos=vlen*0.05
        if pos>60:
            pos=60

        frfname=write_frame(inname, pos)
        if frfname is None:
            return self.failed_image(rsize, 'Bad file')

        # Now we load the raw image in

        return rox.g.gdk.pixbuf_new_from_file(frfname)
        
# Process command line args.  Although the filer always passes three args,
# let the last two default to something sensible to allow use outside
# the filer.
def main(argv):
    """Process command line args.  Although the filer always passes three args,
    let the last two default to something sensible to allow use outside
    the filer."""
    global rsize
    
    #print argv
    inname=argv[0]
    try:
        outname=argv[1]
    except:
        pass
    try:
        rsize=int(argv[2])
    except:
        pass
    
    if not os.path.isabs(inname):
        inname=os.path.abspath(inname)

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
    #print inname, outname, rsize

    thumb=VidThumb(options.report.int_value)
    thumb.run(inname, outname, rsize)

def configure():
    """Configure the app"""
    options.edit_options()
    rox.mainloop()

if __name__=='__main__':
    main(sys.argv[1:])
