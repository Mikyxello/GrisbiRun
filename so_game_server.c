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

#define BUFFER_SIZE     1000000       // Dimensione massima dei buffer utilizzati (grande per evitare di mandare messaggi a pezzi)
#define TCP_PORT        25252         // Porta per la connessione TCP
#define UDP_PORT        8888          // Porta per la connessione UDP
#define SERVER_ADDRESS  "127.0.0.1"   // Indirizzo del server (localhost)

// Struttura per args dei threads in TCP
typedef struct {
	int client_desc;
	struct sockaddr_in client_addr;
	Image* elevation_texture;
	Image* surface_elevation;
} tcp_args_t;

// Definizione Socket e variabili 'globali'
int tcp_socket, udp_socket;
World world;
UserHead* users; 	// Inizio lista utenti per ricerca

/* Gestione pacchetti TCP ricevuti */
int TCP_packet (int tcp_socket, int id, char* buffer, Image* surface_elevation, Image* elevation_texture, int len) {
  PacketHeader* header = (PacketHeader*) buffer;  // Pacchetto per controllo del tipo di richiesta

  // Se la richiesta dal client a questo server è per l'ID (invia l'id assegnato al client che lo richiede)
  if (header->type == GetId) {

    printf("[TCP] ID requested from %d...\n", id);

    // Crea un IdPacket utilizzato per mandare l'id assegnato dal server al client (specifica struct per ID)
    IdPacket* id_to_send = (IdPacket*) malloc(sizeof(IdPacket));

    PacketHeader header_send;
    header_send.type = GetId;
    
    id_to_send->header = header_send;
    id_to_send->id = id;  // Gli assegno l'id passato da funzione TCPHandler

    char buffer_send[BUFFER_SIZE];
    int pckt_length = Packet_serialize(buffer_send, &(id_to_send->header)); // Ritorna il numero di bytes scritti

    // Invio del messaggio tramite socket
    int bytes_sent = 0;
    int ret;
    while(bytes_sent < pckt_length){
      ret = send(tcp_socket, buffer_send + bytes_sent, pckt_length - bytes_sent,0);
      if (ret==-1 && errno==EINTR) continue;
      ERROR_HELPER(ret, "[ERROR] Error assigning ID!!!");
      if (ret==0) break;
      bytes_sent += ret;
    }

    Packet_free(&(id_to_send->header));   // Libera la memoria del pacchetto non più utilizzato
    free(id_to_send);

    printf("[TCP] ID sent to %d (size = %d)...\n", id, bytes_sent);  // DEBUG OUTPUT

    return 1;
  }

  // Se la richiesta dal client a questo server è per la texture della mappa
  else if (header->type == GetTexture) {

    printf("[TCP] Texture requested from %d...\n", id);

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

    char buffer_send[BUFFER_SIZE];
    int pckt_length = Packet_serialize(buffer_send, &(texture_to_send->header)); // Ritorna il numero di bytes scritti

    // Invio del messaggio tramite socket
    int bytes_sent = 0;
    int ret;
    while(bytes_sent < pckt_length){
      ret = send(tcp_socket, buffer_send + bytes_sent, pckt_length - bytes_sent,0);
      if (ret==-1 && errno==EINTR) continue;
      ERROR_HELPER(ret, "[ERROR] Error requesting texture!!!");
      if (ret==0) break;
      bytes_sent += ret;
    }

    Packet_free(&(texture_to_send->header));   // Libera la memoria del pacchetto non più utilizzato
    free(texture_to_send);

    printf("[TCP] Texture sent to %d (size = %d)...\n", id, bytes_sent);   // DEBUG OUTPUT

    return 1;
  }

  // Se la richiesta dal client a questo server è per la elevation surface
  else if (header->type == GetElevation) {

    printf("[TCP] Elevation requested from %d...\n", id);

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

    char buffer_send[BUFFER_SIZE];
    int pckt_length = Packet_serialize(buffer_send, &(elevation_to_send->header)); // Ritorna il numero di bytes scritti

    // Invio del messaggio tramite socket
    int bytes_sent = 0;
    int ret;
    while(bytes_sent < pckt_length){
      ret = send(tcp_socket, buffer_send + bytes_sent, pckt_length - bytes_sent,0);
      if (ret==-1 && errno==EINTR) continue;
      ERROR_HELPER(ret, "[ERROR] Error requesting elevation texture!!!");
      if (ret==0) break;
      bytes_sent += ret;
    }

    Packet_free(&(elevation_to_send->header));   // Libera la memoria del pacchetto non più utilizzato
    free(elevation_to_send);

    printf("[TCP] Elevation sent to %d (size = %d)...\n", id, bytes_sent);   // DEBUG OUTPUT

    return 1;
  }

  // Se il server riceve una texture dal client
  else if (header->type == PostTexture) {
    //int ret;
 
    if (len < header->size) {
      //printf("[TCP] Actual packet (size = %d)...\n", len);
      return -1;
    }

    // Deserializzazione del pacchetto
    PacketHeader* received_header = Packet_deserialize(buffer, header->size);
    ImagePacket* received_texture = (ImagePacket*) received_header;

    printf("[TCP] Vehicle sent from %d...\n", id);
    
    //  Aggiunta veicolo nuovo al mondo
    Vehicle* new_vehicle = malloc(sizeof(Vehicle));
    Vehicle_init(new_vehicle, &world, id, received_texture->image);
    World_addVehicle(&world, new_vehicle);

   	Packet_free(received_header);	// Libera la memoria del pacchetto non più utilizzato
    free(received_texture);

    printf("[TCP] Vehicle loaded from %d...\n", id);   // DEBUG OUTPUT

    return 1;
  }

  // Nel caso il pacchetto sia sconosciuto
  else {
    printf("[ERROR] Unknown packet received from %d!!!\n", id);   // DEBUG OUTPUT

  }

  return -1;  // Return in caso di errore
}



/* Gestione del thread del client per aggiunta del client alla lista e controllo pacchetti tramite TCP_packet (DA COMPLETARE) */
void* TCP_client_handler (void* args){
  tcp_args_t* tcp_args = (tcp_args_t*) args;

  printf("[TCP] Handling client with client descriptor (%d)...\n", tcp_args->client_desc);

  int tcp_client_desc = tcp_args->client_desc;
  int msg_length = 0;
  int ret;
  char buffer_recv[BUFFER_SIZE];	// Conterrà il PacketHeader

  printf("[TCP] Creating user with id (%d)...\n", tcp_client_desc);

  // Inserimento utente in lista
  User* user = (User*) malloc(sizeof(User));
  user->id = tcp_client_desc;
  user->user_addr_tcp = tcp_args->client_addr;
  user->x = 0;
  user->y = 0;
  user->theta = 0;
  user->vehicle = NULL;
  User_insert_last(users, user);

  printf("[TCP] User (%d) inserted...\n", tcp_client_desc);

  // Ricezione del pacchetto
  int packet_length = BUFFER_SIZE;
  while(1) {
    while( (ret = recv(tcp_client_desc, buffer_recv + msg_length, packet_length - msg_length, 0)) < 0){
    	if (ret==-1 && errno == EINTR) continue;
    	ERROR_HELPER(ret, "[ERROR] Failed to receive packet!!!");
    }
    if (ret == 0) {
      printf("[TCP] Connection closed with (%d)...\n", user->id);
      break;
    }

    msg_length += ret;

    printf("[TCP] Received packet (total size = %d)...\n", ((PacketHeader*) buffer_recv)->size);

    // Gestione del pacchetto ricevuto tramite l'handler dei pacchetti
    ret = TCP_packet(tcp_client_desc, tcp_args->client_desc, buffer_recv, tcp_args->surface_elevation, tcp_args->elevation_texture, msg_length);

    if (ret == 1) {
      printf("[TCP] Success...\n");
      msg_length = 0;
    }
    else {
      printf("[TCP] Next packet...\n");
    }
  }

  // Chiusura socket
  ret = close(tcp_client_desc);
  if (ret) printf("[TCP] Socket closed with client (%d)...\n", tcp_client_desc);
  else printf("[ERROR] Error closing socket with client!!!\n");

  // Chiusura thread
  pthread_exit(0);
}



/* Handler della connessione TCP con il client (nel thread) */
void* TCP_handler(void* args){
  printf("[TCP] Handler started...\n");

  int ret;

  tcp_args_t* tcp_args = (tcp_args_t*) args;	// Cast degli args da void a tcp_args_t

  printf("[TCP] Accepting connection from clients...\n");

  while(1) {
    int sockaddr_len = sizeof(struct sockaddr_in);
    struct sockaddr_in client_addr;
    int tcp_client_desc = accept(tcp_socket, (struct sockaddr*)&client_addr, (socklen_t*) &sockaddr_len);   // Accetta nuova connessione dal client
    ERROR_HELPER(tcp_client_desc, "[ERROR] Failed to accept client TCP connection!!!");

    if (tcp_client_desc>=0) printf("[TCP] Connection enstablished with %d...\n", tcp_client_desc);

    pthread_t client_thread;

    // args del thread client
    tcp_args_t tcp_args_aux;
    tcp_args_aux.client_desc = tcp_client_desc;
    tcp_args_aux.elevation_texture = tcp_args->elevation_texture;
    tcp_args_aux.surface_elevation = tcp_args->surface_elevation;
    tcp_args_aux.client_addr = client_addr;

    printf("[TCP] Creating client handling thread for %d...\n", tcp_args_aux.client_desc);

    // Thread create
    ret = pthread_create(&client_thread, NULL, TCP_client_handler, &tcp_args_aux);
    PTHREAD_ERROR_HELPER(ret, "[ERROR] Failed to create TCP client thread!!!");

    // Thread join
    ret=pthread_join(client_thread, NULL);
    ERROR_HELPER(ret,"[ERROR] Failed to join TCP client handling thread!!!");

    printf("[TCP] Thread joined...\n");
  }

  // Chiusura socket e libera memoria 
  ret = close(tcp_socket);
  if (ret) printf("[TCP] Socket closed...\n");
  else printf("[ERROR] Error closing socket!!!\n");
  //free(tcp_args);

  // Chiusura thread
  pthread_exit(0);
}



/* Handler della connessione UDP con il client in modalità 'receiver' (riceve pacchetti) */
void* UDP_receiver_handler(void* args) {
  printf("[UDP RECEIVER] Handler started...\n");

  int ret;

  char buffer_recv[BUFFER_SIZE];
  struct sockaddr_in client_addr = {0};
  socklen_t addrlen = sizeof(struct sockaddr_in);
  
  ret = recvfrom(udp_socket, buffer_recv, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &addrlen);
  ERROR_HELPER(ret, "[ERROR] [UDP RECEIVER] Error receiving UDP packet!!!");

  // Raccoglie il pacchetto ricevuto
  PacketHeader* header = (PacketHeader*)buffer_recv;

  VehicleUpdatePacket* packet = (VehicleUpdatePacket*)Packet_deserialize(buffer_recv, header->size);
  User* user = User_find_id(users, packet->id);

  if(!user) {
  	printf("[ERROR] [UDP RECEIVER] Cannot find a user with this ID: %d!!!\n", packet->id);
    pthread_exit(0);
  }
  
  // Aggiorna la posizione dell'utente
  Vehicle_setForcesUpdate(user->vehicle, packet->translational_force, packet->rotational_force);
  //Vehicle_setXYTheta(user->vehicle, packet->x, packet->y, packet->theta);
  
  // Update del mondo
  World_update(&world);

  Packet_free(&packet->header);	// Liberazione memoria utilizzata
  pthread_exit(0);
}


/* Handler della connessione UDP con il client in modalità 'sender' (invia pacchetti) */
void* UDP_sender_handler(void* args) {
  printf("[UDP SENDER] Handler started...\n");

  char buffer_send[BUFFER_SIZE];

  // Creazione del pacchetto da inviare
  PacketHeader header;
  header.type = WorldUpdate;

  WorldUpdatePacket* world_update = (WorldUpdatePacket*) malloc(sizeof(WorldUpdatePacket));
  world_update->header = header;

  int n_users = 0;
 
  // Conta il numero di utenti collegati
  User* user = users->first;
  while(user != NULL) {
    n_users++;
    user = user->next;
  }
  user = users->first;

  world_update->updates = (ClientUpdate*)malloc(sizeof(ClientUpdate) * n_users);

  for (int i=0; i<n_users; i++) {
    ClientUpdate* client = &(world_update->updates[i]);

    client->id = user->id;
    Vehicle_getXYTheta(user->vehicle, &(client->x), &(client->y), &(client->theta));
    // Vehicle_getForcesUpdate(user->vehicle, client->translational_force, client->rotational_force);

    user = user->next;
  }

  // Serializzazione del pacchetto per update nel buffer
  int size = Packet_serialize(buffer_send, &world_update->header);

  user = users->first;

  // Invia i pacchetti a tutti gli utenti connessi
  while (user != NULL) {
    int ret = sendto(udp_socket, buffer_send, size, 0, (struct sockaddr*)&user->user_addr_udp, (socklen_t)sizeof(user->user_addr_udp));
    ERROR_HELPER(ret, "[ERROR] [UDP SENDER] Error sending update to user!!!");
    user = user->next;
  }

  // Liberazione memoria
  free(world_update);
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
  tcp_socket = socket(AF_INET , SOCK_STREAM , 0);
  ERROR_HELPER(tcp_socket, "[TCP] Failed to create TCP socket");
  if(tcp_socket>=0) printf("[TCP] Socket opened %d...\n", tcp_socket);

  struct sockaddr_in tcp_server_addr = {0};
  int sockaddr_len = sizeof(struct sockaddr_in);
  tcp_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  tcp_server_addr.sin_family      = AF_INET;
  tcp_server_addr.sin_port        = htons(TCP_PORT);

  int reuseaddr_opt_tcp = 1;
  ret = setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt_tcp, sizeof(reuseaddr_opt_tcp));
  ERROR_HELPER(ret, "[ERROR] [MAIN] [TCP] Failed setsockopt on TCP server socket!!!");
  if(ret>=0) printf("[MAIN] [TCP] Setsockopt worked...\n");

  ret = bind(tcp_socket, (struct sockaddr*) &tcp_server_addr, sockaddr_len);
  ERROR_HELPER(ret, "[ERROR] [MAIN] [TCP] Failed bind address on TCP server socket!!!");
  if(ret>=0) printf("[MAIN] [TCP] Bind worked...\n");

  ret = listen(tcp_socket, 0);
  ERROR_HELPER(ret, "[ERROR] [MAIN] [TCP] Failed listen on TCP server socket!!!");
  if (ret>=0) printf("[MAIN] [TCP] Server listening on port %d...\n", TCP_PORT);

  fprintf(stdout, "[MAIN] [TCP] Server TCP started...\n");  // DEBUG OUTPUT
  /* Server TCP inizializzato */

  /* Inizializza server UDP */
  udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
  ERROR_HELPER(udp_socket, "[ERROR] [MAIN] [UDP] Failed to create UDP socket!!!");

  struct sockaddr_in udp_server_addr = {0};
  udp_server_addr.sin_addr.s_addr = INADDR_ANY;
  udp_server_addr.sin_family      = AF_INET;
  udp_server_addr.sin_port        = htons((uint16_t) UDP_PORT);

  int reuseaddr_opt_udp = 1;
  ret = setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt_udp, sizeof(reuseaddr_opt_udp));
  ERROR_HELPER(ret, "[ERROR] [MAIN] [UDP] Failed setsockopt on UDP server socket!!!");

  ret = bind(udp_socket, (struct sockaddr*) &udp_server_addr, sizeof(udp_server_addr));
  ERROR_HELPER(ret, "[ERROR] [MAIN] [UDP] Failed bind address on UDP server socket!!!");

  printf("[MAIN] [UDP] Server UDP started...\n");  // DEBUG OUTPUT
  /* Server UDP inizializzato */

  /* Inizializzazione utenti */
  users = (UserHead*) malloc(sizeof(UserHead));
  Users_init(users);
  printf("[MAIN] User list initialized...\n");

  /* Inizializzazione del mondo */
  World_init(&world, surface_elevation, surface_texture,  0.5, 0.5, 0.5);
  printf("[MAIN] World initialized...\n");

  /* ------------------- */
  /* Gestione dei thread */
  /* ------------------- */
  pthread_t TCP_connection, UDP_sender_thread, UDP_receiver_thread;

  /* Args per il thread TCP */
  tcp_args_t tcp_args;
  tcp_args.elevation_texture = surface_texture;
  tcp_args.surface_elevation = surface_elevation;

  printf("[MAIN] Initializating threads...\n");

  /* Create dei thread */
  ret = pthread_create(&TCP_connection, NULL, TCP_handler, &tcp_args);
  PTHREAD_ERROR_HELPER(ret, "[ERROR] [MAIN] Failed to create TCP connection thread!!!");

  ret = pthread_create(&UDP_sender_thread, NULL, UDP_sender_handler, NULL);
  PTHREAD_ERROR_HELPER(ret, "[ERROR] [MAIN] Failed to create UDP sender thread!!!");

  ret = pthread_create(&UDP_receiver_thread, NULL, UDP_receiver_handler, NULL); 
  PTHREAD_ERROR_HELPER(ret, "[ERROR] [MAIN] Failed to create UDP receiver thread!!!");

  printf("[MAIN] Threads created...\n");
  printf("[MAIN] Joining threads...\n");

  /* Join dei thread */
  ret=pthread_join(TCP_connection,NULL);
  ERROR_HELPER(ret,"[ERROR] [MAIN] Failed to join TCP server connection thread!!!");

  ret=pthread_join(UDP_sender_thread,NULL);
  ERROR_HELPER(ret,"[ERROR] [MAIN] Failed to join UDP server sender thread!!!");

  ret=pthread_join(UDP_receiver_thread,NULL);
  ERROR_HELPER(ret,"[ERROR] [MAIN] Failed to join UDP server receiver thread!!!");

  printf("[MAIN] Threads joined...\n");
  printf("[MAIN] Closing...\n");

  /* Cleanup generale per liberare la memoria utilizzata */
  Image_free(surface_texture);
  Image_free(surface_elevation);
  World_destroy(&world);
  return 0;     
}