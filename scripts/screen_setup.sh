#!/bin/bash -f

if [ -z $1 ]; then
  echo "Usage: script <process-name>"
  exit
fi

SCREEN_NAME=$1
HOMEPATH=/home/vshastry
SERVERS=`echo vm{1..10}`

screen -dmS "$SCREEN_NAME"
NUM=0
for SERVER in $SERVERS
do
  NUM=$((NUM + 1))
  screen -S "$SCREEN_NAME" -X screen
  if [ z"$SERVER" != z"" ]
  then
    screen -S "$SCREEN_NAME" -p $NUM -X title "$SERVER"
    screen -S "$SCREEN_NAME" -p $NUM -X stuff "ssh root@$SERVER $(printf \\r)"
    screen -S "$SCREEN_NAME" -p $NUM -X stuff "bash $(printf \\r)"
    screen -S "$SCREEN_NAME" -p $NUM -X stuff "cd $HOMEPATH $(printf \\r)"
  fi
done
printf "Done,  now you can join your screen with :\n"
printf "$ screen -dr -S $SCREEN_NAME \n"
