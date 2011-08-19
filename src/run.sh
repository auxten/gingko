#!/bin/bash

start()
{   if [ `uname` = 'Linux' ];then
        echo "############# tcp_tw_recycle  = $(cat /proc/sys/net/ipv4/tcp_tw_recycle)"
        echo "############# tcp_tw_reuse    = $(cat /proc/sys/net/ipv4/tcp_tw_reuse)"
        echo "############# tcp_syncookies  = $(cat /proc/sys/net/ipv4/tcp_syncookies)"
        echo "############# tcp_fin_timeout = $(cat /proc/sys/net/ipv4/tcp_fin_timeout)"
    fi
    env CPUPROFILE=/tmp/gingko_serv.prof ../src/gingko_serv
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

starttest()
{
    ./clnt_unittest
    ./serv_unittest
}

makeit()
{
    cd ../src/ && cd hash && make && cd .. && make -j 4 && echo  "All made!"
}

makedbg()
{
    cd ../src/ && ./configure CXXFLAGS='-O0 -ggdb' --enable-debug && make clean; cd hash && make && cd .. && make -j 4
}

makeproduct()
{
    cd ../src/ && ./configure CXXFLAGS='-O2 -ggdb' && make clean; cd hash && make && cd .. && make -j 4
}

makeo3()
{
    cd ../src/ && ./configure CXXFLAGS='-O3 -ggdb'  --enable-o3 && make clean; cd hash && make && cd .. && make -j 4
}

makeprofile()
{
    cd ../src/ && ./configure CXXFLAGS='-O2 -ggdb' --enable-profile && make clean; cd hash && make && cd .. && make -j 4
}

makegprof()
{
    cd ../src/ && ./configure CXXFLAGS='-O0 -ggdb' --enable-gprofile && make clean; cd hash && make && cd .. && make -j 4
}

maketest()
{
    cd ../src/ && ./configure CXXFLAGS='-O0 -ggdb' --enable-unittest && make clean; cd hash && make && cd .. && make -j 4
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
    
    Cgprof)
    stop
    makegprof && start
    ;;  
    
    Cprod)
    stop
    makeproduct && start
    ;;  
    
    Co3)
    stop
    makeo3 && start
    ;;  
    
    Ctest)
    stop
    maketest && starttest
    ;;  

    C*) 
    echo "Usage: $0 {start|stop|status|make|debug|gprof|prof|prod|o3|test}"
    ;;  
esac
