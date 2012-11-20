#!/bin/bash

start()
{
    gkod -l ./server.log
}

stop()
{
    killall gkod && sleep 1
    if [ `uname` = 'Linux' ];then
        netstat -npl 2>/dev/null| grep 'gkod'
    else
        lsof -ni -P | grep 'gkod'
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
        netstat -npl 2>/dev/null | grep 'gkod'
    else
        lsof -ni -P | grep 'gkod'
    fi
}

case C"$1" in
    Cstart)
    stop &>/dev/null && sleep 1
    start
    sleep 1
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
