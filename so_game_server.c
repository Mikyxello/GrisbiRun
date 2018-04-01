
// #include <GL/glut.h> // not needed here
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"

typedef struct handler_args_s {
    int socket_desc;
    struct sockaddr_in* client_addr;
    Image* surface_elevation;
    Image* surface_texture;
    Image* vehicle_texture;
} handler_args_t;


void* connection_handler(void* arg) { //da implementare

    handler_args_t* args = (handler_args_t*)arg;

    /* We make local copies of the fields from the handler's arguments
     * data structure only to share as much code as possible with the
     * other two versions of the server. In general this is not a good
     * coding practice: using simple indirection is better! */
    int socket_desc = args->socket_desc;
    struct sockaddr_in* client_addr = args->client_addr;

	int ret, bytes_sent, bytes_recv, connection_id;
	char image_packet_buffer[1000000];
	char id_packet_buffer[1000000];
	
	ImagePacket* image_packet = (ImagePacket*)malloc(sizeof(ImagePacket));
	IdPacket* id_packet = (IdPacket*)malloc(sizeof(IdPacket));


    // parse client IP address and port
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    uint16_t client_port = ntohs(client_addr->sin_port); // port number is an unsigned short

	// read message from client
	while ( (recv_bytes = recv(socket_desc, id_packet_buffer, 1000000, 0)) < 0 ) {
		if (errno == EINTR) continue;
		ERROR_HELPER(-1, "Cannot read from socket");
	}
	
	id_packet = (IdPacket*)Packet_deserialize(id_packet_buffer,bytes_recv);
	
	if(id_packet->id ==-1){
		registerID(id_packet->id,&connection_id); // TODO: funzione che assegna ID al Client e se lo conserva nella linkedlist, eventualmente invia l'aggiornamento anche agli altri client connessi
	} else (ERROR_HELPER(-1,"wrong packet received, waiting for a IDPacket to register");
	
	bytes_sent = Packet_serialize(id_packet_buffer, &id_packet->header);
    
	while ( (ret = send(socket_desc, id_packet_buffer, bytes_sent, 0)) < 0) { //invio al client l'id assegnato
		if (errno == EINTR) continue;
		ERROR_HELPER(-1, "Cannot write to socket");
	}
	
	
	while ( (recv_bytes = recv(socket_desc, image_packet_buffer, 1000000, 0)) < 0 ) { //ricevo mytexture dal client
		if (errno == EINTR) continue;
		ERROR_HELPER(-1, "Cannot read from socket");
	}
	
	image_packet = (ImagePacket*)Packet_deserialize(image_packet_buffer,recv_bytes); //deserializzo mytexture ricevuta
	
	// ^^^ TODO: che ce devo fà con la texture del giocatore? who knows >> qui invierò anche la texture all'id che me l'ha inviato
	
    ImagePacket* image_packet = (ImagePacket*)malloc(sizeof(ImagePacket));

    PacketHeader im_head;
    im_head.type = PostTexture;
    im_head.id = connection_id;  //faccio il pacchetto per la vehicle_texture
    
    image_packet->header = im_head;
    image_packet->image = args->vehicle_texture; //collego
    
	int image_packet_buffer_size = Packet_serialize(image_packet_buffer, &image_packet->header); //serializzo mytexture
	
  	while ( (ret = send(socket_desc, image_packet_buffer, image_packet_buffer_size, 0)) < 0) { //invio vehicle_texture al client
		if (errno == EINTR) continue;
		ERROR_HELPER(-1, "Cannot write to socket");
	}
	
    im_head.type = PostElevation;
    im_head.id = 0;			//faccio il pacchetto per la surface_elevation
    
    image_packet->header = im_head;
	image_packet->image = args->surface_elevation; //collego

	image_packet_buffer_size = Packet_serialize(image_packet_buffer, &image_packet->header); //serializzo surface_elevation
	
  	while ( (ret = send(socket_desc, image_packet_buffer, image_packet_buffer_size, 0)) < 0) { //invio surface_elevation al client
		if (errno == EINTR) continue;
		ERROR_HELPER(-1, "Cannot write to socket");
	}
	
    im_head.type = PostTexture;
    im_head.id = 0;			//faccio il pacchetto per la surface_texture
    
    image_packet->header = im_head;
	image_packet->image = args->surface_texture; //collego

	image_packet_buffer_size = Packet_serialize(image_packet_buffer, &image_packet->header); //serializzo surface_texture
	
  	while ( (ret = send(socket_desc, image_packet_buffer, image_packet_buffer_size, 0)) < 0) { //invio surface_texture al client
		if (errno == EINTR) continue;
		ERROR_HELPER(-1, "Cannot write to socket");
	}
	
    Packet_free(&image_packet->header); //libero pacchetto


	
	
	
	
	
	
	
	//TODO : creare handler udp ?
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
    free(args->client_addr); // do not forget to free this buffer!
    free(args);
    pthread_exit(NULL);
}




int main(int argc, char **argv) {
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


//INIZIALIZZAZIONE SERVER MULTITHREAD

	int ret;

    int socket_desc, client_desc;

    // some fields are required to be filled with 0
    struct sockaddr_in server_addr = {0};

    int sockaddr_len = sizeof(struct sockaddr_in); // we will reuse it for accept()

    // initialize socket for listening
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    ERROR_HELPER(socket_desc, "Could not create socket");

    server_addr.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(SERVER_PORT); // don't forget about network byte order!

    /* We enable SO_REUSEADDR to quickly restart our server after a crash:
     * for more details, read about the TIME_WAIT state in the TCP protocol */
    int reuseaddr_opt = 1;
    ret = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
    ERROR_HELPER(ret, "Cannot set SO_REUSEADDR option");

    // bind address to socket
    ret = bind(socket_desc, (struct sockaddr*) &server_addr, sockaddr_len);
    ERROR_HELPER(ret, "Cannot bind address to socket");

    // start listening
    ret = listen(socket_desc, MAX_CONN_QUEUE);
    ERROR_HELPER(ret, "Cannot listen on socket");

    // we allocate client_addr dynamically and initialize it to zero
    struct sockaddr_in* client_addr = calloc(1, sizeof(struct sockaddr_in));

    // loop to manage incoming connections spawning handler threads
    while (1) {
        // accept incoming connection
        client_desc = accept(socket_desc, (struct sockaddr*) client_addr, (socklen_t*) &sockaddr_len);
        if (client_desc == -1 && errno == EINTR) continue; // check for interruption by signals
        ERROR_HELPER(client_desc, "Cannot open socket for incoming connection");

        if (DEBUG) fprintf(stderr, "Incoming connection accepted...\n");

        pthread_t thread;

        // put arguments for the new thread into a buffer
        handler_args_t* thread_args = malloc(sizeof(handler_args_t));
    
        thread_args->socket_desc = client_desc;
        thread_args->client_addr = client_addr;
        thread_args->surface_elevation = surface_elevation;
        thread_args->surface_texture = surface_texture;
        thread_args->vehicle_texture = vehicle_texture;


        if (pthread_create(&thread, NULL, connection_handler, (void*)thread_args) != 0) {
            fprintf(stderr, "Can't create a new thread, error %d\n", errno);
            exit(EXIT_FAILURE);
        }

        if (DEBUG) fprintf(stderr, "New thread created to handle the request!\n");

        pthread_detach(thread); // I won't phtread_join() on this thread

        // we can't just reset fields: we need a new buffer for client_addr!
        client_addr = calloc(1, sizeof(struct sockaddr_in));
    }

    exit(EXIT_SUCCESS); // this will never be executed
} 




  // not needed here
  //   // construct the world
  // World_init(&world, surface_elevation, surface_texture,  0.5, 0.5, 0.5);

  // // create a vehicle
  // vehicle=(Vehicle*) malloc(sizeof(Vehicle));
  // Vehicle_init(vehicle, &world, 0, vehicle_texture);

  // // add it to the world
  // World_addVehicle(&world, vehicle);


  
  // // initialize GL
  // glutInit(&argc, argv);
  // glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  // glutCreateWindow("main");

  // // set the callbacks
  // glutDisplayFunc(display);
  // glutIdleFunc(idle);
  // glutSpecialFunc(specialInput);
  // glutKeyboardFunc(keyPressed);
  // glutReshapeFunc(reshape);
  
  // WorldViewer_init(&viewer, &world, vehicle);

  
  // // run the main GL loop
  // glutMainLoop();

  // // check out the images not needed anymore
  // Image_free(vehicle_texture);
  // Image_free(surface_texture);
  // Image_free(surface_elevation);

  // // cleanup
  // World_destroy(&world);
//  return 0;             
//}
