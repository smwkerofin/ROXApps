import os

import findrox; findrox.version(2, 0, 4)
import rox
import rox.loading

import vidthumb

class Window(rox.Window, rox.loading.XDSLoader):
    size=128
    
    def __init__(self, fname=None):
        rox.Window.__init__(self)
        rox.loading.XDSLoader.__init__(self, None)

        tab=rox.g.Table(4, 4)
        self.add(tab)
        self.buttons={}
        for x in range(4):
            for y in range(4):
                self.buttons[x, y]=rox.g.Button('')
                tab.attach(self.buttons[x, y], x, x+1, y, y+1)

        tab.show_all()

        if fname is not None:
            self.xds_load_from_file(fname)

    def xds_load_from_file(self, fname):
        self.set_title(fname)

        #nthumb=4*4
        helper=vidthumb.VidThumbMPlayer(True)
        helper.make_working_dir()
        
        for y in range(4):
            for x in range(4):
                fpos=float(1+x+4*y)/(4*4+2)
                print x, y, 1+x+4*y, fpos
                pbuf=helper.get_image(fname, self.size, fpos)
                print pbuf
                if pbuf is None:
                    self.buttons[x,y].set_label('')
                    continue
                w=pbuf.get_width()
                if w>self.size:
                    h=pbuf.get_height()*self.size/w
                    pbuf=pbuf.scale_simple(self.size, h, rox.g.gdk.INTERP_BILINEAR)
                pbuf=helper.post_process_image(pbuf, pbuf.get_width(),
                                               pbuf.get_height(),
                                               fpos*helper.total_time if helper.total_time is not None else None)
                img=rox.g.Image()
                img.set_from_pixbuf(pbuf)
                self.buttons[x, y].set_image(img)

        helper.remove_working_dir()

def main(fname):
    win=Window(fname)
    win.show()
    rox.mainloop()

if __name__=='__main__':
    import sys
    main(sys.argv[1])
    
