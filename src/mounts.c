/*
 * mounts.c - Minimal linked list set of mount points
 * Copyright (c) 2016 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved. 
 *
 * This software may be freely redistributed and/or modified under the
 * terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING. If not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor 
 * Boston, MA 02110-1335, USA.
 *
 * Authors:
 *   Steve Grubb <sgrubb@redhat.com>
 */

#include "config.h"
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include "mounts.h"
#include "message.h"

typedef struct _lnode{
	const char *path;
	struct _lnode *next;  // Next node pointer
} lnode;

typedef struct {
	lnode *head;          // List head
	lnode *cur;           // Pointer to current node
	unsigned int cnt;     // How many items in this list
} llist;

static llist mounts;

static void mounts_create(llist *l)
{
        l->head = NULL;
        l->cur = NULL;
        l->cnt = 0;
}

static void mounts_last(llist *l)
{
	register lnode* window;

	if (l->head == NULL)
		return;

	window = l->head;
	while (window->next)
		window = window->next;

	l->cur = window;
}

static int mounts_append(llist *l, const char *buf, unsigned int lineno)
{
        lnode* newnode;

	if (buf) {
		newnode = malloc(sizeof(lnode));
		newnode->path = strdup(buf);
	} else
		return 1;

	newnode->next = NULL;
	mounts_last(l);

	// if we are at top, fix this up
	if (l->head == NULL)
		l->head = newnode;
	else    // Otherwise add pointer to newnode
		l->cur->next = newnode;

	// make newnode current
	l->cur = newnode;
	l->cnt++;

	return 0;
}

static char *get_line(FILE *f, char *buf, int size)
{
	if (fgets_unlocked(buf, size, f)) {
		/* remove newline */
		char *ptr = strchr(buf, 0x0a);
		if (ptr)
			*ptr = 0;
		return buf;
	}
	return NULL;
}

int load_mounts(void)
{
	FILE *f;
	int lineno = 1;
	char buf[PATH_MAX+1];

	mounts_create(&mounts);

	f = fopen(MOUNTS_CONFIG_FILE, "r");
	
	if (f == NULL) {
		msg(LOG_ERR, "Error while opening (%s)", strerror(errno));
		return 1;
	}

	while (get_line(f, buf, PATH_MAX)) {
		if (buf[0] != '#') { 
			char *ptr = buf;

			while (*ptr == ' ')
				ptr++;

			if (*ptr == '/') {
				struct stat sb;

				if (stat(ptr, &sb) == -1) {
					msg(LOG_ERR, "Cannot stat %s: %s", 
						ptr, strerror(errno));
					return 1;	
				}

				if (!S_ISDIR(sb.st_mode)) {
					msg(LOG_ERR, "Mount point %s is not a"
						" directory", ptr);
					return 1;	
				}

				mounts_append(&mounts, ptr, lineno);
			}
		}

		lineno++;
	}

	fclose(f);

	if (mounts.cnt == 0) { 
		msg(LOG_INFO, "No mount points - exiting");
		return 1;
	}

	return 0;
}

const char *first_mounts(void)
{
	llist *l = &mounts;
	l->cur = l->head;

	if (l->cur == NULL)
		return NULL;
	return l->cur->path;
}

const char *next_mounts(void)
{
	llist *l = &mounts;
	if (l->cur == NULL)
		return NULL;

	l->cur = l->cur->next;
	if (l->cur == NULL)
		return NULL;
	return l->cur->path;
}

void clear_mounts(void)
{
	llist *l = &mounts;
	lnode* nextnode;
        register lnode* current;

        current = l->head;
	while (current) {
		nextnode=current->next;
		free((void *)current->path);
		free((void *)current);
		current=nextnode;
	}
	l->head = NULL;
	l->cur = NULL;
	l->cnt = 0;
}

