all: compile move rm_config

compile:
	cd src && ./configure CXXFLAGS='-O0 -ggdb' --enable-unittest --enable-debug && make clean && \
	cd hash && make && cd .. && make -j 4

rm_config:
	rm ./src/config.h
	rm ./src/hash/config.h
	
clean:
	pwd
	#cd src && make clean && cd .. && rm -rf output
	#find ./../../../../../../.. -type f -name "event.h"

move:
	if [ ! -d output ];then mkdir output;fi
	mv ./src/gingko_serv ./output/
	mv ./src/gingko_clnt ./output/	
	mv ./src/serv_unittest ./output/	
	mv ./src/clnt_unittest ./output/	
	mv ./src/erase_job.py ./output/