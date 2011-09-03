#!/bin/bash

start()
{
    ./gingko_serv -l ../log/server.log
}

stop()
{
    killall gingko_serv && sleep 3
    if [ `uname` = 'Linux' ];then
        netstat -npl | grep ':2120'
    else
        lsof -ni -P | grep ':2120'
    fi
}

status()
{
    if [ `uname` = 'Linux' ];then
        echo "############# tcp_tw_recycle  = $(cat /proc/sys/net/ipv4/tcp_tw_recycle)"
        echo "############# tcp_tw_reuse    = $(cat /proc/sys/net/ipv4/tcp_tw_reuse)"
        echo "############# tcp_syncookies  = $(cat /proc/sys/net/ipv4/tcp_syncookies)"
        echo "############# tcp_fin_timeout = $(cat /proc/sys/net/ipv4/tcp_fin_timeout)"
    fi
    if [ `uname` = 'Linux' ];then
        netstat -npl | grep ':2120'
    else
        lsof -ni -P | grep ':2120'
    fi
}

case C"$1" in
    Cstart)
    stop
    start
    status
    ;;  

    Cstop)
    stop
    ;;  

    Cstatus)
    status
    ;;  
    
    C*) 
    echo "Usage: $0 {start|stop|status}"
    ;;  
esac
