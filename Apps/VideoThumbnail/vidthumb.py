#!/usr/bin/env python
#
# $Id: vidthumb.py,v 1.1.1.1 2003/10/11 12:15:26 stephen Exp $

"""Generate thumbnails for video files.  This must be called as
      vidthumb.py source_file destination_thumbnail maximum_size
where maximum_size is the maximum width and height.

This uses mplayer to grab a single frame as a .png file, then gdk-pixbuf (via
pygtk 1.99.x) to generate a thumbnail of that frame.  A film strip effect is
added to mark the thumbnail as from a video file.

To use this in ROX-Filer, get a recent CVS snapshot then create a directory
in your Choices directory called MIME-thumb.  Create a link from this file to
Choices/MIME-thumb/video.  Generating a video thumbnail takes a lot longer
than a simple image file, but ROX-Filer remains responsive while it is
being processed.

You might like to use this as a template for other thumbnailers."""

import os, sys
import md5

import rox

# Defaults
import options

outname=None
rsize=options.tsize.int_value

# Width of the film strip effect to put at each side
bwidth=options.ssize.int_value

debug=os.environ.get('VIDTHUMB_DEBUG', 0)

# Process command line args.  Although the filer always passes three args,
# let the last two default to something sensible to allow use outside
# the filer.
def main(argv):
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

    work_dir=os.path.join('/tmp', 'vidthumb.%d' % os.getpid())
    #print work_dir
    try:
        os.makedirs(work_dir)
    except:
        report_exception()

    old_dir=os.getcwd()
    os.chdir(work_dir)

    try:
        vlen=get_length(inname)
    except:
        report_exception()
        vlen=None
    #print vlen
    
    if vlen is None:
        sys.exit(2)

    # Select a frame 5% of the way in, but not more than 60s  (Long files
    # usually have a fade in).
    pos=vlen*0.05
    if pos>60:
        pos=60
    #print pos
    tmp=write_frame(inname, pos)
    if tmp is None:
        try:
            remove_work_dir(work_dir, old_dir)
        except:
            report_exception()
            sys.exit(2)

    # Now we load the raw image in, scale it to the required size and resize,
    # with the required tEXt::Thumb data
    gtk=rox.g

    img=gtk.gdk.pixbuf_new_from_file(tmp)
    #print img
    try:
        remove_work_dir(work_dir, old_dir)
    except:
        report_exception()

    ow=img.get_width()
    oh=img.get_height()
    if ow>oh:
        s=float(rsize)/float(ow)
    else:
        s=float(rsize)/float(oh)
    w=int(s*ow)
    h=int(s*oh)
    #print w, h

    img=img.scale_simple(w, h, gtk.gdk.INTERP_BILINEAR)

    pixmap, mask=img.render_pixmap_and_mask()

    # Draw the film strip effect
    if options.sprocket.int_value:
        cmap=pixmap.get_colormap()
        gc=pixmap.new_gc(foreground=cmap.alloc_color('black'))
        pixmap.draw_rectangle(gc, gtk.TRUE, 0, 0, 8, h)
        pixmap.draw_rectangle(gc, gtk.TRUE, w-8, 0, 8, h)

        gc.set_foreground(cmap.alloc_color('#DDD'))
        for y in range(1, h, 8):
            pixmap.draw_rectangle(gc, gtk.TRUE, 2, y, 4, 4)
            pixmap.draw_rectangle(gc, gtk.TRUE, w-8+2, y, 4, 4)

    img=img.get_from_drawable(pixmap, cmap, 0, 0, 0, 0, -1, -1)

    s=os.stat(inname)
    #print s
    img.save(outname+'.vidthumb', 'png', {'tEXt::Thumb::Image::Width': str(ow),
                          'tEXt::Thumb::Image::Height': str(oh),
                          "tEXt::Thumb::Size": str(s.st_size),
                          "tEXt::Thumb::MTime": str(s.st_mtime),
                          'tEXt::Thumb::URI': 'file://'+inname,
                          'tEXt::Software': 'vidthumb.py'})
    os.rename(outname+'.vidthumb', outname)

def report_exception():
    """Report an exception (if debug enabled)"""
    if debug<1:
        return
    exc=sys.exc_info()[:2]
    sys.stderr.write('%s: %s %s\n' % (sys.argv[0], exc[0], exc[1]))
    #print 'reported'
    

def remove_work_dir(work_dir, old_dir):
    """Remove our temporary directory"""
    #print 'remove_work_dir'
    os.chdir(old_dir)
    #print old_dir
    for f in os.listdir(work_dir):
        path=os.path.join(work_dir, f)
        #print path
        try:
            os.remove(path)
        except:
            report_exception()
    #print work_dir
    try:
        os.rmdir(work_dir)
    except:
        report_exception()
    #print 'done'
        
def get_length(fname):
    """Get the length in seconds of the source. """
    # -frames 0 might be needed on debian systems
    unused, inf, junk=os.popen3(
        'mplayer -frames 0 -vo null -ao null -identify "%s"' % fname, 'r')

    for l in inf.readlines():
        # print l[:10]
        if l[:10]=='ID_LENGTH=':
            return float(l.strip()[10:])
    #return 0.

def write_frame(fname, pos):
    """Return filename of a single frame from the source, taken from pos
    seconds into the video"""

    # Ask for 2 frames and take the second.  Seems to work better
    cmd='mplayer -really-quiet -vo png -z 5 -ss %f -frames 2 -nosound "%s"' % (pos, fname)
    cmd+=' > /dev/null 2>&1'

    # If we have 2 frames ignore the first and return the second, else
    # if we have 1 return it.  Otherwise mplayer couldn't cope and we
    # return None
    os.system(cmd)
    ofile='%08d.png' % 2
    try:
        os.stat(ofile)
    except:
        report_exception()
        ofile='%08d.png' % 1

    try:
        os.stat(ofile)
    except:
        report_exception()
        ofile=None
        
    return ofile

def configure():
    options.edit_options()
    rox.mainloop()

if __name__=='__main__':
    main(sys.argv[1:])
