SHARED_OBJECTS = job.o main.o message.o schedulers.o worker.o

DEBUG = #-DDEBUG

FLAGS = -Ofast -Wall -fno-strict-aliasing $(DEBUG) 

all: sim_one sim_static sim_dynamic

sim_one: $(SHARED_OBJECTS) master_one.o
	smpicc -o sim_one $(SHARED_OBJECTS) master_one.o -lm
	
sim_static: $(SHARED_OBJECTS) master_static.o
	smpicc -o sim_static $(SHARED_OBJECTS) master_static.o -lm
	
sim_dynamic: $(SHARED_OBJECTS) master_dynamic.o
	smpicc -o sim_dynamic $(SHARED_OBJECTS) master_dynamic.o -lm

job.o: job.c job.h shared.h
	smpicc $(FLAGS) -c job.c
	
main.o: main.c config.h master.h worker.h
	smpicc $(FLAGS) -c main.c
	
message.o: message.c config.h shared.h message.h
	smpicc $(FLAGS) -c message.c

schedulers.o: schedulers.c shared.h schedulers.h
	smpicc $(FLAGS) -c schedulers.c
	
worker.o: worker.c worker.h config.h job.h message.h shared.h
	smpicc $(FLAGS) -c worker.c
	
master_one.o: master.c config.h shared.h message.h job.h schedulers.h master.h
	smpicc $(FLAGS) -c master.c -o master_one.o -DONE_JOB
	
master_static.o: master.c config.h shared.h message.h job.h schedulers.h master.h
	smpicc $(FLAGS) -c master.c -o master_static.o -DSTATIC
	
master_dynamic.o: master.c config.h shared.h message.h job.h schedulers.h master.h
	smpicc $(FLAGS) -c master.c -o master_dynamic.o -DDYNAMIC
	
clean:
	rm -f sim_one sim_static sim_dynamic *.o
