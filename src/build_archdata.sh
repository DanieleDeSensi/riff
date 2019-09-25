#!/bin/bash

rm -rf $1

echo "/*" >> $1
echo " * This file is part of riff" >> $1
echo " *" >> $1
echo " * (c) 2016- Daniele De Sensi (d.desensi.software@gmail.com)" >> $1
echo " *" >> $1
echo " * For the full copyright and license information, please view the LICENSE" >> $1
echo " * file that was distributed with this source code." >> $1
echo " */" >> $1

echo "#ifndef RIFF_ARCHDATA_HPP_" >> $1
echo "#define RIFF_ARCHDATA_HPP_" >> $1

# Cache line size
CACHE_LINESIZE=$(cat /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size)
if [ "$?" -ne "0" ] || [ "$CACHE_LINESIZE" -eq "0" ]; then
    CACHE_LINESIZE=$(getconf LEVEL1_DCACHE_LINESIZE)
    if [ "$CACHE_LINESIZE" -eq "0" ]; then
        CACHE_LINESIZE=64
    fi
fi
echo "#define LEVEL1_DCACHE_LINESIZE "$CACHE_LINESIZE >> $1
# Check if constant_tsc is present
CONSTANT_TSC=$(grep -o '^flags\b.*: .*\bconstant_tsc\b' /proc/cpuinfo | tail -1 | wc -w)
if [ "$CONSTANT_TSC" -gt "0" ]; then
    VALUE=$(./ticksPerNs)
    echo "#define RIFF_NS_PER_TICK "$VALUE >> $1
fi

echo "#endif // RIFF_ARCHDATA_HPP_" >> $1
