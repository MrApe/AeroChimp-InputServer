#!/bin/bash


IP="10.0.1.14"
PORT="5050"
PIID=$(hostname)
ID=$(echo $2 | sed -e 's/\/input.*$//g' -e 's/\/00.*$//g' -e 's/^.*\///g')

logger Starting judgePad on ${PIID} sending to ${IP}:${PORT} with device $1 as $ID

inputserver ${IP} ${PORT} ${PIID} /dev/input/$1 $ID & 

logger Started judgePad on ${PIID} sending to ${IP}:${PORT} with device $1 as $ID
