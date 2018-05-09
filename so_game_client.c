#include <GL/glut.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>  // htons() and inet_addr()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include "common.h"
#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "so_game_protocol.h"
#include "user_list.h"
#include "semaphore.h"

#define TIME_TO_SLEEP   20000
#define WORLDSIZE       4096
#define SERVER_ADDRESS  "127.0.0.1"
#define SERVER_PORT     25252
#define UDPPORT         8888
#define BUFFER_SIZE     1000000

typedef struct localWorld{
    int id_list[WORLDSIZE];
    int players_online;
    Vehicle** vehicles;
} localWorld;

typedef struct udpArgs{
    localWorld* lw;
    struct sockaddr_in server_addr;
    int socket_udp;
    int socket_tcp;
}udpArgs;

localWorld player_world;

/* Original part */
int window;
WorldViewer viewer;
World world;
Vehicle* vehicle; // The vehicle
int socket_tcp;

/* End of original part */


/*
void* UDP_Sender(void* args){
    int ret;
    udpArgs* udp_args = (udpArgs*) args;
    struct sockaddr_in server_addr = udp_args->server_addr;
    int socket_udp = udp_args->socket_udp;
    int serverlen = sizeof(server_addr);   
    while (server_connected){
		  ret = send_updates(socket_udp,server_addr,serverlen);
		  PTHREAD_ERROR_HELPER(ret,"error during send_updates");
	  }
    close(s);
    return 0;
}

void* UDP_Receiver(void* args){
	udpArgs udp_args =(udpArgs*)args;
	struct sockaddr_in server_addr=udp_args.server_addr;
  int socket_udp =udp_args.socket_udp;
  int serverlen=sizeof(server_addr);
  int bytes_read;
  char buf_rcv[BUFFER_SIZE];
  while (1){
		bytes_read=recvfrom(socket_udp, buf_rcv, BUFFER_SIZE, 0, (struct sockaddr*) &server_addr, &addrlen);
		PTHREAD_ERROR_HELPER(bytes_read,"error during recvfrom");
		ret = packet_analisys(buf_rcv,bytes_read);
		PTHREAD_ERROR_HELPER(bytes_read,"error during packet_analisys");
	}
}

int packet_analisys(char* buffer, int len){
	
	World world_aux = world;
	ret=sem_wait(world_update_sem);
	PTHREAD_ERROR_HELPER(ret,"errore sulla sem_wait in world_updater");
	int i,new_players=0;
  WorldUpdatePacket* deserialized_wu_packet = (WorldUpdatePacket*)Packet_deserialize(world_buffer, world_buffer_size);
	if(deserialized_wu_packet->header->type != WorldUpdate) return 0;
	ClientUpdate* aux = deserialized_wu_packet->updates;
	while(aux!=NULL){
		for(int i=0;i<WORLDSIZE;i++){
			if (player_world->id_list[i] == aux->id){
				Vehicle_setXYTheta(player_world->vehicles[i],deserialized_wu_packet->updates->x,deserialized_wu_packet->updates->y,deserialized_wu_packet->updates->theta);
				break;
			}
		new_players=1;
		}
	}
	
	if (deserialized_wu_packet->num_vehicles != player_world->players_online || new_players) check_newplayers(deserialized_wu_packet); //aggiunge eventuali nuovi giocatori
	
	
	
	
	
	
}
int sendUpdates(int socket_udp,struct sockaddr_in server_addr,int serverlen){
		Vehicle_update(vehicle,1);
		VehicleUpdatePacket* vehicle_packet = (VehicleUpdatePacket*)malloc(sizeof(VehicleUpdatePacket));
		PacketHeader v_head;
		v_head.type = VehicleUpdate;
		vehicle_packet->header = v_head;
		vehicle_packet->id = my_id;
		vehicle_packet->rotational_force = vehicle->rotational_force;
		vehicle_packet->translational_force = vehicle->translational_force;
		char vehicle_buffer[BUFFER_SIZE];
		int vehicle_buffer_size = Packet_serialize(vehicle_buffer, &vehicle_packet->header);
		ret = sendto(s, vehicle_buffer, vehicle_buffer_size , 0 , (struct sockaddr *) &si_other, slen);
		PTHREAD_ERROR_HELPER(ret,"UDP sendto failed");
}
*/

/* World updater */
void* world_updater(void* args){
	while (1){
		World_update(&world);
		usleep(30000);
	}
	return 0;
}


 /* ----------------- */
 /* GetId dell'utente */
 /* ----------------- */
void recv_ID(int* my_id, int socket_desc) {
  int ret;
  char buffer[BUFFER_SIZE];

  printf("[TCP] Requesting ID...\n");
  PacketHeader header;
  header.type = GetId;
  
  IdPacket* packet = (IdPacket*)malloc(sizeof(IdPacket));
  packet->header = header;
  packet->id = -1;

  int buffer_size = Packet_serialize(buffer, &packet->header);

  // Invia la richiesta
  while ( (ret = send(socket_desc, buffer, buffer_size, 0)) < 0 ) {
    if (errno == EINTR) continue;
    ERROR_HELPER(-1, "[ERROR] Cannot write to socket!!!");
  }

  printf("[TCP] ID request sent...\n");
  // printf("[TCP] Waiting for ID receiving...\n");

  // Riceve l'id
  while ( (buffer_size = recv(socket_desc, buffer, BUFFER_SIZE, 0)) < 0 ) { //ricevo ID dal server
    if (errno == EINTR) continue;
    ERROR_HELPER(-1, "[ERROR] Cannot read from socket!!!");
  }

  printf("[TCP] ID received...\n");

  // Prende l'id ricevuto e lo inserisce in my_id
  IdPacket* id_recv = (IdPacket*) Packet_deserialize(buffer,buffer_size);
  *my_id = id_recv->id;

  printf("[TCP] ID loaded: %d...\n", *my_id);
  
  // Libera la memoria non più utilizzata 
  Packet_free(&id_recv->header);
  Packet_free(&packet->header);
  free(id_recv);
  free(packet);
}


/* ------------------------------------ */
/* GetTexture della texture della mappa */
/* ------------------------------------ */
void recv_Texture(Image** map_texture, int socket_desc) {
  int ret;
  char buffer[BUFFER_SIZE];

  printf("[TCP] Requesting texture...\n");
  PacketHeader header;
  header.type = GetTexture;

  ImagePacket* packet = (ImagePacket*) malloc(sizeof(ImagePacket));
  packet->image = NULL;
  packet->header = header;

  int buffer_size = Packet_serialize(buffer, &packet->header);

  // Invia la richiesta
  while ( (ret = send(socket_desc, buffer, buffer_size, 0)) < 0 ) {
    if (errno == EINTR) continue;
    ERROR_HELPER(-1, "[ERROR] Cannot write to socket!!!");
  }

  printf("[TCP] Texture request sent...\n");
  // printf("[TCP] Waiting for texture receiving...\n");

  int actual_size = 0;
  buffer_size = 0;
  while(1) {
    // Riceve la texture della mappa
    while ( (buffer_size += recv(socket_desc, buffer + buffer_size, BUFFER_SIZE - buffer_size, 0)) < 0 ) {
      if (errno == EINTR) continue;
      ERROR_HELPER(-1, "[ERROR] Cannot read from socket!!!");
    }

    printf("[TCP] Texture received...\n");

    // Dimensione totale del pacchetto da ricevere
    actual_size = ((PacketHeader*) buffer)->size;

    // Se la dimensione del pacchetto ricevuto è ancora minore della dimensione del pacchetto totale aspetta le altre parti
    if (buffer_size < actual_size) {
      printf("[TCP] Next packet...\n");
      continue;
    }
    else break;
  }

  // Load della texture della mappa
  ImagePacket* texture_recv = (ImagePacket*) Packet_deserialize(buffer, buffer_size);
  *map_texture = texture_recv->image;

  printf("[TCP] Texture loaded...\n");

  Packet_free(&packet->header);
  Packet_free(&texture_recv->header);
  free(texture_recv);
  free(packet);
}


/* -------------------------------------- */
/* GetElevation della texture della mappa */
/* -------------------------------------- */
void recv_Elevation(Image** map_elevation, int socket_desc) {
  int ret;
  char buffer[BUFFER_SIZE];

  printf("[TCP] Requesting elevation...\n");
  PacketHeader header;
  header.type = GetElevation;

  ImagePacket* packet = (ImagePacket*) malloc(sizeof(ImagePacket));
  packet->image = NULL;
  packet->header = header;

  // Serializza il pacchetto
  int buffer_size = Packet_serialize(buffer, &packet->header);

  // Invia la richiesta
  while ( (ret = send(socket_desc, buffer, buffer_size, 0)) < 0 ) {
    if (errno == EINTR) continue;
    ERROR_HELPER(-1, "[ERROR] Cannot write to socket!!!");
  }

  printf("[TCP] Elevation request sent...\n");
  // printf("[TCP] Waiting for elevation receiving...\n");

  int actual_size = 0;
  buffer_size = 0;
  while(1) {
    // Riceve la elevation della mappa
    while ( (buffer_size += recv(socket_desc, buffer + buffer_size, BUFFER_SIZE - buffer_size, 0)) < 0 ) {
      if (errno == EINTR) continue;
      ERROR_HELPER(-1, "[ERROR] Cannot read from socket!!!");
    }

    printf("[TCP] Elevation received...\n");

    // Dimensione totale del pacchetto da ricevere
    actual_size = ((PacketHeader*) buffer)->size;

    // Se la dimensione del pacchetto ricevuto è ancora minore della dimensione del pacchetto totale aspetta le altre parti
    if (buffer_size < actual_size) {
      printf("[TCP] Next packet...\n");
      continue;
    }
    else break;
  }

  // Load della elevation della mappa
  ImagePacket* elevation_packet = (ImagePacket*) Packet_deserialize(buffer, buffer_size);
  *map_elevation = elevation_packet->image;

  printf("[TCP] Elevation loaded...\n");

  Packet_free(&packet->header);
  Packet_free(&elevation_packet->header);
  free(elevation_packet);
  free(packet);
}

/* ------------------------------------- */
/* PostTexture della texture del veicolo */
/* ------------------------------------- */
void send_Texture(Image** my_texture, Image** my_texture_from_server, int socket_desc) {
  int ret;
  char buffer[BUFFER_SIZE];

  printf("[TCP] Sending texture to server...\n");
  PacketHeader header;
  header.type = PostTexture;

  ImagePacket* packet = (ImagePacket*)malloc(sizeof(ImagePacket));
  packet->image = *my_texture;
  packet->header = header;

  int buffer_size = Packet_serialize(buffer, &packet->header);
  
  // Invia la texture
  while ( (ret = send(socket_desc, buffer, buffer_size, 0)) < 0) {
    if (errno == EINTR) continue;
    ERROR_HELPER(ret, "[ERROR] Cannot write to socket!!!");
  }

  printf("[TCP] Vehicle texture sent...\n");
  // printf("[TCP] Waiting for texture receiving back...\n");

  int actual_size = 0;
  buffer_size = 0;
  while(1) {
    // Riceve la texture indietro
    while ( (buffer_size += recv(socket_desc, buffer + buffer_size, BUFFER_SIZE - buffer_size, 0)) < 0 ) {
      if (errno == EINTR) continue;
      ERROR_HELPER(-1, "[ERROR] Cannot read from socket!!!");
    }

    // Dimensione totale del pacchetto da ricevere
    actual_size = ((PacketHeader*) buffer)->size;

    // Se la dimensione del pacchetto ricevuto è ancora minore della dimensione del pacchetto totale aspetta le altre parti
    if (buffer_size < actual_size) {
      printf("[TCP] Next packet...\n");
      continue;
    }
    else {
      // printf("[TCP] Vehicle texture received back...\n");
      break;
    }
  }

  // Load della texture ricevuta
  ImagePacket* texture_back = (ImagePacket*) Packet_deserialize(buffer, buffer_size);
  *my_texture_from_server = texture_back->image;

  printf("[TCP] Received texture back from server...\n");

  // Libera la memoria non più utilizzata
  Packet_free(&packet->header);
  free(packet);
}


void serverHandshake (int* socket_desc, int* my_id, Image** my_texture, Image** map_elevation,Image** map_texture, Image** my_texture_from_server){
  int ret;
  printf("[TCP] Handsaking started...\n");

  // Apre la socket
  struct sockaddr_in server_addr = {0};
  *socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(*socket_desc, "[ERROR] Could not create socket!!!");
  if (*socket_desc >= 0) 
    printf("[TCP] Socket opened %d...\n", *socket_desc);

  server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
  server_addr.sin_family      = AF_INET;
  server_addr.sin_port        = htons(SERVER_PORT);

  // Si connette al server
  ret = connect(*socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in)); 
  ERROR_HELPER(ret, "[ERROR] Could not create connection!!!");  
  if (ret >= 0) printf("[TCP] Connection enstablished...\n");

  // Esegue i 4 scambi in TCP
  recv_ID(my_id, *socket_desc);
  recv_Texture(map_texture, *socket_desc);
  recv_Elevation(map_elevation, *socket_desc);
  send_Texture(my_texture, my_texture_from_server, *socket_desc);
  
  // TODO: Chiudere connessione in TCP col server e aprirne una UDP per le update

  return;
}

/* MAIN */
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
  
  //Image* my_texture_for_server;
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

  // Scambia messaggi TCP
  serverHandshake(&socket_tcp, &my_id, &my_texture, &map_elevation, &map_texture, &my_texture_from_server);

   // construct the world
  printf("[MAIN] Initializating world and vehicle (ID = %d)...\n", my_id);

  World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);
  printf("[MAIN] World initialized...\n");

  vehicle=(Vehicle*) malloc(sizeof(Vehicle));
  Vehicle_init(vehicle, &world, my_id, my_texture_from_server);
  printf("[MAIN] Vehicle initialized...\n");

  World_addVehicle(&world, vehicle);
  printf("[MAIN] Vehicle added...\n");
  
  // spawn a thread that will listen the update messages from
  // the server, and sends back the controls
  // the update for yourself are written in the desired_*_force
  // fields of the vehicle variable
  // when the server notifies a new player has joined the game
  // request the texture and add the player to the pool
  /*FILLME*/

  // TODO: far partire il mondo attraverso la funzione WorldViewer_runGlobal

  pthread_t world_runner;
  pthread_create(&world_runner, NULL, world_updater, NULL);
  printf("[MAIN] World thread created...\n");

  WorldViewer_runGlobal(&world, vehicle, &argc, argv);
  printf("[MAIN] Run global...\n");

  pthread_join(world_runner, NULL);

  /*
  uint16_t port_number_udp = htons((uint16_t)UDPPORT); // we use network byte order
	int socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
  ERROR_HELPER(socket_desc, "Can't create an UDP socket");

	struct sockaddr_in udp_server = {0}; // some fields are required to be filled with 0
  udp_server.sin_addr.s_addr = INADDR_ANY;
  udp_server.sin_family      = AF_INET;
  udp_server.sin_port        = port_number_udp;

  pthread_t UDP_sender,UDP_receiver;
  udpArgs udp_args;
  udp_args.socket_tcp=socket_desc;
  udp_args.server_addr=udp_server;
  udp_args.socket_udp=socket_udp;
    
  ret = pthread_create(&UDP_sender, NULL, UDP_sender, udp_args);
  ERROR_HELPER(ret,"can't create UDP_Sender thread");
    
  ret = pthread_create(&UDP_receiver, NULL, UDP_Receiver, udp_args);
  ERROR_HELPER(ret,"can't create UDP_Receiver thread");
  */

  // cleanup
  World_destroy(&world);
  return 0;             
  
        
}
