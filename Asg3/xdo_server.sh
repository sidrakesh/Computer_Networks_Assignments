#!/bin/bash

WID=`xdotool search --name $1 | tail -1`
xdotool windowactivate $WID

> zoom_page.txt

> check.txt
echo $WID >> check.txt

z=0
p=0

for i in 1 2 3 4 5 6
do
  echo -n | xclip -selection clipboard
  xdotool key Tab
  xdotool key ctrl+c

  exec 3<> temp.txt

  xclip -o -selection c > temp.txt

  exec 3>&-

  filename="temp.txt"
  while read -r line
  do
  {}
  done < "$filename"

  len=${#line}

  if [ $len -gt 0 ]
  then
      pos=`expr index "$line" %`
      if [ $pos -eq 0 ]
      then
        if [ $p -eq 0 ]
        then
        p=1
        echo "page $line" >> zoom_page.txt
        fi
      elif [ $z -eq 0 ]
      then
        z=1
        echo "zoom $line" >> zoom_page.txt
      fi
  fi
done
# Get the coordinates of the active window's
#    top-left corner, and the window's size.
#    This excludes the window decoration.
  unset x y w h
  eval $(xwininfo -id $WID |
    sed -n -e "s/^ \+Absolute upper-left X: \+\([0-9]\+\).*/x=\1/p" \
           -e "s/^ \+Absolute upper-left Y: \+\([0-9]\+\).*/y=\1/p" \
           -e "s/^ \+Width: \+\([0-9]\+\).*/w=\1/p" \
           -e "s/^ \+Height: \+\([0-9]\+\).*/h=\1/p" )
  echo "$x $y $w $h" >> zoom_page.txt
#

xdotool getactivewindow >> check.txt

#xdotool key --clearmodifiers Shift+Ctrl+F

