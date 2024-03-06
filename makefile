gobang:gobang.cc session.hpp matcher.hpp server.hpp
	g++ -g -o $@ $^ -L/usr/lib64/mysql/ -lmysqlclient -ljsoncpp -lboost_system -lpthread
.PHONY:clean
clean:
	rm gobang