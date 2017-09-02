#!/bin/bash

rm -rf archdata.hpp

echo "/*" >> archdata.hpp
echo " * This file is part of knarr" >> archdata.hpp
echo " *" >> archdata.hpp
echo " * (c) 2016- Daniele De Sensi (d.desensi.software@gmail.com)" >> archdata.hpp
echo " *" >> archdata.hpp
echo " * For the full copyright and license information, please view the LICENSE" >> archdata.hpp
echo " * file that was distributed with this source code." >> archdata.hpp
echo " */" >> archdata.hpp

echo "#ifndef KNARR_ARCHDATA_HPP_" >> archdata.hpp
echo "#define KNARR_ARCHDATA_HPP_" >> archdata.hpp

# Cache line size
echo "#define LEVEL1_DCACHE_LINESIZE "`getconf LEVEL1_DCACHE_LINESIZE` >> archdata.hpp
# Check if constant_tsc is present
CONSTANT_TSC=$(grep -o '^flags\b.*: .*\bconstant_tsc\b' /proc/cpuinfo | tail -1 | wc -w)
if [ "$CONSTANT_TSC" -gt "0" ]; then
	VALUE=$(./ticksPerNs)
   echo "#define KNARR_NS_PER_TICK "$VALUE >> archdata.hpp
fi

echo "#endif // KNARR_ARCHDATA_HPP_" >> archdata.hpp