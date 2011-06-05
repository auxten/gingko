all: compile move

compile:
	cd ./lib/libevent && ./configure --disable-openssl && make && cd ../.. &&\
	cd src && ./configure && make clean && make 

clean:
	pwd
	#cd src && make clean && cd .. && rm -rf output

move:
	if [ ! -d output ];then mkdir output;fi
	mv ./src/gingko_serv ./output/
	mv ./src/gingko_clnt ./output/
	mv ./src/cts/test/test ./output/
	mv ./src/cts/src/cts ./output/
	
