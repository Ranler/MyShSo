#!/usr/bin/env sh

HTTP_SERVER=127.0.0.1:8000
ShSo_CLIENT=127.0.0.1:1080
ShSo_SERVER=127.0.0.1:7070

BASE_DIR=..
LOG_DIR=${BASE_DIR}/test/log


rm -rf $LOG_DIR
mkdir $LOG_DIR


### HTTP Server
python2 -m SimpleHTTPServer > ${LOG_DIR}/httpserver.log 2>&1 &

### Client
python2 shadowsocks-client/client.py > ${LOG_DIR}/client.log 2>&1 &

### Server
${BASE_DIR}/bin/mp_server > ${LOG_DIR}/server.log 2>&1 &


### testing commands

curl http://${HTTP_SERVER}/ --socks5 ${ShSo_CLIENT}


## trap "exit 0" 15
