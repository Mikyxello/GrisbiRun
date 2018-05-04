CCOPTS= -Wall -g -std=gnu99 -Wstrict-prototypes
LIBS= -lglut -lGLU -lGL -lm -lpthread
CC=gcc
AR=ar


BINS=libso_game.a\
	 so_game_client\
     so_game_server\
     test_packets_serialization

OBJS = image.o\
       linked_list.o\
       so_game_protocol.o\
       so_game_client.o\
       so_game_server.o\
       surface.o\
       user_list.o\
       vec3.o\
       vehicle.o\
       world.o\
       world_viewer.o\

HEADERS=common.h\
		helpers.h\
		image.h\
		linked_list.h\
		so_game_protocol.h\
		surface.h\
		user_list.h\
		vec3.h\
		vehicle.h\
		world.h\
		world_viewer.h\

%.o:	%.c $(HEADERS)
	$(CC) $(CCOPTS) -c -o $@  $<

.phony: clean all


all:	$(BINS) 

libso_game.a: $(OBJS) 
	$(AR) -rcs $@ $^
	$(RM) $(OBJS)
	
so_game_client: so_game_client.c libso_game.a
	$(CC) $(CCOPTS) -Ofast -o $@ $^ $(LIBS)

so_game_server: so_game_server.c libso_game.a
	$(CC) $(CCOPTS) -Ofast -o $@ $^ $(LIBS)

test_packets_serialization: test_packets_serialization.c libso_game.a  
	$(CC) $(CCOPTS) -Ofast -o $@ $^  $(LIBS)

clean:
	rm -rf *.o *~  $(BINS)