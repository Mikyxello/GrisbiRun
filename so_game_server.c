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
#define TIME_TO_SLEEP   10000         // Imposta timeout di aggiornamento

// Struttura per args dei threads in TCP
typedef struct {
  int client_desc;
  struct sockaddr_in client_addr;
  Image* elevation_texture;
  Image* surface_elevation;
} tcp_args_t;

// Definizione Socket e variabili 'globali'
World world;
UserHead* users;
int running;
int tcp_socket, udp_socket;
pthread_t TCP_connection, UDP_sender_thread, UDP_receiver_thread;
Image* surface_elevation;
Image* surface_texture;

/* Funzione per il cleanup generico della memoria */
void cleanMemory(void) {
  int ret;

  running = 0;

  /*
  ret = pthread_kill(TCP_connection, SIGTERM);
  ERROR_HELPER(ret, "[ERROR] Cannot terminate the TCP connection thread!!!");

  ret = pthread_kill(UDP_sender_thread, SIGTERM);
  ERROR_HELPER(ret, "[ERROR] Cannot terminate the UDP sender thread!!!");

  ret = pthread_kill(UDP_receiver_thread, SIGTERM);
  ERROR_HELPER(ret, "[ERROR] Cannot terminate the UDP receiver thread!!!");
  */

  ret = close(tcp_socket);
  ERROR_HELPER(ret, "[ERROR] Cannot close TCP socket!!!");

  ret = close(udp_socket);
  ERROR_HELPER(ret, "[ERROR] Cannot close UDP socket!!!");

  World_destroy(&world);
  Image_free(surface_elevation);
  Image_free(surface_texture);

  printf("[CLEANUP] Memory cleaned...\n");
  return;
}

/* Funzione per la gestione dei segnali */
void signalHandler(int signal){
  switch (signal) {
	case SIGHUP:
	  printf("[CLOSING] Server is closing...\n");	
	  cleanMemory();
	  exit(1);
	case SIGINT:
	  printf("[CLOSING] Server is closing...\n");
	  cleanMemory();
	  exit(1);
	default:
	  printf("[ERROR] Uncaught signal: %d...\n", signal);
	  return;
  }
}

/* Gestione pacchetti TCP ricevuti */
int TCP_packet (int client_tcp_socket, int id, char* buffer, Image* surface_elevation, Image* elevation_texture, int len, User* user) {
  PacketHeader* header = (PacketHeader*) buffer;  // Pacchetto per controllo del tipo di richiesta

  // Se la richiesta dal client a questo server è per l'ID (invia l'id assegnato al client che lo richiede)
  if (header->type == GetId) {

    printf("[TCP ID] ID requested from (%d)...\n", id);

    // Crea un IdPacket utilizzato per mandare l'id assegnato dal server al client (specifica struct per ID)
    IdPacket* id_to_send = (IdPacket*) malloc(sizeof(IdPacket));

    PacketHeader header_send;
    header_send.type = GetId;
    
    id_to_send->header = header_send;
    id_to_send->id = id;  // Gli assegno l'id passato da funzione TCPHandler

    char buffer_send[BUFFER_SIZE];
    int pckt_length = Packet_serialize(buffer_send, &(id_to_send->header));

    // Invio del messaggio tramite socket
    int bytes_sent = 0;
    int ret;
    while(bytes_sent < pckt_length){
      ret = send(client_tcp_socket, buffer_send + bytes_sent, pckt_length - bytes_sent,0);
      if (ret==-1 && errno==EINTR) continue;
      ERROR_HELPER(ret, "[ERROR] Error assigning ID!!!");
      if (ret==0) break;
      bytes_sent += ret;
    }

    printf("[TCP ID] ID sent to (%d)...\n", id);

    return 1;
  }

  // Se la richiesta dal client a questo server è per la texture della mappa
  else if (header->type == GetTexture) {

    printf("[TCP MAP TEXTURE] Texture requested from (%d)...\n", id);

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
    int pckt_length = Packet_serialize(buffer_send, &(texture_to_send->header));

    // Invio del pacchetto serializzato
    int bytes_sent = 0;
    int ret;
    while(bytes_sent < pckt_length){
      ret = send(client_tcp_socket, buffer_send + bytes_sent, pckt_length - bytes_sent,0);
      if (ret==-1 && errno==EINTR) continue;
      ERROR_HELPER(ret, "[ERROR] Error requesting texture!!!");
      if (ret==0) break;
      bytes_sent += ret;
    }

    printf("[TCP MAP TEXTURE] Texture sent to (%d)...\n", id);   // DEBUG OUTPUT

    return 1;
  }

  // Se la richiesta dal client a questo server è per la elevation surface
  else if (header->type == GetElevation) {

    printf("[TCP MAP ELEVATION] Elevation requested from (%d)...\n", id);

    // Converto il pacchetto ricevuto in un ImagePacket per estrarne la elevation richiesta
    ImagePacket* elevation_request = (ImagePacket*) buffer;
    int id_request = elevation_request->id;
    
    // Preparo header per la risposta
    PacketHeader header_send;
    header_send.type = PostElevation;
    
    // Preparazione del pacchetto per inviare la elevation al client
    ImagePacket* elevation_to_send = (ImagePacket*) malloc(sizeof(ImagePacket));
    elevation_to_send->header = header_send;
    elevation_to_send->id = id_request;
    elevation_to_send->image = surface_elevation;

    char buffer_send[BUFFER_SIZE];
    int pckt_length = Packet_serialize(buffer_send, &(elevation_to_send->header));

    // Invio del pacchetto serializzato
    int bytes_sent = 0;
    int ret;
    while(bytes_sent < pckt_length){
      ret = send(client_tcp_socket, buffer_send + bytes_sent, pckt_length - bytes_sent,0);
      if (ret==-1 && errno==EINTR) continue;
      ERROR_HELPER(ret, "[ERROR] Error requesting elevation texture!!!");
      if (ret==0) break;
      bytes_sent += ret;
    }

    printf("[TCP MAP ELEVATION] Elevation sent to (%d)...\n", id);

    return 1;
  }

  // Se il server riceve una texture dal client
  else if (header->type == PostTexture) {
    int ret;
 
 	// Pacchetto non ricevuto completo, aspetta le parti successive
    if (len < header->size) return -1;

    // Deserializzazione del pacchetto
    PacketHeader* received_header = Packet_deserialize(buffer, header->size);
    ImagePacket* received_texture = (ImagePacket*) received_header;

    printf("[TCP VEHICLE TEXTURE] Vehicle sent from (%d)...\n", id);
    
    // Aggiunta veicolo nuovo al mondo
    Vehicle* new_vehicle = malloc(sizeof(Vehicle));
    Vehicle_init(new_vehicle, &world, id, received_texture->image);
    World_addVehicle(&world, new_vehicle);

    // Rimanda la texture al client come conferma
    PacketHeader header_aux;
    header_aux.type = PostTexture;

    ImagePacket* texture_for_client = (ImagePacket*)malloc(sizeof(ImagePacket));
    texture_for_client->image = received_texture->image;
    texture_for_client->header = header_aux;

    // Serializza la texture da mandare
    int buffer_size = Packet_serialize(buffer, &texture_for_client->header);
    
    // Invia la texture
    while ( (ret = send(client_tcp_socket, buffer, buffer_size, 0)) < 0) {
      if (errno == EINTR) continue;
      ERROR_HELPER(ret, "[ERROR] Cannot write to socket!!!");
    }

    printf("[TCP VEHICLE TEXTURE] Vehicle texture sent back to (%d)...\n", id);

    // Inserimento utente in lista
    User_insert_last(users, user);

    printf("[TCP USER CONNECTED] User (%d) inserted...\n", user->id);

    // Invia agli altri users la connessione del nuovo utente
    // Pacchetto della nuova connessione con la texture dell'utente nuovo
    PacketHeader header_connect;
    header_connect.type = UserConnected;

    ImagePacket* user_connected = (ImagePacket*) malloc(sizeof(ImagePacket));
    user_connected->header = header_connect;
   	user_connected->id = user->id;
   	user_connected->image = received_texture->image;
    
    User* user_aux = users->first;

    int msg_length = 0;
    char buffer_connection[BUFFER_SIZE];
    int packet_connection = Packet_serialize(buffer_connection, &(user_connected->header)); // Ritorna il numero di bytes scritti

    // Invio pacchetto della connessione del nuovo utente a tutti gli utenti già connessi
    while (user_aux != NULL) {
    	if (user_aux->id != user->id) {
		    msg_length = 0;
		    while(msg_length < packet_connection){
		      ret = send(user_aux->id, buffer_connection + msg_length, packet_connection - msg_length,0);
		      if (ret==-1 && errno==EINTR) continue;
		      ERROR_HELPER(ret, "[ERROR] Error sending new user to other users!!!");
		      if (ret==0) break;
		      msg_length += ret;
		    }
	    }

	    user_aux = user_aux->next;
  	}

  	// Invio di tutti gli utenti già connessi al nuovo utente appena connesso
  	user_aux = users->first;

  	while (user_aux != NULL) {
    	if (user_aux->id != user->id) {
    		// Invia al nuovo utente la connessione degli utenti già online
    		char buffer_connection_new[BUFFER_SIZE];
		    
		    PacketHeader header_new;
		    header_new.type = UserConnected;

		    ImagePacket* user_for_new = (ImagePacket*) malloc(sizeof(ImagePacket));
		    user_for_new->header = header_new;
		   	user_for_new->id = user_aux->id;
		   	user_for_new->image = World_getVehicle(&world, user_aux->id)->texture;

		    packet_connection = Packet_serialize(buffer_connection_new, &(user_for_new->header));

		    // Invio del pacchetto serializzato
		    msg_length = 0;
		    while(msg_length < packet_connection){
		      ret = send(user->id, buffer_connection_new + msg_length, packet_connection - msg_length,0);
		      if (ret==-1 && errno==EINTR) continue;
		      ERROR_HELPER(ret, "[ERROR] Error sending online users to the new user!!!");
		      if (ret==0) break;
		      msg_length += ret;
		    }

		    // Attende la conferma del client
			while( (ret = recv(user->id, buffer_connection_new, 12, 0)) < 0){
				if (ret==-1 && errno == EINTR) continue;
				ERROR_HELPER(ret, "[ERROR] Failed to receive packet!!!");
			}

	    }

	    user_aux = user_aux->next;
  	}

    return 1;
  }

  // Nel caso il pacchetto sia sconosciuto
  else {
    printf("[ERROR] Unknown packet received from %d!!!\n", id);   // DEBUG OUTPUT
  }

  return -1;  // Return in caso di errore
}

/* Gestione del thread del client per aggiunta del client alla lista e controllo pacchetti tramite TCP_packet */
void* TCP_client_handler (void* args){
  tcp_args_t* tcp_args = (tcp_args_t*) args;

  printf("[TCP CLIENT HANDLER] Handling client with client descriptor (%d)...\n", tcp_args->client_desc);

  int client_tcp_socket = tcp_args->client_desc;
  int msg_length = 0;
  int ret;
  char buffer_recv[BUFFER_SIZE];

  // Preparazione utente da inserire in lista
  User* user = (User*) malloc(sizeof(User));
  user->id = client_tcp_socket;
  user->user_addr_tcp = tcp_args->client_addr;
  user->x = 0;
  user->y = 0;
  user->theta = 0;
  user->translational_force = 0;
  user->rotational_force = 0;

  // Ricezione del pacchetto
  int packet_length = BUFFER_SIZE;
  while(running) {
    while( (ret = recv(client_tcp_socket, buffer_recv + msg_length, packet_length - msg_length, 0)) < 0){
    	if (ret==-1 && errno == EINTR) continue;
    	ERROR_HELPER(ret, "[ERROR] Failed to receive packet!!!");
    }
    // Utente disconnesso
    if (ret == 0) {
      printf("[TCP USER DISCONNECTED] Connection closed with (%d)...\n", user->id);
      User_detach(users, user->id);
      Vehicle* deleted = World_getVehicle(&world, user->id);
      Vehicle* aux = World_detachVehicle(&world, deleted);
      Vehicle_destroy(aux);

      // Invia agli altri users la disconnessione dell'utente
      // Pacchetto della disconnessione con l'id dell'utente disconnesso
      PacketHeader header_aux;
      header_aux.type = UserDisconnected;

      IdPacket* user_disconnected = (IdPacket*) malloc(sizeof(IdPacket));
      user_disconnected->header = header_aux;
      user_disconnected->id = client_tcp_socket;

      User* user_aux = users->first;

      while (user_aux != NULL) {
        char buffer_disconnection[BUFFER_SIZE];
      	int packet_disconnection = Packet_serialize(buffer_disconnection, &(user_disconnected->header));

	    // Invio del pacchetto serializzato
	    msg_length = 0;
	    while(msg_length < packet_disconnection){
	      ret = send(user_aux->id, buffer_disconnection + msg_length, packet_disconnection - msg_length,0);
	      if (ret==-1 && errno==EINTR) continue;
	      ERROR_HELPER(ret, "[ERROR] Error sending user disconnection to other clients!!!");
	      if (ret==0) break;
	      msg_length += ret;
	    }

        user_aux = user_aux->next;
      }

      break;
    }

    msg_length += ret;

    // Gestione del pacchetto ricevuto tramite l'handler dei pacchetti
    ret = TCP_packet(client_tcp_socket, tcp_args->client_desc, buffer_recv, tcp_args->surface_elevation, tcp_args->elevation_texture, msg_length, user);

    if (ret == 1) {
      msg_length = 0;
      continue;
    }
    else continue;
  }

  // Chiusura thread
  pthread_exit(0);
}

/* Handler della connessione TCP con il client (nel thread) */
void* TCP_handler(void* args){
  int ret;
  int tcp_client_desc;

  tcp_args_t* tcp_args = (tcp_args_t*) args;

  printf("[TCP HANDLER] Accepting connection from clients...\n");

  int sockaddr_len = sizeof(struct sockaddr_in);
  struct sockaddr_in client_addr;

  while( (tcp_client_desc = accept(tcp_socket, (struct sockaddr*)&client_addr, (socklen_t*) &sockaddr_len)) > 0) {
    printf("[TCP NEW CONNECTION] Connection enstablished with (%d)...\n", tcp_client_desc);

    pthread_t client_thread;

    // args del thread client
    tcp_args_t tcp_args_aux;
    tcp_args_aux.client_desc = tcp_client_desc;
    tcp_args_aux.elevation_texture = tcp_args->elevation_texture;
    tcp_args_aux.surface_elevation = tcp_args->surface_elevation;
    tcp_args_aux.client_addr = client_addr;

    // Thread create
    ret = pthread_create(&client_thread, NULL, TCP_client_handler, &tcp_args_aux);
    PTHREAD_ERROR_HELPER(ret, "[ERROR] Failed to create TCP client handling thread!!!");
  }
  ERROR_HELPER(tcp_client_desc, "[ERROR] Failed to accept client TCP connection!!!");

  printf("[TCP HANDLER] Stopped accepting connection...\n");

  // Chiusura thread
  pthread_exit(0);
}

/* Handler della connessione UDP con il client in modalità 'receiver' (riceve pacchetti) */
void* UDP_receiver_handler(void* args) {
  int ret;
  char buffer_recv[BUFFER_SIZE];
  
  struct sockaddr_in client_addr = {0};
  socklen_t addrlen = sizeof(struct sockaddr_in);
  
  printf("[UDP RECEIVER] Ready to receive updates...\n");
  while (1) {
    if((ret = recvfrom(udp_socket, buffer_recv, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &addrlen)) > 0) {}
    ERROR_HELPER(ret, "[ERROR] Error receiving UDP packet!!!");

    // Raccoglie il pacchetto ricevuto e
    PacketHeader* header = (PacketHeader*) buffer_recv;

    VehicleUpdatePacket* packet = (VehicleUpdatePacket*)Packet_deserialize(buffer_recv, header->size);
    User* user = User_find_id(users, packet->id);
    user->user_addr_udp = client_addr;

    if(!user) {
      printf("[ERROR] Cannot find a user with ID %d!!!\n", packet->id);
      pthread_exit(0);
    }
    
    // Aggiorna la posizione dell'utente nel mondo locale
    Vehicle* vehicle_aux = World_getVehicle(&world, user->id);

    vehicle_aux->translational_force_update = packet->translational_force;
    vehicle_aux->rotational_force_update = packet->rotational_force;

    // Update del mondo
    World_update(&world);
  }

  printf("[UDP RECEIVER] Stop receiving updates...\n");

  pthread_exit(0);
}

/* Handler della connessione UDP con il client in modalità 'sender' (invia pacchetti) */
void* UDP_sender_handler(void* args) {
  char buffer_send[BUFFER_SIZE];

  printf("[UDP SENDER] Ready to send updates...\n");
  while(running) {
    int n_users = users->size;

    // Controllo che ci siano utenti connessi
    if (n_users > 0) {
      // Creazione del pacchetto da inviare
      PacketHeader header;
      header.type = WorldUpdate;

      WorldUpdatePacket* world_update = (WorldUpdatePacket*) malloc(sizeof(WorldUpdatePacket));
      world_update->header = header;
      world_update->updates = (ClientUpdate*) malloc(sizeof(ClientUpdate) * n_users);
      world_update->num_vehicles = users->size;

      User* user = users->first;

      for (int i=0; i<n_users; i++) {
      	// Creazione update dell'i-esimo utente
        ClientUpdate* client = &(world_update->updates[i]);

        Vehicle* user_vehicle = World_getVehicle(&world, user->id);
        client->id = user->id;
        client->x = user_vehicle->x;
        client->y = user_vehicle->y;
        client->theta = user_vehicle->theta;

        user = user->next;
      }

      // Serializzazione del pacchetto per update nel buffer
      int size = Packet_serialize(buffer_send, &world_update->header);

      user = users->first;

      // Invia i pacchetti a tutti gli utenti connessi
      while (user != NULL) {
        if(user->user_addr_udp.sin_addr.s_addr != 0) {
          int ret = sendto(udp_socket, buffer_send, size, 0, (struct sockaddr*) &user->user_addr_udp, (socklen_t)sizeof(user->user_addr_udp));
          ERROR_HELPER(ret, "[ERROR] Error sending update to user!!!");
        }
        user = user->next;
      }
    }

    // Timer per il prossimo update
    usleep(TIME_TO_SLEEP);
  }
  printf("[UDP SENDER] Stop sending updates...\n");

  pthread_exit(0);
}


/* Main */
int main(int argc, char **argv) {
  running = 0;

  if (argc<3) {
    printf("usage: %s <elevation_image> <texture_image>\n", argv[1]);
    exit(-1);
  }

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

  char* elevation_filename=argv[1];
  char* texture_filename=argv[2];
  //printf("loading elevation image from %s ... ", elevation_filename);

  // load the images
  surface_elevation = Image_load(elevation_filename);
  if (surface_elevation) {
    //printf("Done! \n");
  } else {
    //printf("Fail! \n");
  }

  //printf("loading texture image from %s ... ", texture_filename);
  surface_texture = Image_load(texture_filename);
  if (surface_texture) {
    //printf("Done! \n");
  } else {
    //printf("Fail! \n");
  }

  // Inizializza server TCP 
  tcp_socket = socket(AF_INET , SOCK_STREAM , 0);
  ERROR_HELPER(tcp_socket, "[TCP] Failed to create TCP socket");

  struct sockaddr_in tcp_server_addr = {0};
  int sockaddr_len = sizeof(struct sockaddr_in);
  tcp_server_addr.sin_addr.s_addr = INADDR_ANY;
  tcp_server_addr.sin_family      = AF_INET;
  tcp_server_addr.sin_port        = htons(TCP_PORT);

  int reuseaddr_opt_tcp = 1;
  ret = setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt_tcp, sizeof(reuseaddr_opt_tcp));
  ERROR_HELPER(ret, "[ERROR] Failed setsockopt on TCP server socket!!!");
  
  ret = bind(tcp_socket, (struct sockaddr*) &tcp_server_addr, sockaddr_len);
  ERROR_HELPER(ret, "[ERROR] Failed bind address on TCP server socket!!!");

  ret = listen(tcp_socket, 3);
  ERROR_HELPER(ret, "[ERROR] Failed listen on TCP server socket!!!");
  if (ret>=0) printf("[MAIN] Server listening on port %d...\n", TCP_PORT);

  // Inizializza server UDP
  udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
  ERROR_HELPER(udp_socket, "[ERROR] Failed to create UDP socket!!!");

  struct sockaddr_in udp_server_addr = {0};
  udp_server_addr.sin_addr.s_addr = INADDR_ANY;
  udp_server_addr.sin_family      = AF_INET;
  udp_server_addr.sin_port        = htons(UDP_PORT);

  int reuseaddr_opt_udp = 1;
  ret = setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt_udp, sizeof(reuseaddr_opt_udp));
  ERROR_HELPER(ret, "[ERROR] Failed setsockopt on UDP server socket!!!");

  ret = bind(udp_socket, (struct sockaddr*) &udp_server_addr, sizeof(udp_server_addr));
  ERROR_HELPER(ret, "[ERROR] Failed bind address on UDP server socket!!!");

  printf("[MAIN] Server UDP started...\n");

  // Inizializzazione lista utenti
  users = (UserHead*) malloc(sizeof(UserHead));
  Users_init(users);

  // Inizializzazione del mondo
  World_init(&world, surface_elevation, surface_texture,  0.5, 0.5, 0.5);

  /* ------------------- */
  /* Gestione dei thread */
  /* ------------------- */

  // Args per il thread TCP
  tcp_args_t tcp_args;
  tcp_args.elevation_texture = surface_texture;
  tcp_args.surface_elevation = surface_elevation;

  running = 1;

  // Create dei thread 
  ret = pthread_create(&TCP_connection, NULL, TCP_handler, &tcp_args);
  PTHREAD_ERROR_HELPER(ret, "[ERROR] Failed to create TCP connection thread!!!");

  ret = pthread_create(&UDP_sender_thread, NULL, UDP_sender_handler, NULL);
  PTHREAD_ERROR_HELPER(ret, "[ERROR] Failed to create UDP sender thread!!!");

  ret = pthread_create(&UDP_receiver_thread, NULL, UDP_receiver_handler, NULL); 
  PTHREAD_ERROR_HELPER(ret, "[ERROR] Failed to create UDP receiver thread!!!");

  // Join dei thread 
  ret = pthread_join(TCP_connection,NULL);
  ERROR_HELPER(ret,"[ERROR] Failed to join TCP server connection thread!!!");

  ret = pthread_join(UDP_sender_thread,NULL);
  ERROR_HELPER(ret,"[ERROR] Failed to join UDP server sender thread!!!");

  ret = pthread_join(UDP_receiver_thread,NULL);
  ERROR_HELPER(ret,"[ERROR] Failed to join UDP server receiver thread!!!");

  running = 0;

  // Cleanup generale per liberare la memoria utilizzata
  ret = close(tcp_socket);
  ERROR_HELPER(ret, "[ERROR] Cannot close TCP socket!!!");

  ret = close(udp_socket);
  ERROR_HELPER(ret, "[ERROR] Cannot close UDP socket!!!");

  Image_free(surface_texture);
  Image_free(surface_elevation);
  World_destroy(&world);
  return 0;     
}