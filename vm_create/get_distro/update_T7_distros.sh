#!/bin/bash
# Copy the store onto the T7 disk.
rsync -av --delete /media/perrier/T7Shield2/distros/ /media/perrier/T7/distros/
