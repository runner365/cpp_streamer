#!/bin/bash
echo "start building ${1}"

case $1 in
    all)
     if [ -d "objs" ];then
          rm -rf objs
     fi
     mkdir objs
     cd objs
     cmake ..
     make -j 2
     ;;
    m1)
     if [ -d "objs" ];then
          rm -rf objs
     fi
     mkdir objs
     cd objs
     cmake .. -DMAC_ARM=YES
     make -j 2
     ;;

     *)
     if [ ! -d "objs" ];then
          mkdir objs
     fi
     cd objs
     cmake ..
     make -j 2
     ;;

esac
