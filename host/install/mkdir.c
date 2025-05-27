/*
 * Copyright (C) 2020 Yuan Jianpeng <yuan89@163.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>           /* Definition of AT_* constants */
#include <sys/stat.h>

#define SLASH '/'
#define IS_SLASH(c) ((c) == SLASH)
#define NIL '\0'
#define IS_NIL(c) ((c) == 0)

/*
	/
	////a
	////a/
	a///b
	a/b///
 */
char *next_component(char *path, int *pos)
{
	int p = *pos;
	char *component;

	if (p == 0) {
		/* skip multiple leading SLASH: ////a */
		while (IS_SLASH(path[p]))
			p++;
		component = p ? path + p - 1 : path;
	}
	else {
		/* restore previous slash */
		if (IS_NIL(path[p - 1]))
			path[p - 1] = SLASH;
		/* skip multiple middle SLASH a///b */
		while (IS_SLASH(path[p]))
			p++;
		component = path + p;
	}

	/* find component end position */
	while (! IS_NIL(path[p]) && ! IS_SLASH(path[p]))
		p++;

	/* no more components */
	if (path + p - component == 0)
		return NULL;

	/* Find a SLASH, make it NIL, and plus p */
	if (IS_SLASH(path[p]))
		path[p++] = NIL;

	*pos = p;
	return component;
}

int mkdir_p(char *path, unsigned mode)
{
	char *component;
	int pos = 0;
	int fd = AT_FDCWD, tmpfd;

	while ((component = next_component(path, &pos))) {
		if (!strcmp(component, "."))
			continue;
		if (strcmp(component, "..")) {
			if (mkdirat(fd, component, mode) < 0 && errno != EEXIST) {
				fprintf(stderr, "mkdir %s failed: %s\n", component, strerror(errno));
				return -1;
			}
		}
		tmpfd = openat(fd, component, O_RDONLY|O_DIRECTORY);
		if (tmpfd < 0) {
			fprintf(stderr, "open %s failed: %s\n", component, strerror(errno));
			return -1;
		}
		if (fd != AT_FDCWD)
			close(fd);
		fd = tmpfd;
	}

	if (fd != AT_FDCWD)
		close(fd);
	return 0;
}

int mkdir_l(char *path, unsigned mode)
{
	int ret;
	char *last = strrchr(path, SLASH);
	if (!last || last == path)
		return 0;
	*last = NIL;
	ret = mkdir_p(path, mode);
	*last = SLASH;
	return ret;
}

