#!/bin/bash -f


if [ -z $1 ]; then
  echo "Usage: script <process-name>"
  exit
fi

for NUM in {1..10}
do
  screen -S "$1" -X -p $NUM stuff $'\003'
  screen -S "$1" -X -p $NUM stuff $'\003'
  screen -S "$1" -X -p $NUM stuff $'\003'
done
printf "Done,  now you can join your screen with :\n"
printf " screen -dr -S $1\n"
