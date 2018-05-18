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

#define TIME_TO_SLEEP    10000
#define WORLD_SIZE       10
#define SERVER_ADDRESS   "127.0.0.1"
#define TCP_PORT         25252
#define UDP_PORT         8888
#define BUFFER_SIZE      1000000

typedef struct {
    struct sockaddr_in server_addr;
    int id;
} udp_args_t;

typedef struct {
    struct sockaddr_in server_addr;
    int id;
} tcp_args_t;

typedef struct {
  volatile int run;
  World* world;
} UpdaterArgs;

int window;
WorldViewer viewer;
World world;
Vehicle* vehicle;

char* server_address = SERVER_ADDRESS;

int running;
int udp_socket, tcp_socket;
pthread_t TCP_connection, UDP_sender, UDP_receiver, runner_thread;

Image* map_elevation;
Image* map_texture;
Image* my_texture_from_server;

/* Funzione per il cleanup generico della memoria */
void cleanMemory(void) {
  int ret;

  running = 0;

  ret = pthread_cancel(TCP_connection);
  ERROR_HELPER(ret, "[ERROR] Cannot terminate the TCP connection thread!!!");

  ret = pthread_cancel(UDP_sender);
  ERROR_HELPER(ret, "[ERROR] Cannot terminate the UDP sender thread!!!");

  ret = pthread_cancel(UDP_receiver);
  ERROR_HELPER(ret, "[ERROR] Cannot terminate the UDP receiver thread!!!");

  ret = pthread_cancel(runner_thread);
  ERROR_HELPER(ret, "[ERROR] Cannot terminate the world runner thread!!!");

  ret = close(tcp_socket);
  ERROR_HELPER(ret, "[ERROR] Cannot close TCP socket!!!");

  ret = close(udp_socket);
  ERROR_HELPER(ret, "[ERROR] Cannot close UDP socket!!!");

  World_destroy(&world);
  Image_free(map_elevation);
  Image_free(map_texture);
  Image_free(my_texture_from_server);

  printf("[CLEANUP] Memory cleaned...\n");


   printf("   _____      _     _     _ ____             \n");
   printf("  / ____|    (_)   | |   (_)  _ \\            \n");
   printf(" | |  __ _ __ _ ___| |__  _| |_) |_   _  ___ \n");
   printf(" | | |_ | '__| / __| '_ \\| |  _ <| | | |/ _ \\ \n");
   printf(" | |__| | |  | \\__ \\ |_) | | |_) | |_| |  __/\n");
   printf("  \\_____|_|  |_|___/_.__/|_|____/ \\__, |\\___| \n");
   printf("                                   __/ |     \n");
   printf("                                  |___/      \n");
   printf("\n");


  return;
}

/* Funzione per la gestione dei segnali */
void signalHandler(int signal){
  switch (signal) {
  case SIGHUP:
    printf("\n[CLOSING] The game is closing...\n"); 
    cleanMemory();
    exit(1);
  case SIGINT:
    printf("\n[CLOSING] The game is closing...\n");
    cleanMemory();
    exit(1);
  case SIGTERM:
    printf("\n[CLOSING] The game is closing...\n");
    cleanMemory();
    exit(1);
  case SIGSEGV:
    printf("[ERROR] Segmentation fault!!!\n");
    return;
  default:
    printf("[ERROR] Uncaught signal: %d...\n", signal);
    return;
  }
}

/* Invia ogni tot millisecondi l'aggiornamento del proprio veicolo al server in UDP -- TODO: Temporizzare l'invio di messaggi */
void* UDP_Sender(void* args){
  int ret;
  char buffer[BUFFER_SIZE];

  printf("[UDP SENDER] Sender thread started...\n");

  // Preparazione connessione
  udp_args_t* udp_args = (udp_args_t*) args;
  int id = udp_args->id;

  struct sockaddr_in server_addr = udp_args->server_addr;
  int sockaddr_len = sizeof(struct sockaddr_in);

  while(running) {
    // Preparazione aggiornamento da inviare al server
    VehicleUpdatePacket* vehicle_packet = (VehicleUpdatePacket*) malloc(sizeof(VehicleUpdatePacket));

    // Header pacchetto    
    PacketHeader header;
    header.type = VehicleUpdate;

    // Contenuto del pacchetto
    vehicle_packet->header = header;
    vehicle_packet->id = id;

    vehicle_packet->rotational_force = vehicle->rotational_force_update;
    vehicle_packet->translational_force = vehicle->translational_force_update;
    
    // Stampa i valori attuali della posizione e le forze
    /*printf("X: %.2f, Y: %.2f, THETA: %.2f, ROTATIONAL: %.2f, TRANSLATIONAL: %.2f\n", 
      vehicle->x,
      vehicle->y,
      vehicle->theta,
      vehicle->rotational_force_update,
      vehicle->translational_force_update);*/

    // Serializzazione del pacchetto
    int buffer_size = Packet_serialize(buffer, &vehicle_packet->header);

    // Invio il pacchetto
    ret = sendto(udp_socket, buffer, buffer_size , 0, (struct sockaddr*) &server_addr, (socklen_t) sockaddr_len);
    ERROR_HELPER(ret,"[ERROR] Failed sending update to the server!!!");

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

  struct sockaddr_in server_addr = udp_args->server_addr;
  socklen_t addrlen = sizeof(struct sockaddr_in);

  // Ricezione update
  while ( (ret = recvfrom(udp_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr*) &server_addr, &addrlen)) > 0){   
  	ERROR_HELPER(ret, "[ERROR] Cannot receive updates from server!!!");
  
    buffer_size += ret;

    // Deserializza il pacchetto degli aggiornamenti
    WorldUpdatePacket* world_update = (WorldUpdatePacket*) Packet_deserialize(buffer, buffer_size);

    // Aggiorna le posizioni dei veicoli
    for(int i=0; i < world_update->num_vehicles; i++) {
      ClientUpdate* client = &(world_update->updates[i]);

      Vehicle* client_vehicle = World_getVehicle(&world, client->id);

      if (client_vehicle == 0) continue;

      client_vehicle = World_getVehicle(&world, client->id);
      client_vehicle->x = client->x;
      client_vehicle->y = client->y;
      client_vehicle->theta = client->theta;

      World_update(&world);
    }
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

 /* GetId dell'utente */
void recv_ID(int* my_id) {
  int ret;
  char buffer[BUFFER_SIZE];

  printf("[TCP ID] Requesting ID...\n");
  
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
    ERROR_HELPER(-1, "[ERROR] Cannot write to socket sending id request!!!");
  }

  // Riceve l'id
  while ( (buffer_size = recv(tcp_socket, buffer, BUFFER_SIZE, 0)) < 0 ) { //ricevo ID dal server
    if (errno == EINTR) continue;
    ERROR_HELPER(-1, "[ERROR] Cannot read from socket receiving assigned id!!!");
  }

  // Prende l'id ricevuto e lo inserisce in my_id
  IdPacket* id_recv = (IdPacket*) Packet_deserialize(buffer,buffer_size);
  *my_id = id_recv->id;

  printf("[TCP ID] ID received: %d...\n", *my_id);
}

/* GetTexture della texture della mappa */
void recv_Texture(Image** map_texture) {
  int ret;
  char buffer[BUFFER_SIZE];

  printf("[TCP MAP TEXTURE] Requesting map texture...\n");

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
    ERROR_HELPER(-1, "[ERROR] Cannot write to socket sending map texture request!!!");
  }

  int actual_size = 0;
  buffer_size = 0;

  // Riceve la texture della mappa
  while(1) {
    while ( (buffer_size += recv(tcp_socket, buffer + buffer_size, BUFFER_SIZE - buffer_size, 0)) < 0 ) {
      if (errno == EINTR) continue;
      ERROR_HELPER(-1, "[ERROR] Cannot read from socket receiving map texture!!!");
    }

    // Dimensione totale del pacchetto da ricevere
    actual_size = ((PacketHeader*) buffer)->size;

    // Se la dimensione del pacchetto ricevuto è ancora minore della dimensione del pacchetto totale aspetta le altre parti
    if (buffer_size < actual_size) continue;
    else break;
  }

  // Load della texture della mappa
  ImagePacket* texture_recv = (ImagePacket*) Packet_deserialize(buffer, buffer_size);
  *map_texture = texture_recv->image;

  printf("[TCP MAP TEXTURE] Map texture loaded...\n");
}

/* GetElevation della texture della mappa */
void recv_Elevation(Image** map_elevation) {
  int ret;
  char buffer[BUFFER_SIZE];

  printf("[TCP MAP ELEVATION] Requesting map elevation...\n");

  // Preparazione del pacchetto da inviare
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
    ERROR_HELPER(-1, "[ERROR] Cannot write to socket sending map elevation request!!!");
  }

  int actual_size = 0;
  buffer_size = 0;
  while(1) {
    // Riceve la elevation della mappa
    while ( (buffer_size += recv(tcp_socket, buffer + buffer_size, BUFFER_SIZE - buffer_size, 0)) < 0 ) {
      if (errno == EINTR) continue;
      ERROR_HELPER(-1, "[ERROR] Cannot read from socket receiving map elevation!!!");
    }

    // Dimensione totale del pacchetto da ricevere
    actual_size = ((PacketHeader*) buffer)->size;

    // Se la dimensione del pacchetto ricevuto è ancora minore della dimensione del pacchetto totale aspetta le altre parti
    if (buffer_size < actual_size) continue;
    else break;
  }

  // Load della elevation della mappa
  ImagePacket* elevation_packet = (ImagePacket*) Packet_deserialize(buffer, buffer_size);
  *map_elevation = elevation_packet->image;

  printf("[TCP MAP ELEVATION] Map elevation loaded...\n");
}

/* PostTexture della texture del veicolo */
void send_Texture(Image** my_texture, Image** my_texture_from_server) {
  int ret;
  char buffer[BUFFER_SIZE];

  printf("[TCP VEHICLE TEXTURE] Sending texture to server...\n");

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
    ERROR_HELPER(ret, "[ERROR] Cannot write to socket sending vehicle texture!!!");
  }

  printf("[TCP VEHICLE TEXTURE] Vehicle texture sent...\n");

  // Riceve la texture indietro
  int actual_size = 0;
  buffer_size = 0;
  while(1) {
    while ( (buffer_size += recv(tcp_socket, buffer + buffer_size, BUFFER_SIZE - buffer_size, 0)) < 0 ) {
      if (errno == EINTR) continue;
      ERROR_HELPER(-1, "[ERROR] Cannot read from socket receiving vehicle texture!!!");
      if (ret == 0) {
        printf("[ERROR] ...\n");
      }
    }

    // Dimensione totale del pacchetto da ricevere
    actual_size = ((PacketHeader*) buffer)->size;

    // Se la dimensione del pacchetto ricevuto è ancora minore della dimensione del pacchetto totale aspetta le altre parti
    if (buffer_size < actual_size) continue;
    else break;
  }

  // Load della texture ricevuta
  ImagePacket* texture_back = (ImagePacket*) Packet_deserialize(buffer, buffer_size);
  *my_texture_from_server = texture_back->image;

  printf("[TCP VEHICLE TEXTURE] Received texture back from server...\n");
}

/* Riceve i nuovi utenti e gli utenti disconnessi in TCP */
void* TCP_connections_receiver(void* args) {

	printf("[TCP CONNECTION CONTROLLER] Connection controller running...\n");

	while(running) {
		char buffer[BUFFER_SIZE];
		int actual_size = 0;
		int buffer_size = 0;
    int ret=0;

		while(1) {
  		while ( (buffer_size += recv(tcp_socket, buffer + buffer_size, BUFFER_SIZE - buffer_size, 0)) < 0 ) {
  			if (errno == EINTR) continue;
  			ERROR_HELPER(-1, "[ERROR] Cannot read from socket on connections controller!!!");
  		}

  		actual_size = ((PacketHeader*) buffer)->size;

      if (buffer_size < actual_size) continue;

      PacketHeader* head = (PacketHeader*) Packet_deserialize(buffer, actual_size);

      // Se si connette un user
  		if(head->type == UserConnected) {
				// Load della texture ricevuta
				ImagePacket* texture_back = (ImagePacket*) Packet_deserialize(buffer, actual_size);
				Image* new_texture_user = texture_back->image;

		    Vehicle* v = (Vehicle*) malloc(sizeof(Vehicle));
		    Vehicle_init(v, &world, texture_back->id, new_texture_user);
		    World_addVehicle(&world, v);

        printf("[USER CONNECTED] User %d joined the game...\n", texture_back->id);

        // Invia conferma al server

        PacketHeader* pack = (PacketHeader*)malloc(sizeof(PacketHeader));
        pack->type = ClientReady;

        actual_size = Packet_serialize(buffer, pack);


        while ( (ret = send(tcp_socket, buffer,actual_size , 0)) < 0) {
          if (errno == EINTR) continue;
          ERROR_HELPER(ret, "[ERROR] Cannot write to socket client is becoming ready!!!\n");
        }
        free(pack);
		break;
			}

      // Se si disconnette un user
			else if(head->type == UserDisconnected) {
				IdPacket* id_disconnected = (IdPacket*) Packet_deserialize(buffer, buffer_size);

				Vehicle* vehicle_to_delete = World_getVehicle(&world, id_disconnected->id);
				World_detachVehicle(&world, vehicle_to_delete);
				Vehicle_destroy(vehicle_to_delete);

        printf("[USER DISCONNECTED] User %d left the game...\n", id_disconnected->id);

				break;
			}

      else {
        printf("[TCP CONNECTION CONTROLLER] Received unknown packet from server: %d...\n", head->type);
        continue;
      }
		}
	}

	printf("[TCP CONNECTION CONTROLLER] Connection controller stopped...\n");

  pthread_exit(0);
}

/* Riceve l'id e le texture dal server e invia la texture del proprio veicolo */
void serverHandshake (int* my_id, Image** my_texture, Image** map_elevation,Image** map_texture, Image** my_texture_from_server){
  // Esegue le 4 operazioni per ottenere id, texture e elevation e inviare la propria texture
  recv_ID(my_id);
  recv_Texture(map_texture);
  recv_Elevation(map_elevation);
  send_Texture(my_texture, my_texture_from_server);

  return;
}

/* Main */
int main(int argc, char **argv) {
  running = 0;

  if (argc<3) {
    printf("usage: %s <server_address> <player texture>\n", argv[1]);
    exit(-1);
  }

printf(" _____        _       _      _  ______              \n");
printf("|  __ \\      (_)     | |    (_) | ___ \\             \n");
printf("| |  \\/ _ __  _  ___ | |__   _  | |_/ /_   _  _ __  \n");
printf("| | __ | '__|| |/ __|| '_ \\ | | |    /| | | || '_ \\ \n");
printf("| |_\\ \\| |   | |\\__ \\| |_) || | | |\\ \\| |_| || | | |\n");
printf(" \\____/|_|   |_||___/|_.__/ |_| \\_| \\_|\\__,_||_| |_|\n");
printf("\n");

  int ret;

  // Inizializzazione del signal handler
  struct sigaction signal_action;
  signal_action.sa_handler = signalHandler;
  signal_action.sa_flags = SA_RESTART;

  sigfillset(&signal_action.sa_mask);
  ret = sigaction(SIGHUP, &signal_action, NULL);
  ERROR_HELPER(ret,"[ERROR] Cannot handle SIGHUP!!!");
  ret = sigaction(SIGINT, &signal_action, NULL);
  ERROR_HELPER(ret,"[ERROR] Cannot handle SIGINT!!!");
  ret = sigaction(SIGSEGV, &signal_action, NULL);
  ERROR_HELPER(ret,"[ERROR] Cannot handle SIGSEGV!!!");
  ret = sigaction(SIGTERM, &signal_action, NULL);
  ERROR_HELPER(ret,"[ERROR] Cannot handle SIGTERM!!!");

  //printf("loading texture image from %s ... ", argv[2]);
  Image* my_texture = Image_load(argv[2]);
  if (my_texture) {
    //printf("Done! \n");
  } else {
    //printf("Fail! \n");
  }

  server_address = argv[1];
  
  //Image* my_texture_for_server;
  // todo: connect to the server
  //   -get ad id
  //   -send your texture to the server (so that all can see you)
  //   -get an elevation map
  //   -get the texture of the surface

  // these come from the server
  int my_id;

  // Apertura connessione TCP
  struct sockaddr_in server_addr = {0};

  tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(tcp_socket, "[ERROR] Could not create socket!!!");

  server_addr.sin_addr.s_addr = inet_addr(server_address);
  server_addr.sin_family      = AF_INET;
  server_addr.sin_port        = htons(TCP_PORT);

  ret = connect(tcp_socket, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in)); 
  ERROR_HELPER(ret, "[ERROR] Could not create connection!!!"); 
  printf("[MAIN] Connection enstablished with server...\n");

  // Apertura connessione UDP
  udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
  ERROR_HELPER(udp_socket, "[ERROR] Can't create an UDP socket!!!");

  struct sockaddr_in udp_server = {0}; // some fields are required to be filled with 0
  udp_server.sin_addr.s_addr = inet_addr(server_address);
  udp_server.sin_family      = AF_INET;
  udp_server.sin_port        = htons(UDP_PORT);

  running = 1;

  // Richiede l'ID, la texture e l'elevation della mappa e invia la propria texture al server che la rimanda indietro
  serverHandshake(&my_id, &my_texture, &map_elevation, &map_texture, &my_texture_from_server);

  // Carica il mondo con le texture e elevation ricevute
  World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);

  // Aggiunge il nostro veicolo al mondo locale
  vehicle=(Vehicle*) malloc(sizeof(Vehicle));
  Vehicle_init(vehicle, &world, my_id, my_texture_from_server);
  World_addVehicle(&world, vehicle);
  
  // spawn a thread that will listen the update messages from
  // the server, and sends back the controls
  // the update for yourself are written in the desired_*_force
  // fields of the vehicle variable
  // when the server notifies a new player has joined the game
  // request the texture and add the player to the pool
  /*FILLME*/

  udp_args_t udp_args;
  udp_args.server_addr = udp_server;
  udp_args.id = my_id;

  pthread_attr_t runner_attrs;
  UpdaterArgs runner_args={
    .run=1,
    .world=&world
  };
  
  pthread_attr_init(&runner_attrs);
  runner_args.run=0;
  void* retval;
  
  ret = pthread_create(&TCP_connection, NULL, TCP_connections_receiver, NULL);
  PTHREAD_ERROR_HELPER(ret, "[ERROR] Can't create TCP connection receiver thread!!!");

  ret = pthread_create(&UDP_sender, NULL, UDP_Sender, &udp_args);
  PTHREAD_ERROR_HELPER(ret, "[ERROR] Can't create UDP Sender thread!!!");

  ret = pthread_create(&UDP_receiver, NULL, UDP_Receiver, &udp_args);
  PTHREAD_ERROR_HELPER(ret, "[ERROR] Can't create UDP receiver thread!!!");

  ret = pthread_create(&runner_thread, &runner_attrs, updater_thread, &runner_args);
  PTHREAD_ERROR_HELPER(ret, "[ERROR] Can't create runner thread!!!");

  // Apre la schermata di gioco
  WorldViewer_runGlobal(&world, vehicle, &argc, argv);

  ret = pthread_join(TCP_connection, NULL);
  PTHREAD_ERROR_HELPER(ret, "[ERROR] Cannot join TCP connection thread!!!");

  ret = pthread_join(UDP_sender, NULL);
  PTHREAD_ERROR_HELPER(ret, "[ERROR] Cannot join UDP sender thread!!!");

  ret = pthread_join(UDP_receiver, NULL);
  PTHREAD_ERROR_HELPER(ret, "[ERROR] Cannot join UDP receiver thread!!!");

  ret = pthread_join(runner_thread, &retval);
  PTHREAD_ERROR_HELPER(ret, "[ERROR] Cannot run world!!!\n");

  //cleanMemory();

  return 0;  
}
