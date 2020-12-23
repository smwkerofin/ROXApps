"""Implement the Disk and Manager objects for udisks2"""

import os
import sys

import dbus
import dbus.glib
from xml.dom import minidom, Node

import rox

import disks
import manager

introspection='org.freedesktop.DBus.Introspectable'

class NoDriveObject(Exception):
    def __init__(self, path):
        Exception.__init__(self, 'no drive object for path %s' % path)

class Disk(disks.Disk):
    props_interface_name='org.freedesktop.DBus.Properties'
    #drv_interface_name='org.freedesktop.UDisks2.Drive'
    dev_interface_name='org.freedesktop.UDisks2.Block'
    fs_interface_name='org.freedesktop.UDisks2.Filesystem'
    
    def __init__(self, uobj, path, manager):
        self.udisks_obj=uobj
        self.object_path=path
        self.manager=manager
        
        self.props_iface=dbus.Interface(uobj, self.props_interface_name)
        self.dev_iface=dbus.Interface(uobj, self.dev_interface_name)
        self.fs_iface=dbus.Interface(uobj, self.fs_interface_name)

        self.idtype=self.prop_get_str(self.dev_interface_name, 'IdType')
        self.idusage=self.prop_get_str(self.dev_interface_name, 'IdUsage')
        print 'IdType', self.idtype, 'IdUsage', self.idusage

        mounts=self.prop_get_str_array(self.fs_interface_name, 'MountPoints')
        if len(mounts)>0:
            #print mounts
            #print dir(mounts[0])
            #print str(mounts[0])
            mount=mounts[0]
            print 'mount is', mount
        else:
            mount=None
        devpath=self.prop_get_str(self.dev_interface_name, 'Device')
        self.label=self.prop_get_str(self.dev_interface_name,
                                                      'IdLabel')
        #print devpath, self.label
        print 'IdLabel=%s' % self.label
        if '/' in self.label:
            self.label=os.path.basename(self.label)
        if not self.label:
            self.label=os.path.basename(devpath)
        #print self.label

        disks.Disk.__init__(self, devpath, mount)

        self.props_iface.connect_to_signal('PropertiesChanged',
                                           self.props_modified)

        self.icon_name=self.prop_get_str(self.dev_interface_name, 'HintIconName')
        if not self.icon_name:
            pass
        self.drive_type=self.icon_name
        drive_object=self.prop_get_str(self.dev_interface_name, 'Drive')
        if drive_object=='/':
            raise NoDriveObject(self.object_path)
        self.drive_object=Drive(drive_object, self.manager.bus)
        self.removable=self.drive_object.is_removable()
        print 'icon', self.icon_name, 'removable', self.removable

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
        print self.drive_type, self.media_type

    def prop_get_str(self, ifname, propname):
        val=self.props_iface.Get(ifname, propname)
        #print val
        s=''.join(map(str, val))
        if len(s)>0 and s[-1]==chr(0):
            s=s[:-1]
        #print s
        return s

    def prop_get_str_array(self, ifname, propname):
        vals=self.props_iface.Get(ifname, propname)
        ans=map(lambda i: ''.join(map(str, i)), vals)
        for i in range(len(ans)):
            if len(ans[i])>0 and ans[i][-1]==chr(0):
                ans[i]=ans[i][:-1]
        #print ans
        return ans

    def prop_get_bool(self, ifname, propname):
        val=self.props_iface.Get(ifname, propname)
        #print val, repr(val)
        #print bool(val)
        return bool(val)

    def props_modified(self, interface_name, changed_props, invalid_props):
        print 'props_modified', interface_name, changed_props, invalid_props
        mount_change='MountPoints' in changed_props

        if mount_change:
            mount_points=changed_props['MountPoints']
            print mount_points
            if len(mount_points)==0:
                mount=None
            else:
                mount=''.join(map(str, mount_points[0]))
                if mount[-1]=='\x00':
                    mount=mount[:-1]
            print self.mount_path, mount
                
            self.set_mounted(mount)
            if mount:
                self.manager.udisks2_volume_mounted(self.object_path, self)
            else:
                self.manager.udisks2_volume_unmounted(self.object_path, self)

    def get_suggested_mount_name(self):
        return self.label
    
    def get_is_removable(self):
        return self.removable

    def get_is_mountable(self):
        print self.idusage
        return self.idusage in ('filesystem',)

    def mount_disk(self, mount_path):
        try:
            #if self.fstype in linfs:
            #    opts=['uid=%d' % os.getuid()]
            #else:
            #    opts=[]
            try:
                #opts=['uid=%d' % os.getuid()]
                opts={}
                print self, mount_path, opts
                mount_path=self.fs_iface.Mount(opts)
                print mount_path
            except Exception, ex:
                print >> sys.stderr, ex
                opts=[]
                print self, mount_path, self.fstype, opts
                self.vol_iface.Mount(os.path.basename(mount_path), self.fstype,
                                 opts)
        except Exception:
            rox.report_exception()
            return False
        return True

    def unmount_disk(self):
        try:
            print self
            self.fs_iface.Unmount({})
        except Exception:
            rox.report_exception()
            return False
        return True

    def eject_disk(self):
        return self.drive_object.eject()

    @classmethod
    def is_filesystem(klass, obj):
        intro=dbus.Interface(obj, introspection)
        doc=minidom.parseString(intro.Introspect())
        for ifnode in doc.getElementsByTagName('interface'):
            name=ifnode.getAttribute('name')
            if name==klass.fs_interface_name:
                return True
        return False

    def __repr__(self):
        if self.mount_path:
            return '<Disk "%s" on %s %s>' % (self.device, self.mount_path,
                                             self.removable)
        return '<Disk "%s" not mounted>' % self.device

class Drive(object):
    props_interface_name='org.freedesktop.DBus.Properties'
    drv_interface_name='org.freedesktop.UDisks2.Drive'
    
    def __init__(self, path, bus):
        self.path=path
        
        self.obj=bus.get_object('org.freedesktop.UDisks2', path)
        self.drv_iface=dbus.Interface(self.obj,
                                    self.drv_interface_name)
        self.props_iface=dbus.Interface(self.obj, self.props_interface_name)
 
    def prop_get_bool(self, ifname, propname):
        val=self.props_iface.Get(ifname, propname)
        #print val, repr(val)
        #print bool(val)
        return bool(val)

    def is_removable(self):
        return self.prop_get_bool(self.drv_interface_name, 'Removable')

    def eject(self):
        try:
            self.drv_iface.Eject({})
        except Exception:
            rox.report_exception()
            return False
        return True
        

class Manager(manager.Manager):
    bus_name='org.freedesktop.UDisks2'
    object_name='/org/freedesktop/UDisks2'
    man_interface_name='org.freedesktop.DBus.ObjectManager'

    HAL_VOLUME='volume'
    
    def __init__(self):
        manager.Manager.__init__(self)

        self.bus=dbus.SystemBus()
        self.manager_object=self.bus.get_object(self.bus_name,
                                                self.object_name)
        self.manager=dbus.Interface(self.manager_object,
                                    self.man_interface_name)

        self.manager.connect_to_signal('InterfacesAdded', self.device_added)
        self.manager.connect_to_signal('InterfacesRemoved',
                                       self.device_removed)

        self.volumes={}

        self.lookup_disks()

    def lookup_disks(self):
        devs=self.manager.GetManagedObjects()
        #print devs
        for path in devs:
            #print path
            obj=self.bus.get_object(self.bus_name, path)
            if Disk.is_filesystem(obj):
                print path
                try:
                    self.volumes[str(path)]=Disk(obj, path, self)
                except NoDriveObject:
                    continue

    def get_disks(self):
        return self.volumes.itervalues()

    def device_added(self, object_path, ifs_and_props):
        print 'device_added', object_path, ifs_and_props
        print self.volumes.keys()
        hobj=self.bus.get_object(self.bus_name, object_path)
        if Disk.is_filesystem(hobj):
            self.volumes[object_path]=Disk(hobj, object_path, self)

            self.disk_event(self.volumes[object_path], 'disk_added')
        
    def device_removed(self, object_path, interfaces):
        print 'device_removed', object_path, interfaces

        if object_path in self.volumes:
            self.disk_event(self.volumes[object_path], 'disk_removed')
            del self.volumes[object_path]

    def udisks2_volume_mounted(self, udi, disk):
        self.disk_event(disk, 'disk_mounted', disk.mount_path)

    def udisks2_volume_unmounted(self, udi, disk):
        self.disk_event(disk, 'disk_unmounted')
    

def test():
    man=Manager()

    for disk in man.get_disks():
        print disk
    print man.volumes.keys()

if __name__=='__main__':
    test()

    
