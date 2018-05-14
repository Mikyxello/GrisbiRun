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

  while (user != NULL && user->next != NULL) {
    if(user->next->id == id) {
      user->next = user->next->next;
      return 1;
    }

    user = user->next;
  }

  return 0;
}

User* User_find_prev(UserHead* head, int id) {
  // linear scanning of User
  User* aux=head->first;
  while(aux){
    if (aux->next != NULL) {
      if (aux->next->id == id) {
        return aux;
      }
    }
    aux=aux->next;
  }
  return 0;
}

int User_detach(UserHead* head, int id) {

  // we check that the element is in the list
  User* user = User_find_id(head, id);

  if(!user) return -1;

  User* prev = User_find_prev(head, id);
  User* next = user->next;

  if (prev) {
    prev->next = next;
  }
  else if(next){
    head->first = next;
  }
  else if(user) {
    head->first = NULL;
  }
  
  head->size--;
  return 1;
}