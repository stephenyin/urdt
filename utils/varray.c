#include <stdlib.h>
#include "varray.h"
#include "vassert.h"
#include <time.h>


/*
 *  for array
 * @array:
 * @first_capc:
 */
int varray_init(struct varray* array, int first_capc)
{
    vassert(array);

    array->capc  = 0;
    array->used  = 0;
    array->first = first_capc;
    array->items = NULL;
    if (!array->first) {
        array->first = VARRAY_FIRST_CAPC;
    }
    return 0;
}

void varray_deinit(struct varray* array)
{
    vassert(array);

    if (array->items) {
        free(array->items);
        array->items = NULL;
    }
    return ;
}

int varray_size(struct varray* array)
{
    vassert(array);
    return array->used;
}

void* varray_get(struct varray* array, int idx)
{
    vassert(array);

    if(!array->items || idx < 0 || idx >= array->used){
        return NULL;
    }
    return array->items[idx];
}

void* varray_get_rand(struct varray* array)
{
    int idx = 0;
    vassert(array);

    if(!array->items){
        return NULL;
    }

    srand((int)time(NULL));
    idx = rand() % (array->used);
    return array->items[idx];
}

int varray_set(struct varray* array, int idx, void* item)
{
    vassert(array);

    if(!item ||idx < 0 ||idx >= array->used ||!array->items ){
        return -1;
    }
    array->items[idx] = item;
    return 0;
}

static
int _aux_extend(struct varray* array)
{
    void* items = NULL;
    int   capc  = 0;

    vassert(array);
    vassert(array->used >= array->capc);

    if (!array->capc) {
        capc = array->first;
    } else {
        capc = array->capc << 1;
    }

    items = realloc(array->items, capc * sizeof(void*));
    if(items == NULL){
        return -1;
    }

    array->capc  = capc;
    array->items = (void**)items;
    return 0;
}

int varray_add(struct varray* array, int idx, void* new)
{
    int i = array->used;
    vassert(array);

    if(idx < 0 || idx >= array->used ||!new){
        return -1;
    }

    if (array->used >= array->capc) {
        if(_aux_extend(array) < 0){
            return 0;
        }
    }
    for (; i >= idx; i--) {
        array->items[i+1] = array->items[i];
    }
    array->items[idx] = new;
    array->used++;
    return 0;
}

int varray_add_tail(struct varray* array, void* item)
{
    vassert(array);
    if(item == NULL){
        return -1;
    }

    if (array->used >= array->capc) {
        if(_aux_extend(array) < 0){
            return -1;
        }
    }
    array->items[array->used++] = item;
    return 0;
}

static
int _aux_shrink(struct varray* array)
{
    void* new_items = NULL;
    int   new_capc  = array->capc >> 1;

    vassert(array);
    vassert(array->items);
    vassert(array->used * 2 <= array->capc);

    if(array->capc == array->first){
        return 0;
    }

    new_items = realloc(array->items, new_capc * sizeof(void*));
    if(!new_items){
        return -1;
    }

    array->capc  = new_capc;
    array->items = (void**)new_items;
    return 0;
}

void* varray_del(struct varray* array, int idx)
{
    void* item = NULL;
    int i = 0;
    vassert(array);

    if(idx < 0 || idx >= array->used){
        return NULL;
    }
    if ((array->capc >= array->first)
        && (array->capc > array->used * 2)) {
        if(_aux_shrink(array) < 0){
            return NULL;
        }
    }

    item = array->items[idx];
    for (i = idx; i < array->used; i++) {
        array->items[i] = array->items[i+1];
    }
    array->items[i] = NULL;
    array->used--;
    return item;
}

void* varray_pop_tail(struct varray* array)
{
    void* item = NULL;
    vassert(array);

    if ((array->capc >= array->first)
        && (array->capc > array->used * 2)) {
        if(_aux_shrink(array) < 0){
            return NULL;
        }
    }
    item = array->items[--array->used];
    array->items[array->used] = NULL;
    return item;
}

void varray_iterate(struct varray* array, varray_iterate_t cb, void* cookie)
{
    int i = 0;
    vassert(array);

    if(!cb){
        return;
    }

    for (; i < array->used; i++) {
        if (cb(array->items[i], cookie) > 0) {
            break;
        }
    }
    return ;
}

void varray_zero(struct varray* array, varray_zero_t zero_cb, void* cookie)
{
    vassert(array);
    if(!zero_cb){
        return;
    }

    while(varray_size(array) > 0) {
        void* item = varray_pop_tail(array);
        zero_cb(item, cookie);
    }
    return;
}

/*
 * for sorted array.
 */
int vsorted_array_init(struct vsorted_array* sarray, int first_capc, varray_cmp_t cb, void* cookie)
{
    vassert(sarray);

    if(!cb){
        return -1;
    }

    varray_init(&sarray->array, first_capc);
    sarray->cmp_cb = cb;
    sarray->cookie = cookie;
    return 0;
}

void vsorted_array_deinit(struct vsorted_array* sarray)
{
    vassert(sarray);
    varray_deinit(&sarray->array);
    return ;
}

int vsorted_array_size(struct vsorted_array* sarray)
{
    vassert(sarray);
    return varray_size(&sarray->array);
}

void* vsorted_array_get(struct vsorted_array* sarray, int idx)
{
    vassert(sarray);
    return varray_get(&sarray->array, idx);
}

int vsorted_array_add(struct vsorted_array* sarray, void* new)
{
    void* item = NULL;
    int sz = varray_size(&sarray->array);
    int i  = 0;

    vassert(sarray);
    if(!new){
        return -1;
    }

    for (; i < sz; i++) {
        item = varray_get(&sarray->array, i);
        if (sarray->cmp_cb(item, new, sarray->cookie) >= 0) {
            // "new" is better than current "item"
            break;
        }
    }
    if (i < sz) {
        return varray_add(&sarray->array, i, new);
    } else {
        return varray_add_tail(&sarray->array, new);
    }
    return 0;
}

void* vsorted_array_del(struct vsorted_array* sarray, void* todel)
{
    void* item = NULL;
    int sz = varray_size(&sarray->array);
    int i  = 0;

    vassert(sarray);
    if(!todel){
        return NULL;
    }

    for (; i < sz; i++) {
       	item = varray_get(&sarray->array, i);
        if (sarray->cmp_cb(todel, item, &sarray->cookie) == 0) {
            break;
        }
    }

    if(i >= sz){
        return NULL;
    }

    return varray_del(&sarray->array, i);
}


void vsorted_array_iterate(struct vsorted_array* sarray, varray_iterate_t cb, void* cookie)
{
    vassert(sarray);
    if(!cb){
        return;
    }

    varray_iterate(&sarray->array, cb, cookie);
    return ;
}

void vsorted_array_zero(struct vsorted_array* sarray, varray_zero_t zero_cb, void* cookie)
{
    vassert(sarray);
    if(!zero_cb){
        return;
    }
    varray_zero(&sarray->array, zero_cb, cookie);
    return ;
}

