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

/* Struttura per args dei threads in TCP */
typedef struct {
	int client_desc;
	Image* elevation_texture;
	Image* surface_elevation;
} tcp_args_t;

int tcp_socket_desc,
	udp_socket_recv,
	udp_socket_send;
World world;


/* Gestione pacchetti TCP ricevuti */
int TCP_packet (int tcp_socket_desc, int id, char* buffer, Image* surface_elevation, Image* elevation_texture) {
  PacketHeader* header = (PacketHeader*) buffer;  // Pacchetto per controllo del tipo di richiesta

  /* Se la richiesta dal client a questo server è per l'ID (invia l'id assegnato al client che lo richiede) */
  if (header->type == GetId) {
    // Crea un IdPacket utilizzato per mandare l'id assegnato dal server al client (specifica struct per ID)
    IdPacket* id_to_send = (IdPacket*) malloc(sizeof(IdPacket));

    PacketHeader header_send;
    header_send.type = GetId;
    
    id_to_send->header = header_send;
    id_to_send->id = id;  // Gli assegno l'id passato da funzione TCPHandler

    char buffer_send[1000000];
    int pckt_length = Packet_serialize(buffer_send, &(id_to_send->header)); // Ritorna il numero di bytes scritti

    // Invio del messaggio tramite socket
    int bytes_sent = 0;
    int ret;
    while(bytes_sent < pckt_length){
      ret = send(tcp_socket_desc, buffer_send + bytes_sent, pckt_length - bytes_sent,0);
      if (ret==-1 && errno==EINTR) continue;
      ERROR_HELPER(ret, "Errore nell'assegnazione dell'ID!");
      if (ret==0) break;
      bytes_sent += ret;
    }

    Packet_free(&(id_to_send->header));   // Libera la memoria del pacchetto non più utilizzato
    free(id_to_send);

    fprintf(stdout, "[ID Sent] %d!\n", id);  // DEBUG OUTPUT

    return 1;
  }

  /* Se la richiesta dal client a questo server è per la texture della mappa */
  else if (header->type == GetTexture) {
    // Converto il pacchetto ricevuto in un ImagePacket per estrarne la texture richiesta
    ImagePacket* texture_request = (ImagePacket*) buffer;
    int id_request = texture_request->id;
    
    // Preparo header per la risposta
    PacketHeader header_send;
    header_send.type = PostTexture;
    
    // Preparo il pacchetto per inviare la texture al client
    ImagePacket* texture_to_send = (ImagePacket*) malloc(sizeof(ImagePacket));
    texture_to_send->header = header_send;
    texture_to_send->id = id_request;
    texture_to_send->image = elevation_texture;

    char buffer_send[1000000];
    int pckt_length = Packet_serialize(buffer_send, &(texture_to_send->header)); // Ritorna il numero di bytes scritti

    // Invio del messaggio tramite socket
    int bytes_sent = 0;
    int ret;
    while(bytes_sent < pckt_length){
      ret = send(tcp_socket_desc, buffer_send + bytes_sent, pckt_length - bytes_sent,0);
      if (ret==-1 && errno==EINTR) continue;
      ERROR_HELPER(ret, "Errore nella richiesta della Texture!");
      if (ret==0) break;
      bytes_sent += ret;
    }

    Packet_free(&(texture_to_send->header));   // Libera la memoria del pacchetto non più utilizzato
    free(texture_to_send);

    fprintf(stdout, "[Texture Sent] %d!\n", id);   // DEBUG OUTPUT

    return 1;
  }

  /* Se la richiesta dal client a questo server è per la elevation surface */
  else if (header->type == GetElevation) {
    // Converto il pacchetto ricevuto in un ImagePacket per estrarne la elevation richiesta
    ImagePacket* elevation_request = (ImagePacket*) buffer;
    int id_request = elevation_request->id;
    
    // Preparo header per la risposta
    PacketHeader header_send;
    header_send.type = PostElevation;
    
    // Preparo il pacchetto per inviare la elevation al client
    ImagePacket* elevation_to_send = (ImagePacket*) malloc(sizeof(ImagePacket));
    elevation_to_send->header = header_send;
    elevation_to_send->id = id_request;
    elevation_to_send->image = surface_elevation;

    char buffer_send[1000000];
    int pckt_length = Packet_serialize(buffer_send, &(elevation_to_send->header)); // Ritorna il numero di bytes scritti

    // Invio del messaggio tramite socket
    int bytes_sent = 0;
    int ret;
    while(bytes_sent < pckt_length){
      ret = send(tcp_socket_desc, buffer_send + bytes_sent, pckt_length - bytes_sent,0);
      if (ret==-1 && errno==EINTR) continue;
      ERROR_HELPER(ret, "Errore nella richiesta della Elevation!");
      if (ret==0) break;
      bytes_sent += ret;
    }

    Packet_free(&(elevation_to_send->header));   // Libera la memoria del pacchetto non più utilizzato
    free(elevation_to_send);

    fprintf(stdout, "[Elevation Sent] %d!\n", id);   // DEBUG OUTPUT

    return 1;
  }

  /* Se il server riceve una texture dal client */
  else if (header->type == PostTexture) {
    PacketHeader* received_header = Packet_deserialize(buffer, header->size);
    ImagePacket* received_texture = (ImagePacket*) received_header;
    
    Vehicle* new_vehicle = malloc(sizeof(Vehicle));
    Vehicle_init(new_vehicle, &world, id, received_texture->image);
    World_addVehicle(&world, new_vehicle);

   	Packet_free(received_header);	// Libera la memoria del pacchetto non più utilizzato
    free(received_texture);

    fprintf(stdout, "[Texture Received] %d!\n", id);   // DEBUG OUTPUT

    return 1;
  }

  /* Nel caso si verificasse un errore */
  else {
    fprintf(stdout, "[Error] Unknown packet received %d!\n", id);   // DEBUG OUTPUT
    return -1;
  }

  return -1;
}



/* Gestione del thread del client per aggiunta del client alla lista e controllo pacchetti tramite TCP_packet (DA COMPLETARE) */
void* TCP_client_handler (void* args){
  /* TODO: creare lista utenti alla quale aggiungere l'utente appena connesso */
  tcp_args_t* tcp_args = (tcp_args_t*) args;

  int tcp_client_desc = tcp_args->client_desc;

  int msg_length = 0;
  int ret;
  char buffer_recv[1000000];	// Conterrà il PacketHeader

  int packet_length = sizeof(PacketHeader);

  /* Ricezione del pacchetto */
  while(msg_length < packet_length){
  	ret = recv(tcp_client_desc, buffer_recv + msg_length, packet_length - msg_length, 0);
  	if (ret==-1 && errno == EINTR) continue;
  	ERROR_HELPER(ret, "[TCP Client Thread] Failed to receive packet");
  	msg_length += ret;
  }

  PacketHeader* header = (PacketHeader*) buffer_recv;
  int size = header->size - packet_length;
  msg_length=0;

  while(msg_length < size){
	ret = recv(tcp_client_desc, buffer_recv + msg_length + packet_length, size - msg_length, 0);
	if (ret==-1 && errno == EINTR) continue;
	ERROR_HELPER(ret, "[TCP Client Thread] Failed to receive packet");
	msg_length += ret;
  }

  ret = TCP_packet(tcp_client_desc, tcp_args->client_desc, buffer_recv, tcp_args->surface_elevation, tcp_args->elevation_texture);

  if (ret == 1) fprintf(stdout, "[TCP Client Thread] Success");
  else fprintf(stdout, "[TCP Client Thread] Failed");

  /* Chiusura thread */
  pthread_exit(0);
}



/* Handler della connessione TCP con il client (nel thread) */
void* TCP_handler(void* args){
	int ret;
    tcp_args_t* tcp_args = (tcp_args_t*) args;	// Cast degli args da void a tcp_args_t

    int sockaddr_len = sizeof(struct sockaddr_in);
    struct sockaddr_in client_addr = {0};
    int tcp_client_desc = accept(tcp_socket_desc, (struct sockaddr*)&client_addr, (socklen_t*) &sockaddr_len);
	ERROR_HELPER(tcp_client_desc, "[Error] Failed to accept client TCP connection");

	pthread_t client_thread;

	/* args del thread client */
	tcp_args_t tcp_args_aux;
	tcp_args_aux.client_desc = tcp_client_desc;
	tcp_args_aux.elevation_texture = tcp_args->elevation_texture;
	tcp_args_aux.surface_elevation = tcp_args->surface_elevation;

	/* Thread create - TODO: TCP_client_handler da implementare */
	ret = pthread_create(&client_thread, NULL, TCP_client_handler, &tcp_args_aux);
	PTHREAD_ERROR_HELPER(ret, "[Client] Failed to create TCP client thread");

	/* Thread detach (NON JOIN) */
	ret = pthread_detach(client_thread);

	/* Chiusura thread */
    pthread_exit(0);
}

/* Main */
int main(int argc, char **argv) {
  int ret;	// Variabile utilizzata per i vari controlli sui return delle connessioni, ...

  if (argc<3) {
    printf("usage: %s <elevation_image> <texture_image>\n", argv[1]);
    exit(-1);
  }
  char* elevation_filename=argv[1];
  char* texture_filename=argv[2];
  char* vehicle_texture_filename="./images/arrow-right.ppm";
  printf("loading elevation image from %s ... ", elevation_filename);

  // load the images
  Image* surface_elevation = Image_load(elevation_filename);
  if (surface_elevation) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }


  printf("loading texture image from %s ... ", texture_filename);
  Image* surface_texture = Image_load(texture_filename);
  if (surface_texture) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }

  printf("loading vehicle texture (default) from %s ... ", vehicle_texture_filename);
  Image* vehicle_texture = Image_load(vehicle_texture_filename);
  if (vehicle_texture) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }

  /* Inizializza server TCP */
  tcp_socket_desc = socket(AF_INET , SOCK_STREAM , 0);
  ERROR_HELPER(tcp_socket_desc, "[TCP] Failed to create TCP socket");

  struct sockaddr_in tcp_server_addr = {0};
  int sockaddr_len = sizeof(struct sockaddr_in);
  tcp_server_addr.sin_addr.s_addr = INADDR_ANY;
  tcp_server_addr.sin_family      = AF_INET;
  tcp_server_addr.sin_port        = htons(SERVER_PORT);

  int reuseaddr_opt_tcp = 1;
  ret = setsockopt(tcp_socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt_tcp, sizeof(reuseaddr_opt_tcp));
  ERROR_HELPER(ret, "[TCP] Failed setsockopt on TCP server socket");

  ret = bind(tcp_socket_desc, (struct sockaddr*) &tcp_server_addr, sockaddr_len);
  ERROR_HELPER(ret, "[TCP] Failed bind address on TCP server socket");

  ret = listen(tcp_socket_desc, 16);
  ERROR_HELPER(ret, "[TCP] Failed listen on TCP server socket");

  fprintf(stdout, "[TCP] Server Started!");  // DEBUG OUTPUT
  /* Server TCP inizializzato */

  /* Inizializza server UDP receiver */
  udp_socket_recv = socket(AF_INET, SOCK_DGRAM, 0);
  ERROR_HELPER(udp_socket_recv, "[UDP Receiver] Failed to create UDP receiver socket");

  struct sockaddr_in udp_recv_server_addr = {0};
  udp_recv_server_addr.sin_addr.s_addr = INADDR_ANY;
  udp_recv_server_addr.sin_family      = AF_INET;
  udp_recv_server_addr.sin_port        = htons(SERVER_PORT);

  int reuseaddr_opt_udp_recv = 1;
  ret = setsockopt(udp_socket_recv, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt_udp_recv, sizeof(reuseaddr_opt_udp_recv));
  ERROR_HELPER(ret, "[UDP Receiver] Failed setsockopt on UDP server receiver socket");

  ret = bind(udp_socket_recv, (struct sockaddr*) &udp_recv_server_addr, sizeof(udp_recv_server_addr));
  ERROR_HELPER(ret, "[UDP Receiver] Failed bind address on UDP server receiver socket");

  fprintf(stdout, "[UDP Receiver] Server receiver Started!");  // DEBUG OUTPUT
  /* Server UDP receiver inizializzato */

  /* Inizializza server UDP sender */
  udp_socket_send = socket(AF_INET, SOCK_DGRAM, 0);
  ERROR_HELPER(udp_socket_send, "[UDP Sender] Failed to create UDP sender socket");

  struct sockaddr_in udp_send_server_addr = {0};
  udp_send_server_addr.sin_addr.s_addr = INADDR_ANY;
  udp_send_server_addr.sin_family      = AF_INET;
  udp_send_server_addr.sin_port        = htons(SERVER_PORT);

  int reuseaddr_opt_udp_send = 1;
  ret = setsockopt(udp_socket_send, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt_udp_send, sizeof(reuseaddr_opt_udp_send));
  ERROR_HELPER(ret, "[UDP Sender] Failed setsockopt on UDP server sender socket");

  ret = bind(udp_socket_send, (struct sockaddr*) &udp_send_server_addr, sizeof(udp_send_server_addr));
  ERROR_HELPER(ret, "[UDP Sender] Failed bind address on UDP server sender socket");

  fprintf(stdout, "[UDP Sender] Server sender Started!");  // DEBUG OUTPUT
  /* Server UDP sender inizializzato */

  /* Inizializzazione del mondo */
  World_init(&world, surface_elevation, surface_texture,  0.5, 0.5, 0.5);

  /* ------------------- */
  /* Gestione dei thread */
  /* ------------------- */

  // TODO: Gestione thread (DA COMPLETARE): aggiungere thread gestione UDP sender/receiver e del World da caricare con relative funzioni
  pthread_t TCP_connection; /*, UDP_sender_thread, UDP_receiver_thread, world_thread; */

  tcp_args_t tcp_args;
  tcp_args.elevation_texture = surface_texture;
  tcp_args.surface_elevation = surface_elevation;

  /* Create dei thread */
  ret = pthread_create(&TCP_connection, NULL, TCP_handler, &tcp_args);
  PTHREAD_ERROR_HELPER(ret, "pthread_create on thread tcp failed");
  
  /* Join dei thread */
  ret=pthread_join(TCP_connection,NULL);
  ERROR_HELPER(ret,"Failed to join TCP server connection thread");

  /* Cleanup generale per liberare la memoria utilizzata */
  Image_free(surface_texture);
  Image_free(surface_elevation);
  World_destroy(&world);
  return 0;     
}