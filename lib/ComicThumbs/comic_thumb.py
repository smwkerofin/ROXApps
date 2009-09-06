import os, sys
import md5
import rox, rox.mime, rox.thumbnail
import pango

import zipfile

debug=os.environ.get('COMICTHUMB_DEBUG', 0)

class ThumbException(Exception):
    def __init__(self, inname, reason):
        self.inname=inname
        self.reason=reason

    def __str__(self):
        return 'cannot thumbnail "%s": %s' % (self.inname, self.reason)

class ComicThumb(rox.thumbnail.Thumbnailer):
    pvname=('preview', 'cover')
    pvext=('.png', '.jpg')
    
    """Generate thumbnail for comic archives"""
    def __init__(self, use_wdir, debug=False):
        """Initialize comic thumbnailer"""
        rox.thumbnail.Thumbnailer.__init__(self, 'ComicThumb', 'comic_thumb',
                                    use_wdir, debug)

    @classmethod
    def get_thumb(cls, inname):
        mtype=rox.mime.get_type(inname)

        s=str(mtype)
        if s=='application/zip':
            return Zip(debug)

    #def read_jpg(self, inname, rsize, inf):
    #    loader=rox.g.gdk.PixbufLoader

    def find_preview(self, files):
        fname=None
        for name in self.pvname:
            for ext in self.pvext:
                tname=name+ext
                
                if tname in files:
                    if self.debug: print tname
                    return tname

        for name in files:
            for ext in self.pvext:
                if name.endswith(ext):
                    if self.debug: print name
                    return tname

        for name in files:
            ptype=rox.mime.get_type_by_name(name)
            if ptype.media=='image':
                if self.debug: print name
                return name


class Zip(ComicThumb):
    def __init__(self, debug=False):
        ComicThumb.__init__(self, False, debug)
        
    def get_image(self, inname, rsize):
        if not zipfile.is_zipfile(inname):
            raise ThumbException(inname, 'not a valid zip file')

        zipf=zipfile.ZipFile(inname, 'r')
        try:
            files=zipf.namelist()

            fname=self.find_preview(files)
            if not fname:
                return

            ptype=rox.mime.get_type_by_name(fname)
            data=zipf.read(fname)
            
            loader=rox.g.gdk.pixbuf_loader_new_with_mime_type(str(ptype))
            loader.write(data)
            pbuf=loader.get_pixbuf()
            loader.close()

            return pbuf

        finally:
            zipf.close()

    def post_process_image(self, img, w, h):
        mtype=rox.mime.lookup('application/zip')
        zip_icon=mtype.get_icon(rox.mime.ICON_SIZE_LARGE)
        iw=zip_icon.get_width()
        ih=zip_icon.get_height()

        x=w-iw-2
        y=h-ih-2
        if x<0: x=0
        if y<0: y=0

        pixmap, mask=img.render_pixmap_and_mask()
        cmap=pixmap.get_colormap()
        gtk=rox.g
        gc=pixmap.new_gc()

        pixmap.draw_pixbuf(gc, zip_icon, 0, 0, x, y)
            
        return img.get_from_drawable(pixmap, cmap, 0, 0, 0, 0, -1, -1)

outname=None
rsize=128

def main(argv):
    """Process command line args.  Although the filer always passes three args,
    let the last two default to something sensible to allow use outside
    the filer."""
    global rsize, outname
    
    #print argv
    inname=argv[0]
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

    thumb=ComicThumb.get_thumb(inname)
    if thumb:
        thumb.run(inname, outname, rsize)

        
def configure():
    """Configure the app"""
    #options.edit_options()
    #rox.mainloop()
    pass

if __name__=='__main__':
    main(sys.argv[1:])
