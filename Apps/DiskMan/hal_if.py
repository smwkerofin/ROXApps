"""Implement the Disk and Manager objects for HAL."""

import os
import sys

import dbus
import dbus.glib

import rox

import disks
import manager

class HalDisk(disks.Disk):
    dev_interface_name='org.freedesktop.Hal.Device'
    vol_interface_name='org.freedesktop.Hal.Device.Volume'
    
    def __init__(self, hobj, udi, manager):
        self.hal_obj=hobj
        self.udi=udi
        self.manager=manager
        
        self.dev_iface=dbus.Interface(hobj, self.dev_interface_name)
        self.vol_iface=dbus.Interface(hobj, self.vol_interface_name)

        self.fstype=self.dev_iface.GetPropertyString('volume.fstype')
        self.fsusage=self.dev_iface.GetPropertyString('volume.fsusage')

        storage_udi=self.dev_iface.GetPropertyString('info.parent')
        storage_obj=manager.bus.get_object(manager.bus_name, storage_udi)
        self.storage_iface=dbus.Interface(storage_obj, self.dev_interface_name)
        self.removable=self.storage_iface.GetPropertyBoolean(
            'storage.removable')
        self.drive_type=self.storage_iface.GetPropertyString(
            'storage.drive_type')
        print 'removable=%d type=%s' % (self.removable, self.drive_type)
        
        if self.dev_iface.GetPropertyBoolean('volume.is_mounted'):
            mount=self.dev_iface.GetPropertyString('volume.mount_point')
            print 'mount is', mount
        else:
            mount=None
        devpath=self.dev_iface.GetPropertyString('block.device')
        self.label=self.dev_iface.GetPropertyString('volume.label')
        print devpath, self.label
        if '/' in self.label:
            self.label=os.path.basename(self.label)
        if not self.label:
            self.label=os.path.basename(devpath)
        print self.label

        disks.Disk.__init__(self, devpath, mount)

        self.dev_iface.connect_to_signal('Condition', self.hal_condition)
        self.dev_iface.connect_to_signal('PropertyModified',
                                         self.hal_prop_modified)

        if self.drive_type in ('compact_flash', 'memory_stick',
                               'smart_media', 'sd_mmc'):
            self.media_type=self.MT_USB

        elif self.drive_type=='cdrom':
            self.media_type=self.MT_OPTICAL

        elif self.drive_type=='floppy':
            self.media_type=self.MT_FLOPPY

        else:
            if self.removable:
                self.media_type=self.MT_RMDISK
            else:
                self.media_type=self.MT_DISK

    def hal_condition(self, cond_name, cond_details):
        print 'hal_condition', cond_name, cond_details

    def hal_prop_modified(self, num_updates, updates):
        #print 'hal_prop_modified', self, num_updates
        mount_change=False
        for update in updates:
            if update[0]=='volume.is_mounted':
                mount_change=True

        if mount_change:
            if self.dev_iface.GetPropertyBoolean('volume.is_mounted'):
                mount=self.dev_iface.GetPropertyString('volume.mount_point')
            else:
                mount=None
            print self.mount_path, mount
                
            self.set_mounted(mount)
            if mount:
                self.manager.hal_volume_mounted(self.udi, self)
            else:
                self.manager.hal_volume_unmounted(self.udi, self)

    def get_suggested_mount_name(self):
        return self.label
    
    def get_is_removable(self):
        return self.removable

    def get_is_mountable(self):
        return self.fsusage in ('filesystem',)

    def mount_disk(self, mount_path):
        try:
            opts=['uid=%d' % os.getuid()]
            print self, mount_path, self.fstype, opts
            self.vol_iface.Mount(os.path.basename(mount_path), self.fstype,
                                 opts)
        except Exception:
            rox.report_exception()
            return False
        return True

    def unmount_disk(self):
        try:
            print self, self.fstype
            self.vol_iface.Unmount([])
        except Exception:
            rox.report_exception()
            return False
        return True

    def eject_disk(self):
        try:
            print self, self.fstype
            self.vol_iface.Eject([])
        except Exception:
            rox.report_exception()
            return False
        return True

    def __repr__(self):
        if self.mount_path:
            return '<HalDisk "%s" on %s>' % (self.device, self.mount_path)
        return '<HalDisk "%s" not mounted>' % self.device

class Manager(manager.Manager):
    bus_name='org.freedesktop.Hal'
    object_name='/org/freedesktop/Hal/Manager'
    man_interface_name='org.freedesktop.Hal.Manager'

    HAL_VOLUME='volume'
    
    def __init__(self):
        manager.Manager.__init__(self)

        self.bus=dbus.SystemBus()
        self.manager_object=self.bus.get_object(self.bus_name,
                                                self.object_name)
        self.manager=dbus.Interface(self.manager_object,
                                    self.man_interface_name)

        self.manager.connect_to_signal('DeviceAdded', self.hal_device_added)
        self.manager.connect_to_signal('DeviceRemoved',
                                       self.hal_device_removed)

        self.volumes={}

        self.lookup_disks()

    def lookup_disks(self):
        devs=self.manager.FindDeviceByCapability(self.HAL_VOLUME)
        for udi in devs:
            obj=self.bus.get_object(self.bus_name, udi)
            self.volumes[udi]=HalDisk(obj, udi, self)

    def get_disks(self):
        return self.volumes.itervalues()

    def hal_device_added(self, udi):
        print 'hal_device_added', udi
        hobj=self.bus.get_object(self.bus_name, udi)
        dev_iface=dbus.Interface(hobj, HalDisk.dev_interface_name)

        if dev_iface.QueryCapability(self.HAL_VOLUME):
            self.volumes[udi]=HalDisk(hobj, udi, self)

            self.disk_event(self.volumes[udi], 'disk_added')
        
    def hal_device_removed(self, udi):
        print 'hal_device_removed', udi

        if udi in self.volumes:
            self.disk_event(self.volumes[udi], 'disk_removed')
            del self.volumes[udi]

    def hal_volume_mounted(self, udi, disk):
        self.disk_event(disk, 'disk_mounted', disk.mount_path)

    def hal_volume_unmounted(self, udi, disk):
        self.disk_event(disk, 'disk_unmounted')
    
def test():
    man=Manager()
    print dir(man.manager)

    for disk in man.get_disks():
        print disk

if __name__=='__main__':
    test()

    
