#pragma once

typedef struct User {
  struct User* next;
  struct User* prev;
  int id;
  int socket_desc;
  struct sockaddr_in server_addr;
  Vehicle* vehicle;
  float x,y, z, theta; //position and orientation of the vehicle, on the surface
} User;

typedef struct UserHead {
  struct User* first;
  struct User* last;
  int size;
} UserHead;

void Users_init(UserHead* head);
User* User_find_id(UserHead* head, int id);
User* User_find(UserHead* head, User* user);
User* User_insert(UserHead* head, User* previous, User* user);
User* User_detach(UserHead* head, User* user);
