PROTOC=~/third_part/bin/protoc

server.pb.cc:server.proto
		$(PROTOC) server.proto --cpp_out=.

.PHONY:clean

clean:
	rm server server.pb.*
