#include "user_list.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

void Users_init(UserHead* head) {
  head->first=NULL;
  head->size=0;
}

User* User_find_id(UserHead* head, int id) {
  // linear scanning of User
  User* aux=head->first;
  while(aux){
    if (aux->id == id)
      return aux;
    aux=aux->next;
  }
  return 0;
}

User* User_find(UserHead* head, User* user) {
  // linear scanning of User
  User* aux=head->first;
  while(aux){
    if (aux==user)
      return user;
    aux=aux->next;
  }
  return 0;
}

User* User_insert_last(UserHead* head, User* user) {
  User* aux = head->first;

  if(aux == NULL) {
    head->first = user;
    head->size++;
    return user;
  }

  while(aux->next != NULL) {
    aux = aux->next;
  }

  aux->next = user;
  head->size++;

  return user;
}

int User_remove_id(UserHead* head, int id) {
  if (head == NULL) return 0;

  if (head->first->id == id) {
    head->first = NULL;
    head->size = 0;
    return 1;
  }

  User* user = head->first;

  while (user->next != NULL && user->next->id == id) {
    user->next = user->next->next;
    head->size = head->size - 1;
    return 1;
  }

  return 0;
}