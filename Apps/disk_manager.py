import os
import sys

# Load the available interfaces
import hal_if

# Select the manager.  Only one available...
manager=hal_if.Manager()

def get_disks():
    return manager.get_disks()

def get_manager():
    return manager
