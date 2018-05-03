#include "user_User.h"
#include <assert.h>

void User_init(UserHead* head) {
  head->first=NULL;
  head->last=NULL;
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

User* User_insert(UserHead* head, User* prev, User* user) {
  if (user->next || user->prev)
    return 0;
  
#ifdef _User_DEBUG_
  // we check that the element is not in the User
  User* instance=User_find(head, user);
  assert(!instance);

  // we check that the previous is inthe User

  if (prev) {
    User* prev_instance=User_find(head, prev);
    assert(prev_instance);
  }
  // we check that the previous is inthe User
#endif

  User* next= prev ? prev->next : head->first;
  if (prev) {
    user->prev=prev;
    prev->next=user;
  }
  if (next) {
    user->next=next;
    next->prev=user;
  }
  if (!prev)
    head->first=user;
  if(!next)
    head->last=user;
  ++head->size;
  return user;
}

User* User_detach(UserHead* head, User* user) {

#ifdef _User_DEBUG_
  // we check that the element is in the User
  User* instance=User_find(head, user);
  assert(instance);
#endif

  User* prev=user->prev;
  User* next=user->next;
  if (prev){
    prev->next=next;
  }
  if(next){
    next->prev=prev;
  }
  if (user==head->first)
    head->first=next;
  if (user==head->last)
    head->last=prev;
  head->size--;
  user->next=user->prev=0;
  return user;
}
