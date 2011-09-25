all: makelib compile move rm_config

makelib:
	cd lib && bash -x patch_build.sh && cd ..

compile:
	cd src && ./configure CXXFLAGS='-O0 -ggdb' --enable-debug && make clean && \
	cd hash && make && cd .. && make -j 4 && cd ..

rm_config:
	rm ./src/config.h
	rm ./src/hash/config.h
	rm -rf ./lib/libev/include
	
clean:
	pwd
	#cd src && make clean && cd .. && rm -rf output
	#find ./../../../../../../.. -type f -name "event.h"

move:
	if [ ! -d output ];then mkdir output;fi
	cd output && if [ ! -d bin ];then mkdir bin;fi && if [ ! -d log ];then mkdir log;fi && if [ ! -d script ];then mkdir script;fi
	cp ./src/gingko_serv ./output/bin/
	cp ./src/gingko_clnt ./output/bin/
	#cp ./src/serv_unittest ./output/bin/
	#cp ./src/clnt_unittest ./output/bin/
	cp ./src/erase_job.py ./output/bin/
	cp ./src/run2.sh ./output/bin/run.sh
	chmod u+x ./output/bin/*
