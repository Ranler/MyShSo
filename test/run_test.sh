#!/usr/bin/env sh

HTTP_SERVER=127.0.0.1:8000
ShSo_CLIENT=127.0.0.1:1080
ShSo_SERVER=127.0.0.1:7070

BASE_DIR=..
LOG_DIR=${BASE_DIR}/test/log

WAIT_TIME=3


rm -rf $LOG_DIR
mkdir $LOG_DIR


### HTTP Server
echo "start http server..."
python2 -m SimpleHTTPServer > ${LOG_DIR}/httpserver.log 2>&1 &


### Client
echo "start ShSo client..."
python2 shadowsocks-client/client.py > ${LOG_DIR}/client.log 2>&1 &


### Server
echo "start ShSo server"
LD_LIBRARY_PATH=${BASE_DIR}/lib ${BASE_DIR}/bin/mp_server > ${LOG_DIR}/server.log 2>&1 &


### testing commands
sleep ${WAIT_TIME}
echo "start testing:"
{
    for i in {1..10};do
	{
	    curl http://${HTTP_SERVER}/ --socks5 ${ShSo_CLIENT} -s > ${LOG_DIR}/test_${i}.log
	    if [ $? -eq 0 ];then
		echo "test $i success."
	    else
		echo "test $i failed."
	    fi
	} &
    done
    wait
} | tee ${LOG_DIR}/testing.log


## kill all subprocess
kill -15 -- `jobs -p`
