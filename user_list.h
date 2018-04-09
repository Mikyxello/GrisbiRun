#pragma once

typedef struct UserList {
  struct UserList* next;
  struct UserList* prev;
  int id;
  int socket_desc;
  struct sockaddr_in server_addr;
  Vehicle* vehicle;
  float x,y, z, theta; //position and orientation of the vehicle, on the surface
} UserList;

typedef struct UserHead {
  struct UserList* first;
  struct UserList* last;
  int size;
} UserHead;


//funzioni da riscrivere per questa struct

void List_init(ListHead* head);
ListItem* List_find(ListHead* head, ListItem* item);
ListItem* List_insert(ListHead* head, ListItem* previous, ListItem* item);
ListItem* List_detach(ListHead* head, ListItem* item);
