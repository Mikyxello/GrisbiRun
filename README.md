# GrisbiRun
GrisbiRun consist of many players with their own vehicle and texture that go around the map (with a texture and an elevation given by the server) and each player can see other players drive around.
The game run through a multithread server that is opened on 25252 TCP port for enstablishing a connection with a new connecting client and on 8888 UDP port for requesting (client) and sending (server) updates through a datagram sockets.

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. 

### Prerequisites

Needed a linux distribution (preferably Ubuntu) and the GNU Compiler Collection (GCC)

### Installing

Just clone the repository with `git clone https://github.com/Mikyxello/SO-Game-Project.git` and compile it with `make`

### Start the game

For running the server, into the directory execute `./so_game_server <elevation_image> <texture_image>`
Then you should see
```
[MAIN] Server listening on port 25252...
[MAIN] Server UDP started...
[TCP HANDLER] Accepting connection from clients...
[UDP SENDER] Ready to send updates...
[UDP RECEIVER] Ready to receive updates...
```

And use `Ctrl-C` to stop running.

For running clients, into the directory run `./so_game_client <server_address> <player texture>`
Actually, server_address will be `127.0.0.1` and player_texture can be selected from those available in the image folder.

Then you should see
```
 _____        _       _      _  ______              
|  __ \      (_)     | |    (_) | ___ \             
| |  \/ _ __  _  ___ | |__   _  | |_/ /_   _  _ __  
| | __ | '__|| |/ __|| '_ \ | | |    /| | | || '_ \ 
| |_\ \| |   | |\__ \| |_) || | | |\ \| |_| || | | |
 \____/|_|   |_||___/|_.__/ |_| \_| \_|\__,_||_| |_|

[MAIN] Connection enstablished with server...
[TCP ID] Requesting ID...
[TCP ID] ID received: 5...
[TCP MAP TEXTURE] Requesting map texture...
[TCP MAP TEXTURE] Map texture loaded...
[TCP MAP ELEVATION] Requesting map elevation...
[TCP MAP ELEVATION] Map elevation loaded...
[TCP VEHICLE TEXTURE] Sending texture to server...
[TCP VEHICLE TEXTURE] Vehicle texture sent...
[TCP VEHICLE TEXTURE] Received texture back from server...
[UDP RECEIVER] Receiving updates...
[TCP CONNECTION CONTROLLER] Connection controller running...
[UDP SENDER] Sender thread started...
```

And you should see the window with the map texture and your vehicle moving around.
Uses arrows for moving, space to stop the vehicle and pag up/pag down for zooming in and out.
Use `Ctrl-C` for stopping, as the server.

## Authors

* **Michele Anselmi** - [Mikyxello](https://github.com/Mikyxello)
* **Andrea Misuraca** - [andreamisu](https://github.com/andreamisu)

See also the list of [contributors](https://github.com/Mikyxello/GrisbiRun/contributors) who participated in this project.

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details
