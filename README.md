# SO-Game-Project
The game consist of many players with their own vehicle texture go around the map (with a texture and an elevation given by the server) and each player can see other players drive around.
Multiplayer videogame C-based, a multithread server is opened on 25252 TCP port for enstablishing a connection with a new connecting client and on 8888 UDP port for requesting (client) and sending (server) updates through a datagram sockets.

## Authors
*[Andrea Misuraca](https://github.com/misu666)*
*[Michele Anselmi](https://github.com/Mikyxello)*

## How it works

### Server
The server opens all texture and elevation files loading them into memory and then opens a TCP socket and a UDP socket, initialize the user list (based on linked list paradigm). Create three threads, each one used for a specific operation:
- *TCP thread*: calls in the *TCP_handler* function giving *tcp_args* as parameter;
	- *TCP_handler*: accept the client connection and create a subthread for handling the user using the function *TCP_client_handler*;
		- *TCP_client_handler*: create a new user with the socket descriptor as id for him and insert him on the users list; then read the *PacketHeader* sent from client to know which type of message is, handling it with the function *TCP_packet*;
			- *TCP_packet*: check the message type and for each type execute an operation:
				- *GetId*: send the choosen id to the client requesting it;
				- *GetTexture*: send the map texture to the client requesting it, using a PostTexture type PacketHeader within a ImagePacket;
				- *GetElevation*: send the map elevation to the client requesting it, as it do for GetTexture;
				- *PostTexture*: receive the vehicle texture from the client and saves it into the server world;

- *UDP thread*: there are two UDP threads:
	- *UDP_receiver_thread*: using the UDP socket, receive the position e forces updates from client, using a *VehicleUpdatePacket* and then update the server world with the new vehicles;
	- *UDP_sender_thread*: using the same UDP socket as the receiver, send a *WorldUpdatePacket* send to all users the world update, where are stored all players positions;

- *Others*:
	- *tcp_args_t* struct is used for the TCP thread;

### Client

// TODO

# License
This project is licensed under the MIT License - see the LICENSE.md file for details
