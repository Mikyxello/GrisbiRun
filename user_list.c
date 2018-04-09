#include "user_list.h"
#include <assert.h>


void List_init(UserHead* head) {
  head->first=0;
  head->last=0;
  head->size=0;
}

UserList* List_find_for_id(UserHead* head,int id) {
  // linear scanning of list
  UserList* aux=head->first;
  while(aux){
    if (aux->id==id)
      return aux;
    aux=aux->next;
  }
  return 0;
}

UserList* List_find(UserHead* head, UserList* item) {
  // linear scanning of list
  UserList* aux=head->first;
  while(aux){
    if (aux==item)
      return item;
    aux=aux->next;
  }
  return 0;
}

UserList* List_insert(UserHead* head, UserList* prev, UserList* item) {
  if (item->next || item->prev)
    return 0;
  
#ifdef _LIST_DEBUG_
  // we check that the element is not in the list
  UserList* instance=List_find(head, item);
  assert(!instance);

  // we check that the previous is inthe list

  if (prev) {
    UserList* prev_instance=List_find(head, prev);
    assert(prev_instance);
  }
  // we check that the previous is inthe list
#endif

  UserList* next= prev ? prev->next : head->first;
  if (prev) {
    item->prev=prev;
    prev->next=item;
  }
  if (next) {
    item->next=next;
    next->prev=item;
  }
  if (!prev)
    head->first=item;
  if(!next)
    head->last=item;
  ++head->size;
  return item;
}

UserList* List_detach(UserHead* head, UserList* item) {

#ifdef _LIST_DEBUG_
  // we check that the element is in the list
  UserList* instance=List_find(head, item);
  assert(instance);
#endif

  UserList* prev=item->prev;
  UserList* next=item->next;
  if (prev){
    prev->next=next;
  }
  if(next){
    next->prev=prev;
  }
  if (item==head->first)
    head->first=next;
  if (item==head->last)
    head->last=prev;
  head->size--;
  item->next=item->prev=0;
  return item;
}
