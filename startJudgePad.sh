#!/bin/bash


IP="10.0.1.14"
PORT="5050"
PIID=$(hostname)
ID=$(echo $2 | sed -e 's/\/input.*$//g' -e 's/\/00.*$//g' -e 's/^.*\///g' | md5sum | sed 's/  -//g')

logger Starting judgePad on ${PIID} sending to ${IP}:${PORT} with device $1 as $ID

curl -d "device=${PIID}_${ID}" http://${IP}:${PORT}/device

inputserver ${IP} ${PORT} ${PIID} /dev/input/$1 $ID  

curl -X DELETE  http://${IP}:${PORT}/device/${PIID}_${ID}

logger judgePad on ${PIID} sending to ${IP}:${PORT} with device $1 as $ID ended
