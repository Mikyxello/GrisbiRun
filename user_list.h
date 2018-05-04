#pragma once
#include <netinet/in.h>
#include "vehicle.h"

typedef struct User {
  struct User* next;
  struct User* prev;
  int id;
  struct sockaddr_in user_addr_tcp;
  struct sockaddr_in user_addr_udp;
  Vehicle* vehicle;
  float x, y, theta;
  float rotational_force, translational_force;
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
User* User_insert_last(UserHead* head, User* user);
User* User_detach(UserHead* head, User* user);

// TODO: Eliminare previous per compattare il tutto (la lista scorre sequenzialmente senza tornare indietro, inutile anche last in UserHead)