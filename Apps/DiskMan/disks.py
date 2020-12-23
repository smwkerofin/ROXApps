import os
import sys

import rox.filer

class Disk(object):
    """Abstract base class to manage a disk."""
    
    media_types=('disk', 'rmdisk', 'usbstick', 'cdrom', 'floppy')

    MT_DISK='disk'
    MT_RMDISK='rmdisk'
    MT_USB='usbstick'
    MT_OPTICAL='cdrom'
    MT_FLOPPY='floppy'
    
    def __init__(self, devpath, mount_path=None):
        self.device=devpath
        self.mount_path=mount_path

        self.media_type=self.media_types[0]

        self.old_mount_path=self.mount_path

        self.open_on_mount=False

    def set_mounted(self, mount_path):
        print 'Disk.set_mounted', self, mount_path
        if mount_path:
            self.mount_path=mount_path
            self.old_mount_path=self.mount_path
            if self.open_on_mount:
                rox.filer.open_dir(self.mount_path)
                self.open_on_mount=False
        else:
            self.mount_path=None

    def set_unmounted(self):
        self.mount_path=None
            
    def get_mount_path(self):
        return self.mount_path

    def get_last_mount_path(self):
        return self.old_mount_path

    def get_device_name(self):
        return self.device

    def get_suggested_mount_name(self):
        return os.path.basename(self.device)

    def get_is_removable(self):
        return True # Can't really tell

    def get_is_mountable(self):
        return True

    def get_type(self):
        return self.media_type

    def open_when_mounted(self):
        self.open_on_mount=True

    def mount_disk(self, mount_path):
        raise Exception, 'mount of %s not implemented' % self

    def unmount_disk(self):
        raise Exception, 'unmount of %s not implemented' % self

    def eject_disk(self):
        raise Exception, 'eject of %s not implemented' % self

    def __repr__(self):
        if self.mount_path:
            return '<Disk "%s" on %s>' % (self.device, self.mount_path)
        return '<Disk "%s" not mounted>' % self.device

