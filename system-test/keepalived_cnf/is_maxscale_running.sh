#!/bin/bash
fileName="maxctrl_output.txt"
rm $fileName
timeout 2s maxctrl list servers > $fileName
to_result=$?
if [ $to_result -ge 1 ]
then
  echo Timed out or error, timeout returned $to_result
  exit 3
else
  echo MaxCtrl success, rval is $to_result
  echo Checking maxctrl output sanity
  grep1=$(grep server1 $fileName)
  grep2=$(grep server2 $fileName)

  if [ "$grep1" ] && [ "$grep2" ]
  then
    echo All is fine
    exit 0
  else
    echo Something is wrong
    exit 3
  fi
fi
