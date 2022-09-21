#!/bin/bash
###string="hello,shell,split,test"
#array=(${string//,/ })
#for var in ${array[@]}
#do
#echo $var
#done
cat  array.txt | while read line
do
   array=(${line// / })
    echo ${array[0]} ${array[1]} ${array[2]} 
    i2ctransfer -f -y 4 w2@0x30 ${array[0]} ${array[1]} r1 >> 1.txt
done