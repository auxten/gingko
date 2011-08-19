#!/bin/bash

killall gingko_serv
cd ccover/output
./gingko_serv -h
./gingko_serv --help
./gingko_serv fdsa fdafda 


./gingko_clnt -h
./gingko_clnt --help
./gingko_clnt ddd dfa fdafds fda
./gingko_clnt 10.0.0.0:/home/work/op/oped/noah/tools/gingko/test /home/work/op/oped/noah/tools/gingko/output
./gingko_serv -u 10 -b 0.0.0.0 -p 2120 -t 10
(./gingko_clnt yf-cm-gingko00.yf01:/home/work/op/oped/noah/tools/gingko/test /home/work/op/oped/noah/tools/gingko/output)
sleep 20
killall gingko_clnt
./gingko_clnt yf-cm-gingko00.yf01:/home/work/op/oped/noah/tools/gingko/test /home/work/op/oped/noah/tools/gingko/output -c -u 1 -d 20

