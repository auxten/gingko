.PHONY: all makelib compile rm_config clean move check
all: check_configure makelib compile #move rm_config

check_configure:
	if [ ! -x ./src/configure ]; then ( cd ./src && ./autogen.sh ); fi

makelib:
	cd lib && bash -x patch_build.sh && cd ..

compile:
	#cd src && ./configure CXXFLAGS='-ggdb -D_GKO_VERSION=\"$(subst VERSION:,,$(VERSION_SCMPF))\"' --enable-debug -enable-unittest && make clean && 
	cd src && ./configure CXXFLAGS='-ggdb ' --enable-debug && make clean && \
	cd hash && make && cd .. && make -j 4 && cd ..

rm_config:
	rm ./src/config.h
	rm ./src/hash/config.h
	rm -rf ./lib/libev/include
	
clean:
	pwd
	rm -rf output test
	cd src && if [ -f Makefile ];then make clean; fi
	#find ./../../../../../../.. -type f -name "event.h"

move:
	if [ ! -d output ];then mkdir output;fi
	cd output && if [ ! -d bin ];then mkdir bin; fi && \
	if [ ! -d testbin ];then mkdir testbin; fi && \
	if [ ! -d conf ];then mkdir conf; fi
	cp ./src/gingko_serv ./output/bin/gkod
	cp ./src/gingko_clnt ./output/bin/gkocp
	cp ./src/serv_unittest ./output/testbin/
	cp ./src/clnt_unittest ./output/testbin/
	cp -r ./output/testbin ./test
	cp ./bin/* ./output/bin/
	cp ./conf/* ./output/conf/
	#cp ./src/erase_job.py ./output/bin/
	#cp ./src/run2.sh ./output/bin/gkod_ctl
	cp deploy ./output/ && chmod +x ./output/deploy
	cd output && md5sum deploy bin/* conf/* > md5sum
	#cd output/bin && cp gkocp{,.new}
	#cd output/bin && cp gkod{,.new}
	#cd output/bin && cp gkod_ctl{,.new}
	chmod +x ./output/bin/*
	cd output && tar czvf gingko.tgz bin conf md5sum deploy

check:
	cd test && ./clnt_unittest && ./serv_unittest
