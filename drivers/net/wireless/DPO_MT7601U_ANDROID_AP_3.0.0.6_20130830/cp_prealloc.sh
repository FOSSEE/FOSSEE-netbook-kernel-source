#!/bin/bash
logfile="../DPA_MT7601U_ANDROID_JB_3.0.0.6_20130830/PREALLOC/os/linux/Module.symvers"
if [ -e $logfile ]; then
  cat ../DPA_MT7601U_ANDROID_JB_3.0.0.6_20130830/PREALLOC/os/linux/Module.symvers >> os/linux/Module.symvers
fi
echo
exit 0
