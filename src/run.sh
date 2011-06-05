#!/bin/bash

start()
{   if [ `uname` = 'Linux' ];then
        echo "############# tcp_tw_recycle  = $(cat /proc/sys/net/ipv4/tcp_tw_recycle)"
        echo "############# tcp_tw_reuse    = $(cat /proc/sys/net/ipv4/tcp_tw_reuse)"
        echo "############# tcp_syncookies  = $(cat /proc/sys/net/ipv4/tcp_syncookies)"
        echo "############# tcp_fin_timeout = $(cat /proc/sys/net/ipv4/tcp_fin_timeout)"
    fi
    cd ../test && ../src/gingko_serv
    if [ `uname` = 'Linux' ];then
        netstat -npl | grep ':2120'
    else
        lsof -ni -P | grep ':2120'
    fi
}

stop()
{
    killall gingko_serv
    if [ `uname` = 'Linux' ];then
        netstat -npl | grep ':2120'
    else
        lsof -ni -P | grep ':2120'
    fi
    echo "All stoped!"
}

makeit()
{
    cd ../src/ && make && echo  "All made!"
}

makedbg()
{
    cd ../src/ && ./configure CXXFLAGS='-O0 -ggdb' --enable-debug && make clean && make
}

makeprofile()
{
    cd ../src/ && ./configure CXXFLAGS='-O0 -pg' --enable-debug && make clean && make
}

case C"$1" in
    Cstart)
    stop
    start
    ;;  

    Cstop)
    stop
    ;;  
    
    Cmake)
    stop
    makeit && start
    ;;  
    
    Cdebug)
    stop
    makedbg && start
    ;;  
    
    Cprof)
    stop
    makeprofile && start
    ;;  

    C*) 
    echo "Usage: $0 {start|stop|status|make|debug|prof}"
    ;;  
esac
