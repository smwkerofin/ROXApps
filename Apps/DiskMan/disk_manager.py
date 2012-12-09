import os
import sys

import dbus

# Load the available interfaces
import hal_if
import udisks2_if

# Select the manager.
try:
    manager=hal_if.Manager()
except dbus.exceptions.DBusException:
    manager=udisks2_if.Manager()

def get_disks():
    return manager.get_disks()

def get_manager():
    return manager
