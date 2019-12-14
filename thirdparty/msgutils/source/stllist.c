/* The MIT License (MIT)
 *
 * Copyright (c) 2019 LUOYUN <sysu.zqlong@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>
#include "msgutils/os_memory.h"
#include "msgutils/stllist.h"

struct stllist_node {
    void *data;
    struct stllist_node *prev;
    struct stllist_node *next;
};

struct stllist {
    struct stllist_node *end;
    void (*free_cb)(void *data);
    size_t elem_count;
};

static struct stllist_node *stllist_node_create(void *data)
{
    struct stllist_node *node = OS_MALLOC(sizeof(struct stllist_node));
    if (node != NULL)
        node->data = data;
    return node;
}

static void stllist_node_destroy(struct stllist_node *node, void (*free_cb)(void *data))
{
    if (free_cb != NULL && node->data != NULL)
        free_cb(node->data);
    OS_FREE(node);
}

stllist_t stllist_create(void (*free_cb)(void *data))
{
    struct stllist *list;
    struct stllist_node *node;

    list = OS_MALLOC(sizeof(struct stllist));
    if (list == NULL) {
        fprintf(stderr, "Out of memory\n");
        return NULL;
    }

    node = stllist_node_create(NULL);
    if (node == NULL) {
        fprintf(stderr, "Out of memory\n");
        OS_FREE(list);
        return NULL;
    }
    node->next = node;
    node->prev = node;

    list->end = node;
    list->free_cb = free_cb;
    list->elem_count = 0;

    return list;
}

void stllist_destroy(const stllist_t list)
{
    stllist_t temp = list;
    stllist_clear(list);
    stllist_node_destroy(list->end, NULL);
    OS_FREE(temp);
}

size_t stllist_size(const stllist_t list)
{
    return list->elem_count;
}

bool stllist_empty(const stllist_t list)
{
    return list->end->next == list->end;
}

iterator stllist_begin(const stllist_t list)
{
    return list->end->next;
}

iterator stllist_end(const stllist_t list)
{
    return list->end;
}

iterator stllist_rbegin(const stllist_t list)
{
    return list->end->prev;
}

iterator stllist_rend(const stllist_t list)
{
    return list->end;
}

iterator stllist_next(iterator iter)
{
    return ((struct stllist_node *)iter)->next;
}

iterator stllist_prev(iterator iter)
{
    return ((struct stllist_node *)iter)->prev;
}

int stllist_pushfront(const stllist_t list, void *data)
{
    iterator iter = stllist_insert(list, stllist_begin(list), data);
    if (iter == NULL)
        return -1;
    return 0;
}

void stllist_popfront(const stllist_t list)
{
    stllist_erase(list, stllist_begin(list));
}

int stllist_pushback(const stllist_t list, void *data)
{
    iterator iter = stllist_insert(list, stllist_end(list), data);
    if (iter == NULL)
        return -1;
    return 0;
}

void stllist_popback(const stllist_t list)
{
    stllist_erase(list, stllist_rbegin(list));
}

void *stllist_front(const stllist_t list)
{
    return ((struct stllist_node *)stllist_begin(list))->data;
}

void *stllist_back(const stllist_t list)
{
    return ((struct stllist_node *)stllist_rbegin(list))->data;
}

void *stllist_at(iterator iter)
{
    return ((struct stllist_node *)iter)->data;
}

iterator stllist_insert(const stllist_t list, iterator iter, void *data)
{
    struct stllist_node *position = (struct stllist_node *)iter;
    struct stllist_node *new_node = stllist_node_create(data);

    if (new_node == NULL) {
        fprintf(stderr, "Out of memory\n");
        return NULL;
    }

    new_node->next = position;
    new_node->prev = position->prev;
    position->prev->next = new_node;
    position->prev = new_node;

    list->elem_count++;
    return (iterator)new_node;
}

iterator stllist_erase(const stllist_t list, iterator iter)
{
    struct stllist_node *position = (struct stllist_node *)iter;
    struct stllist_node *next_node = position->next;
    struct stllist_node *prev_node = position->prev;

    if (iter == stllist_end(list)) {
        fprintf(stderr, "Out of range\n");
        return (iterator)next_node;
    }

    prev_node->next = next_node;
    next_node->prev = prev_node;

    stllist_node_destroy(position, list->free_cb);
    list->elem_count--;
    return (iterator)next_node;
}

void stllist_clear(const stllist_t list)
{
    struct stllist_node *current = (struct stllist_node *)stllist_begin(list);

    while (current != stllist_end(list)) {
        struct stllist_node *temp = current;
        current = current->next;
        stllist_node_destroy(temp, list->free_cb);
    }

    list->end->next = list->end;
    list->end->prev = list->end;
    list->elem_count = 0;
}

iterator stllist_find(const stllist_t list, bool (*match_cb)(void *data))
{
    struct stllist_node *current = (struct stllist_node *)stllist_begin(list);

    if (match_cb == NULL)
        return NULL;

    while (current != stllist_end(list)) {
        if (match_cb(current->data))
            return (iterator)current;
        current = current->next;
    }
    return NULL;
}

void stllist_remove_if(const stllist_t list, bool (*match_cb)(void *data))
{
    struct stllist_node *current = (struct stllist_node *)stllist_begin(list);

    if (match_cb == NULL)
        return;

    while (current != stllist_end(list)) {
        struct stllist_node *temp = current;
        current = current->next;
        if (match_cb(temp->data))
            stllist_erase(list, temp);
    }
}
