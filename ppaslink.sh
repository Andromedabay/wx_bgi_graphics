#!/bin/sh
DoExitAsm ()
{ echo "An error occurred while assembling $1"; exit 1; }
DoExitLink ()
{ echo "An error occurred while linking $1"; exit 1; }
OFS=$IFS
IFS="
"
/usr/bin/ld          -order_file symbol_order.fpc -multiply_defined suppress -L. -o examples/demoFreePascal/demo_bgi_api_coverage `cat link13758.res` -filelist linkfiles13758.res
if [ $? != 0 ]; then DoExitLink ; fi
IFS=$OFS
