#!/bin/bash -f


if [ -z $1 ]; then
  echo "Usage: script <process-name>"
  exit
fi

for NUM in {1..10}
do
  screen -S "$1" -X -p $NUM stuff "./$1 $(printf \\r)"
done
printf "Done,  now you can join your screen with :\n"
printf "$ screen -dr -S $SCREEN_NAME \n"
