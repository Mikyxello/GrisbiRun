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

#define TIME_TO_SLEEP    100000
#define WORLD_SIZE       10
#define SERVER_ADDRESS   "127.0.0.1"
#define TCP_PORT         25252
#define UDP_PORT         8888
#define BUFFER_SIZE      1000000

typedef struct localWorld{
    int id_list[WORLD_SIZE];
    int players_online;
    Vehicle** vehicles;
} localWorld;

typedef struct {
    localWorld* lw;
    struct sockaddr_in server_addr;
    int id;
    int udp_socket;
} udp_args_t;

typedef struct {
  volatile int run;
  World* world;
} UpdaterArgs;

localWorld player_world;

int window;
WorldViewer viewer;
World world;
Vehicle* vehicle;

/* Invia ogni tot millisecondi l'aggiornamento del proprio veicolo al server in UDP -- TODO: Temporizzare l'invio di messaggi */
void* UDP_Sender(void* args){
  int ret;
  char buffer[BUFFER_SIZE];

  printf("[UDP SENDER] Sender thread started...\n");

  // Preparazione connessione
  udp_args_t* udp_args = (udp_args_t*) args;
  int udp_socket = udp_args->udp_socket;
  int id = udp_args->id;

  struct sockaddr_in server_addr = udp_args->server_addr;
  int sockaddr_len = sizeof(struct sockaddr_in);

  //printf("[UDP SENDER] Ready to send updates...\n");
  while(1) {
    // Preparazione aggiornamento da inviare al server
    //printf("[UDP SENDER] Sending new update...\n");
  	Vehicle_update(vehicle, 1);
    VehicleUpdatePacket* vehicle_packet = (VehicleUpdatePacket*) malloc(sizeof(VehicleUpdatePacket));

    // Header pacchetto    
    PacketHeader header;
    header.type = VehicleUpdate;

    // Contenuto del pacchetto
    vehicle_packet->header = header;
    vehicle_packet->id = id;
    vehicle_packet->rotational_force = vehicle->rotational_velocity;
    vehicle_packet->translational_force = vehicle->translational_velocity;
    
    printf("X: %f, Y: %f, THETA: %f, ROTATIONAL: %f, TRANSLATIONAL: %f\n", 
      floorf(vehicle->x * 100) / 100,
      floorf(vehicle->y * 100) / 100,
      floorf(vehicle->theta * 100) / 100,
      floorf(vehicle->rotational_velocity * 100) / 100,
      floorf(vehicle->translational_velocity * 100) / 100);

    // Serializzazione del pacchetto
    int buffer_size = Packet_serialize(buffer, &vehicle_packet->header);

    //printf("[UDP SENDER] Sending packet (%d)...\n", buffer_size);

    // Invio il pacchetto
    ret = sendto(udp_socket, buffer, buffer_size , 0, (struct sockaddr*) &server_addr, (socklen_t) sockaddr_len);
    ERROR_HELPER(ret,"[ERROR] Failed sending update to the server!!!");

    //printf("[UPD SENDER] Update sent...\n");

    World_update(&world);

    usleep(TIME_TO_SLEEP);
  }

  printf("[UDP SENDER] Closed sender...\n");

  pthread_exit(0);
}

/* Riceve gli update del mondo dal server e li carica nel proprio mondo */
void* UDP_Receiver(void* args){
  int ret;
  int buffer_size = 0;
  char buffer[BUFFER_SIZE];

  printf("[UDP RECEIVER] Receiving updates...\n");

  // Preparazione connessione
	udp_args_t* udp_args = (udp_args_t*) args;
  int udp_socket = udp_args->udp_socket;

	struct sockaddr_in server_addr = udp_args->server_addr;
  socklen_t addrlen = sizeof(struct sockaddr_in);

  // printf("[UDP RECEIVER] Ready to receive packets from server...\n");
  // Ricezione update
  while ( (ret = recvfrom(udp_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr*) &server_addr, &addrlen)) > 0){   
		ERROR_HELPER(ret, "[ERROR] Cannot receive packet from server!!!");
  
    // printf("[UDP RECEIVER] Received update...\n");

    buffer_size += ret;

    WorldUpdatePacket* world_update = (WorldUpdatePacket*) Packet_deserialize(buffer, buffer_size);
    //printf("[UDP RECEIVER] Loaded update...\n");

    // Aggiorna le posizioni dei veicoli

    for(int i=0; i < world_update->num_vehicles; i++) {
      ClientUpdate* client = &(world_update->updates[i]);

      Vehicle* client_vehicle = World_getVehicle(&world, client->id);

      //printf("[UDP RECEIVER] Vehicle id (%d)...\n", client_vehicle->id);

      if (client_vehicle == 0) {
        //printf("[UDP RECEIVER] Adding new player (%d)...\n", client->id);
        Vehicle* v = (Vehicle*) malloc(sizeof(Vehicle));
        Vehicle_init(v, &world, client->id, vehicle->texture);
        //printf("[MAIN] Vehicle initialized...\n");
        World_addVehicle(&world, v);
      }

      client_vehicle = World_getVehicle(&world, client->id);

      client_vehicle->x = client->x;
      client_vehicle->y = client->y;
      client_vehicle->theta = client->theta;
    }

    World_update(&world);

  }

  printf("[UDP RECEIVER] Closed receiver...\n");
  pthread_exit(0);
}

// this is the updater threas that takes care of refreshing the agent position
// in your client it is not needed, but you will
// have to copy the x,y,theta fields from the world update packet
// to the vehicles having the corrsponding id
void* updater_thread(void* args_){
  UpdaterArgs* args=(UpdaterArgs*)args_;
  while(args->run){
    World_update(args->world);
    usleep(TIME_TO_SLEEP);
  }
  return 0;
}


 /* ----------------- */
 /* GetId dell'utente */
 /* ----------------- */
void recv_ID(int* my_id, int tcp_socket) {
  int ret;
  char buffer[BUFFER_SIZE];

  printf("[TCP] Requesting ID...\n");
  
  // Preparazione pacchetto da inviare 
  PacketHeader header;
  header.type = GetId;
  
  IdPacket* packet = (IdPacket*)malloc(sizeof(IdPacket));
  packet->header = header;
  packet->id = -1;

  int buffer_size = Packet_serialize(buffer, &packet->header);

  // Invia la richiesta
  while ( (ret = send(tcp_socket, buffer, buffer_size, 0)) < 0 ) {
    if (errno == EINTR) continue;
    ERROR_HELPER(-1, "[ERROR] Cannot write to socket!!!");
  }

  // printf("[TCP] ID request sent...\n");
  // printf("[TCP] Waiting for ID receiving...\n");

  // Riceve l'id
  while ( (buffer_size = recv(tcp_socket, buffer, BUFFER_SIZE, 0)) < 0 ) { //ricevo ID dal server
    if (errno == EINTR) continue;
    ERROR_HELPER(-1, "[ERROR] Cannot read from socket!!!");
  }

  // printf("[TCP] ID received...\n");

  // Prende l'id ricevuto e lo inserisce in my_id
  IdPacket* id_recv = (IdPacket*) Packet_deserialize(buffer,buffer_size);
  *my_id = id_recv->id;

  printf("[TCP] ID received: %d...\n", *my_id);
}

/* ------------------------------------ */
/* GetTexture della texture della mappa */
/* ------------------------------------ */
void recv_Texture(Image** map_texture, int tcp_socket) {
  int ret;
  char buffer[BUFFER_SIZE];

  printf("[TCP] Requesting map texture...\n");

  // Preparazione pacchetto da inviare
  PacketHeader header;
  header.type = GetTexture;

  ImagePacket* packet = (ImagePacket*) malloc(sizeof(ImagePacket));
  packet->image = NULL;
  packet->header = header;

  int buffer_size = Packet_serialize(buffer, &packet->header);

  // Invia la richiesta
  while ( (ret = send(tcp_socket, buffer, buffer_size, 0)) < 0 ) {
    if (errno == EINTR) continue;
    ERROR_HELPER(-1, "[ERROR] Cannot write to socket!!!");
  }

  //printf("[TCP] Map texture request sent...\n");
  //printf("[TCP] Waiting for texture receiving...\n");

  int actual_size = 0;
  buffer_size = 0;

  // Riceve la texture della mappa
  while(1) {
    while ( (buffer_size += recv(tcp_socket, buffer + buffer_size, BUFFER_SIZE - buffer_size, 0)) < 0 ) {
      if (errno == EINTR) continue;
      ERROR_HELPER(-1, "[ERROR] Cannot read from socket!!!");
    }

    //printf("[TCP] Map texture received...\n");

    // Dimensione totale del pacchetto da ricevere
    actual_size = ((PacketHeader*) buffer)->size;

    // Se la dimensione del pacchetto ricevuto è ancora minore della dimensione del pacchetto totale aspetta le altre parti
    if (buffer_size < actual_size) {
      // printf("[TCP] Next packet...\n");
      continue;
    }
    else break;
  }

  // Load della texture della mappa
  ImagePacket* texture_recv = (ImagePacket*) Packet_deserialize(buffer, buffer_size);
  *map_texture = texture_recv->image;

  printf("[TCP] Map texture loaded...\n");
}

/* -------------------------------------- */
/* GetElevation della texture della mappa */
/* -------------------------------------- */
void recv_Elevation(Image** map_elevation, int tcp_socket) {
  int ret;
  char buffer[BUFFER_SIZE];

  printf("[TCP] Requesting map elevation...\n");
  PacketHeader header;
  header.type = GetElevation;

  ImagePacket* packet = (ImagePacket*) malloc(sizeof(ImagePacket));
  packet->image = NULL;
  packet->header = header;

  // Serializza il pacchetto
  int buffer_size = Packet_serialize(buffer, &packet->header);

  // Invia la richiesta
  while ( (ret = send(tcp_socket, buffer, buffer_size, 0)) < 0 ) {
    if (errno == EINTR) continue;
    ERROR_HELPER(-1, "[ERROR] Cannot write to socket!!!");
  }

  //printf("[TCP] Elevation request sent...\n");
  //printf("[TCP] Waiting for elevation receiving...\n");

  int actual_size = 0;
  buffer_size = 0;
  while(1) {
    // Riceve la elevation della mappa
    while ( (buffer_size += recv(tcp_socket, buffer + buffer_size, BUFFER_SIZE - buffer_size, 0)) < 0 ) {
      if (errno == EINTR) continue;
      ERROR_HELPER(-1, "[ERROR] Cannot read from socket!!!");
    }

    //printf("[TCP] Elevation received...\n");

    // Dimensione totale del pacchetto da ricevere
    actual_size = ((PacketHeader*) buffer)->size;

    // Se la dimensione del pacchetto ricevuto è ancora minore della dimensione del pacchetto totale aspetta le altre parti
    if (buffer_size < actual_size) {
      //printf("[TCP] Next packet...\n");
      continue;
    }
    else break;
  }

  // Load della elevation della mappa
  ImagePacket* elevation_packet = (ImagePacket*) Packet_deserialize(buffer, buffer_size);
  *map_elevation = elevation_packet->image;

  printf("[TCP] Map elevation loaded...\n");
}

/* ------------------------------------- */
/* PostTexture della texture del veicolo */
/* ------------------------------------- */
void send_Texture(Image** my_texture, Image** my_texture_from_server, int tcp_socket) {
  int ret;
  char buffer[BUFFER_SIZE];

  printf("[TCP] Sending texture to server...\n");

  // Preparazione del pacchetto da inviare
  PacketHeader header;
  header.type = PostTexture;

  ImagePacket* packet = (ImagePacket*)malloc(sizeof(ImagePacket));
  packet->image = *my_texture;
  packet->header = header;

  int buffer_size = Packet_serialize(buffer, &packet->header);
  
  // Invia la texture del veicolo
  while ( (ret = send(tcp_socket, buffer, buffer_size, 0)) < 0) {
    if (errno == EINTR) continue;
    ERROR_HELPER(ret, "[ERROR] Cannot write to socket!!!");
  }

  //printf("[TCP] Vehicle texture sent...\n");
  //printf("[TCP] Waiting for texture receiving back...\n");


  // Riceve la texture indietro
  int actual_size = 0;
  buffer_size = 0;
  while(1) {
    while ( (buffer_size += recv(tcp_socket, buffer + buffer_size, BUFFER_SIZE - buffer_size, 0)) < 0 ) {
      if (errno == EINTR) continue;
      ERROR_HELPER(-1, "[ERROR] Cannot read from socket!!!");
    }

    // Dimensione totale del pacchetto da ricevere
    actual_size = ((PacketHeader*) buffer)->size;

    // Se la dimensione del pacchetto ricevuto è ancora minore della dimensione del pacchetto totale aspetta le altre parti
    if (buffer_size < actual_size) {
      //printf("[TCP] Next packet...\n");
      continue;
    }
    else {
      //printf("[TCP] Received texture back from server...\n");
      break;
    }
  }

  // Load della texture ricevuta
  ImagePacket* texture_back = (ImagePacket*) Packet_deserialize(buffer, buffer_size);
  *my_texture_from_server = texture_back->image;

  printf("[TCP] Vehicle texture loaded...\n");
}


void serverHandshake (int tcp_socket, int* my_id, Image** my_texture, Image** map_elevation,Image** map_texture, Image** my_texture_from_server){
  //int ret;
  printf("[TCP] Handsaking started...\n");

  // Esegue le 4 operazioni per ottenere id, texture e elevation e inviare la propria texture
  recv_ID(my_id, tcp_socket);
  recv_Texture(map_texture, tcp_socket);
  recv_Elevation(map_elevation, tcp_socket);
  send_Texture(my_texture, my_texture_from_server, tcp_socket);

  printf("[TCP] Handshake ended...\n");

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
  int ret;
  int udp_socket;
  int tcp_socket;

  // Apertura connessione TCP
  struct sockaddr_in server_addr = {0};

  tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(tcp_socket, "[ERROR] Could not create socket!!!");
  //printf("[MAIN] Socket TCP opened %d...\n", tcp_socket);

  server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
  server_addr.sin_family      = AF_INET;
  server_addr.sin_port        = htons(TCP_PORT);

  ret = connect(tcp_socket, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in)); 
  ERROR_HELPER(ret, "[ERROR] Could not create connection!!!"); 
  printf("[MAIN] Connection enstablished with server...\n");

  // Apertura connessione UDP
  udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
  ERROR_HELPER(udp_socket, "[ERROR] Can't create an UDP socket!!!");
  //printf("[MAIN] Socket UDP opened...\n");

  struct sockaddr_in udp_server = {0}; // some fields are required to be filled with 0
  udp_server.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
  udp_server.sin_family      = AF_INET;
  udp_server.sin_port        = htons(UDP_PORT);


  // Scambia messaggi TCP
  serverHandshake(tcp_socket, &my_id, &my_texture, &map_elevation, &map_texture, &my_texture_from_server);

  // construct the world
  //printf("[MAIN] Initializating world and vehicle (ID = %d)...\n", my_id);

  World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);
  //printf("[MAIN] World initialized...\n");

  vehicle=(Vehicle*) malloc(sizeof(Vehicle));
  Vehicle_init(vehicle, &world, my_id, my_texture_from_server);
  //printf("[MAIN] Vehicle initialized...\n");

  World_addVehicle(&world, vehicle);
  //printf("[MAIN] Vehicle added...\n");
  
  // spawn a thread that will listen the update messages from
  // the server, and sends back the controls
  // the update for yourself are written in the desired_*_force
  // fields of the vehicle variable
  // when the server notifies a new player has joined the game
  // request the texture and add the player to the pool
  /*FILLME*/
  
  pthread_t UDP_sender, UDP_receiver, runner_thread;

  udp_args_t udp_args;
  udp_args.server_addr = udp_server;
  udp_args.id = my_id;
  udp_args.udp_socket = udp_socket;

  pthread_attr_t runner_attrs;
  UpdaterArgs runner_args={
    .run=1,
    .world=&world
  };
  
  pthread_attr_init(&runner_attrs);
  runner_args.run=0;
  void* retval;
  
  
  //printf("[MAIN] Creating threads...\n");

  ret = pthread_create(&UDP_sender, NULL, UDP_Sender, &udp_args);
  PTHREAD_ERROR_HELPER(ret, "[ERROR] Can't create UDP Sender thread!!!");

  ret = pthread_create(&UDP_receiver, NULL, UDP_Receiver, &udp_args);
  PTHREAD_ERROR_HELPER(ret, "[ERROR] Can't create UDP receiver thread!!!");

  ret = pthread_create(&runner_thread, &runner_attrs, updater_thread, &runner_args);
  PTHREAD_ERROR_HELPER(ret, "[ERROR] Can't create runner thread!!!");

  //printf("[MAIN] Threads created...\n");

  WorldViewer_runGlobal(&world, vehicle, &argc, argv);

  //printf("[MAIN] Joining threads...\n");

  ret = pthread_join(UDP_sender, NULL);
  PTHREAD_ERROR_HELPER(ret, "[ERROR] Cannot join UDP sender thread!!!");

  ret = pthread_join(UDP_receiver, NULL);
  PTHREAD_ERROR_HELPER(ret, "[ERROR] Cannot join UDP receiver thread!!!");

  ret = pthread_join(runner_thread, &retval);
  PTHREAD_ERROR_HELPER(ret, "[ERROR] Cannot run world!!!\n");

  //printf("[MAIN] Thrads joined...\n");
  //printf("[MAIN] Closing...\n");

  // cleanup
  World_destroy(&world);
  return 0;             
  
        
}
