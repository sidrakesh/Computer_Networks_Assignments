#!/bin/bash

WID=`xdotool search --class "Okular" | tail -1`
xdotool windowactivate $WID

for i in 1 2 3 4 5
do
	xdotool key Tab
	xdotool key Ctrl+c

	exec 3<> temp.txt

	xclip -o -selection c > temp.txt

	exec 3>&-

	filename="temp.txt"
	while read -r line
	do
	{}
	done < "$filename"

	len=${#line}

	if [ $len -eq 0 ]
	then
  		echo "$len"
	else
		pos=`expr index "$line" %`
  		if [ $pos -eq 0 ]
  		then
    		xdotool type $1
			xdotool key Return
  		else
  			xdotool type $2
			xdotool key Return
  		fi
	fi
done

xdotool windowsize $WID $5 $6
xdotool windowmove $WID $3 $4