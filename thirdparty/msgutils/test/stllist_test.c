#include <stdio.h>
#include "msgutils/stllist.h"

#define array_size(arr) sizeof(arr)/sizeof((arr)[0])

static void dump(stllist_t list)
{
    iterator iter;
    int *value;

    printf("/***********************************\n");
    printf(" element count: %d\n", (int)stllist_size(list));
    for (iter = stllist_begin(list); iter != stllist_end(list); iter = stllist_next(iter)) {
        value = stllist_at(iter);
        printf(" %d ", *value);
    }
    printf("\n **********************************/\n");
}

static bool match_cb(void *data)
{
    return *((int *)data) % 3 == 0;
}

int main()
{
    int arr[10], i, val = 99;
    iterator it;
    stllist_t list = stllist_create(NULL);

    printf("\npush back 1...10 into the list\n");
    for (i = 0; i < array_size(arr); i++) {
        arr[i] = i + 1;
        stllist_pushback(list, &arr[i]);
    }
    dump(list);

    printf("\npop back all elements from the list\n");
    for (i = 0; i < array_size(arr); i++) {
        stllist_popback(list);
    }
    dump(list);

    printf("\npush front 1...10 into the list\n");
    for (i = 0; i < array_size(arr); i++) {
        arr[i] = i + 1;
        stllist_pushfront(list, &arr[i]);
    }
    dump(list);

    it = stllist_find(list, match_cb);
    if (it) {
        printf("\nfind the element[%d] that divisible by three at the position[%p]\n",
               *((int *)stllist_at(it)), it);

        printf("\ninsert 99 before the element[%d]\n", *((int *)stllist_at(it)));
        stllist_insert(list, it, &val);
    }
    dump(list);

    printf("\nremove all the elements that divisible by three\n");
    stllist_remove_if(list, match_cb);
    dump(list);

    printf("\nclear all the elements from the list\n");
    stllist_clear(list);
    dump(list);

    stllist_destroy(list);
    return 0;
}
