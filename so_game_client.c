
#include <GL/glut.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"

int window;
WorldViewer viewer;
World world;
Vehicle* vehicle; // The vehicle

void* serverHandshake (int my_id, Image* mytexture,Image* map_elevation,Image* map_texture,Image* my_texture_from_server){
	
	int ret, bytes_sent, bytes_recv;
	PacketHeader packet_recv;

    // variables for handling a socket
    int socket_desc;
    struct sockaddr_in server_addr = {0}; // some fields are required to be filled with 0

    // create a socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    ERROR_HELPER(socket_desc, "Could not create socket");

    // set up parameters for the connection
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(SERVER_PORT); // don't forget about network byte order!

    // initiate a connection on the socket
    ret = connect(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
    ERROR_HELPER(ret, "Could not create connection");

    if (DEBUG) fprintf(stderr, "Connection established!\n");
    
    IdPacket* idpack = (IdPacket*)malloc(sizeof(IdPacket));
    
    PacketHeader id_head;
    id_head.type = GetId;
    
    idpack->header = id_head;
    idpack->id = -1;
    
    char id_packet_buffer[1000000];
    
    
    bytes_sent = Packet_serialize(id_packet_buffer, &idpack->header);
    
	while ( (ret = send(socket_desc, id_packet_buffer, bytes_sent, 0)) < 0) {
		if (errno == EINTR) continue;
		ERROR_HELPER(-1, "Cannot write to socket");
	}
	
	while ( (bytes_recv = recv(socket_desc, bufferPacket, sizeof(IdPacket), 0)) < 0 ) { //ricevo ID dal server
        if (errno == EINTR) continue;
        ERROR_HELPER(-1, "Cannot read from socket");
    }
    
    IdPacket* deserialized_packet = (IdPacket*)Packet_deserialize(bufferPacket,bytes_recv);
    
    my_id = deserialized_packet->id; //scrivo l'id dentro la variabile
    
    Packet_free(&deserialized_packet->header);
	Packet_free(&idpack->header);
	
    ImagePacket* image_packet = (ImagePacket*)malloc(sizeof(ImagePacket));
    PacketHeader im_head;
    im_head.type = PostTexture;
    image_packet->image = mytexture;
    
    char image_packet_buffer[1000000];
    int image_packet_buffer_size = Packet_serialize(image_packet_buffer, &image_packet->header);
    
	while ( (ret = send(socket_desc, image_packet_buffer, image_packet_buffer_size, 0)) < 0) { //invio mytexture al server
		if (errno == EINTR) continue;
		ERROR_HELPER(-1, "Cannot write to socket");
	}
	
	while ( (bytes_recv = recv(socket_desc, image_packet_buffer, sizeof(ImagePacket), 0)) < 0 ) { //ricevo ID dal server
        if (errno == EINTR) continue;
        ERROR_HELPER(-1, "Cannot read from socket");
    }
    
    ImagePacket* deserialized_image_packet = (ImagePacket*)Packet_deserialize(image_packet_buffer, bytes_recv); 
    my_texture_from_server = deserialized_image_packet->image;  //scrivo la texture che ho ricevuto dal server nella variabile
    Packet_free(&deserialized_image_packet->header);
	
	

	
	



	
	
    
    
	
	
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    char id[4];  //buffer char per ID
    size_t msg_len;

    while ( (msg_len = recv(socket_desc, id, 4, 0)) < 0 ) { //ricevo ID dal server
        if (errno == EINTR) continue;
        ERROR_HELPER(-1, "Cannot read from socket");
    }
    
    my_id = (int)id; //metto l'id dentro alla fantastica variabile "my_id"
    
    char* buffer[1024*1024*6]; // ?????????????????????????
    
    msg_len = strlen(msg);
    
    ret = Image_serialize(mytexture,buffer,1024*1024*6);
    ERROR_HELPER(ret-1, "Cannot serialize mytexture");
    
	// send message to server
	while ( (ret = send(socket_desc, buffer, 1024*1024*6, 0)) < 0) { //sistemare size buffer
		if (errno == EINTR) continue;
		ERROR_HELPER(-1, "Cannot write to socket");
	}
	
	char img_size[4];  // int image size from server
	
	while ( (msg_len = recv(socket_desc,img_size,4, 0)) < 0 ) {
        if (errno == EINTR) continue;
        ERROR_HELPER(-1, "Cannot read from socket");
    }
    
    char imagebuf[(int)img_size];  //buffer char dell'immagine 
    
    while ( (msg_len = recv(socket_desc,imagebuf,(int)img_size, 0)) < 0 ) {
        if (errno == EINTR) continue;
        ERROR_HELPER(-1, "Cannot read from socket");
    }
    
	map_elevation = Image_deserialize(imagebuf, (int)img_size);  //deserializzazione immagine
	
	while ( (msg_len = recv(socket_desc,img_size,4, 0)) < 0 ) {
        if (errno == EINTR) continue;
        ERROR_HELPER(-1, "Cannot read from socket");
    }

	char imagebuf2[(int)img_size];  //buffer char dell'immagine 
    
    while ( (msg_len = recv(socket_desc,imagebuf2,(int)img_size, 0)) < 0 ) {
        if (errno == EINTR) continue;
        ERROR_HELPER(-1, "Cannot read from socket");
    }
    
	map_texture = Image_deserialize(imagebuf2, (int)img_size);  //deserializzazione immagine
    
    
	
}

void keyPressed(unsigned char key, int x, int y)
{
  switch(key){
  case 27:
    glutDestroyWindow(window);
    exit(0);
  case ' ':
    vehicle->translational_force_update = 0;
    vehicle->rotational_force_update = 0;
    break;
  case '+':
    viewer.zoom *= 1.1f;
    break;
  case '-':
    viewer.zoom /= 1.1f;
    break;
  case '1':
    viewer.view_type = Inside;
    break;
  case '2':
    viewer.view_type = Outside;
    break;
  case '3':
    viewer.view_type = Global;
    break;
  }
}


void specialInput(int key, int x, int y) {
  switch(key){
  case GLUT_KEY_UP:
    vehicle->translational_force_update += 0.1;
    break;
  case GLUT_KEY_DOWN:
    vehicle->translational_force_update -= 0.1;
    break;
  case GLUT_KEY_LEFT:
    vehicle->rotational_force_update += 0.1;
    break;
  case GLUT_KEY_RIGHT:
    vehicle->rotational_force_update -= 0.1;
    break;
  case GLUT_KEY_PAGE_UP:
    viewer.camera_z+=0.1;
    break;
  case GLUT_KEY_PAGE_DOWN:
    viewer.camera_z-=0.1;
    break;
  }
}


void display(void) {
  WorldViewer_draw(&viewer);
}


void reshape(int width, int height) {
  WorldViewer_reshapeViewport(&viewer, width, height);
}

void idle(void) {
  World_update(&world);
  usleep(30000);
  glutPostRedisplay();
  
  // decay the commands
  vehicle->translational_force_update *= 0.999;
  vehicle->rotational_force_update *= 0.7;
}

int main(int argc, char **argv) {
  if (argc<3) {
    printf("usage: %s <server_address> <player texture>\n", argv[1]);
    exit(-1);
  }

  printf("loading texture image from %s ... ", argv[2]);
  Image* my_texture = Image_load(argv[2]);
  if (my_texture) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }
  
  Image* my_texture_for_server;
  // todo: connect to the server
  //   -get ad id
  //   -send your texture to the server (so that all can see you)
  //   -get an elevation map
  //   -get the texture of the surface

  // these come from the server
  int my_id;
  Image* map_elevation;
  Image* map_texture;
  Image* my_texture_from_server;
  
  connectionHandler(my_id, mytexture, map_elevation, map_texture, my_texture_from_server);

  // construct the world
  World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);
  vehicle=(Vehicle*) malloc(sizeof(Vehicle));
  Vehicle_init(&vehicle, &world, my_id, my_texture_from_server);
  World_addVehicle(&world, v);

  // spawn a thread that will listen the update messages from
  // the server, and sends back the controls
  // the update for yourself are written in the desired_*_force
  // fields of the vehicle variable
  // when the server notifies a new player has joined the game
  // request the texture and add the player to the pool
  /*FILLME*/

  WorldViewer_runGlobal(&world, vehicle, &argc, argv);

  // cleanup
  World_destroy(&world);
  return 0;             
}
