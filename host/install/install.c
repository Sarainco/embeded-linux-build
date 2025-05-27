/*
 * the install program for Xrouter
 * Copyright (C) 2020 Yuan Jianpeng <yuan89@163.com>
 *
 * this install implementation always compare the timestamp
 * if the dest is newer than the src, then install do nothing.
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
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/wait.h>

#define FORCE	0x100
#define STRIP	0x200
#define TODIR	0x400
/* keep directory hierarchy when install
	a relative path file */
#define KEEP_HIERARCHY	0x800
/* install a rc.d if the file is a init.d file
 */
#define RC	0x1000
// install as symbolic in batch mode
#define INSTALL_SYM	0x2000

static int dbg_level;
char *next_component(char *path, int *pos);
int mkdir_p(char *path, unsigned mode);
int mkdir_l(char *path, unsigned mode);

#define fatal(fmt, ...) do { fprintf(stderr, fmt, ##__VA_ARGS__); exit(1); } while (0)
#define debug(fmt, ...) do { if (dbg_level) printf(fmt, ##__VA_ARGS__); } while (0)

static void cp(int srcfd, char *src, char *dst, int len, unsigned mode)
{
	int fd_src, fd_dst, ret;

	fd_src = openat(srcfd, src, O_RDONLY);
	if (fd_src == -1)
		fatal("open %s failed: %s\n", src, strerror(errno));

	fd_dst = open(dst, O_WRONLY|O_TRUNC|O_CREAT, mode);
	if (fd_dst == -1)
		fatal("open %s failed: %s\n", dst, strerror(errno));

	while (len > 0) {
		ret = sendfile(fd_dst, fd_src, NULL, len);
		if (ret == 0)
			fatal("sendfile no enough data\n");
		else if (ret < 0)
			fatal("sendfile %s -> %s failed: %s\n", src, dst, strerror(errno));
		len -= ret;
	}

	close(fd_src);
	close(fd_dst);
}

static void ln(int srcfd, char *src, char *dst)
{
	char buf[1024];
	int ret;

	ret = readlinkat(srcfd, src, buf, sizeof(buf));
	if (ret < 0)
		fatal("read link '%s' failed: %s\n", src, strerror(errno));
	else if (ret >= sizeof(buf))
		fatal("read link '%s' overflow\n", src);
	buf[ret] = '\0';

	if (symlink(buf, dst) < 0)
		fatal("symlink %s -> %s failed: %s\n", dst, buf, strerror(errno));
}

static void strip(char *src, char *dst)
{
	static char *stripprog = NULL;
	static char *stripopt = NULL;
	int pid, status, ret;

	if (!stripprog)
		stripprog = getenv("STRIPPROG");
	if (!stripopt) {
		stripopt = getenv("STRIPOPT");
		if (!stripopt)
			stripopt = "--strip-debug";
	}
	if (!stripprog)
		fatal("set STRIPPROG env to specify the strip program\n");

	pid = fork();
	if (pid < 0)
		fatal("fork failed: %s\n", strerror(errno));
	else if (pid == 0) {
		char *argv[] = { stripprog, stripopt, "-o", dst, src, NULL };
		umask(022);
		if (execvp(argv[0], argv) < 0)
			fatal("execvp strip program %s failed: %s\n", argv[0], strerror(errno));
		exit(1);
	}
	ret = waitpid(pid, &status, 0);
	if (ret < 0)
		fatal("wait strip failed: %s\n", strerror(errno));
	if (ret != pid)
		fatal("wait return %d\n", ret);
	if (!WIFEXITED(status) || WEXITSTATUS(status))
		fatal("strip failed, status %d\n", WEXITSTATUS(status));
}

void make_dst(char *src, char *dir, char *dst, int dst_len)
{
	char *slash = strrchr(src, '/');
	if (!slash)
		slash = src;
	else {
		slash++;
		if (*slash == 0 || (*slash == '.' && slash[1] == 0)
			|| (*slash == '.' && slash[1] == '.' && slash[2] == 0))
			fatal("src %s is a directory path\n", src);
	}
	if (snprintf(dst, dst_len, "%s/%s", dir, slash) >= dst_len)
		fatal("make dest path overflow\n");
}

static void do_install(int srcfd, char *src, char *dst, unsigned mode, unsigned flags)
{
	struct stat stat_dst, stat_src;
	char _dst[1024];

	if (flags & KEEP_HIERARCHY) {
		if (snprintf(_dst, sizeof(_dst), "%s/%s", dst, src) >= sizeof(_dst))
			fatal("path too long\n");
		dst = _dst;
		mkdir_l(dst, 0755);
	}
	else if (flags & TODIR) {
		make_dst(src, dst, _dst, sizeof(_dst));
		dst = _dst;
	}

	if (fstatat(srcfd, src, &stat_src, AT_SYMLINK_NOFOLLOW) < 0)
		fatal("lstat %s failed: %s\n", src, strerror(errno));

	if (mode == 0)
		mode = stat_src.st_mode & 0777;

	if (lstat(dst, &stat_dst) == 0) {
		if (!(flags & FORCE)
				&& (stat_dst.st_mtim.tv_sec > stat_src.st_mtim.tv_sec
				 || (stat_dst.st_mtim.tv_sec == stat_src.st_mtim.tv_sec
					 && stat_dst.st_mtim.tv_nsec > stat_src.st_mtim.tv_nsec))) {
			debug("Skip %s\n", src);
			return;
		}
		unlink(dst);
	}

	if (S_ISREG(stat_src.st_mode)) {
		if (flags & STRIP) {
			debug("Strip %s\n", src);
			strip(src, dst);
		}
		else {
			debug("Copy %s\n", src);
			cp(srcfd, src, dst, stat_src.st_size, mode);
		}
	}
	else if (S_ISLNK(stat_src.st_mode)) {
		debug("Link %s\n", src);
		ln(srcfd, src, dst);
	}

	else
		fatal("src '%s': unsupported file type\n", src);
}

static void install_rc(int srcfd, char *src, char *dst)
{
	char *name, *tmp;
	char _dst[1024];
	char link[128];
	int fd, ret, start;
	struct stat statbuf;

	if (src != strstr(src, "etc/init.d/"))
		return;

	name = src + strlen("etc/init.d/");
	if (*name == '\0' || strchr(name, '/'))
		return;

	ret = snprintf(link, sizeof(link), "../init.d/%s", name);
	if (ret <= 0 || ret >= sizeof(link))
		fatal("path overflow\n");

	fd = openat(srcfd, src, O_RDONLY);
	if (fd < 0)
		fatal("open %s failed: %s\n", src, strerror(errno));

	ret = read(fd, _dst, 256);
	if (ret < 0)
		fatal("read %s failed: %s\n", src, strerror(errno));

	close(fd);
	_dst[ret] = '\0';
	tmp = strstr(_dst, "START=");
	if (!tmp)
		fatal("no START=xx in init script: %s\n", src);
	start = atoi(tmp + strlen("START="));
	if (start  < 1 || start > 99)
		fatal("invalid START=xx in init script: %s\n", src);

	// install S
	ret = snprintf(_dst, sizeof(_dst), "%s/etc/rc.d/S%02d%s", dst, start, name);
	if (ret <= 0 || ret >= sizeof(_dst))
		fatal("path overflow\n");

	mkdir_l(_dst, 0755);

	if (lstat(_dst, &statbuf) == 0)
		debug("Skip etc/rc.d/S%d%s\n", start, name);
	else if (symlink(link, _dst) == 0)
		debug("Link etc/rc.d/S%d%s\n", start, name);
	else
		fatal("symlink %s failed: %s\n", src, strerror(errno));

	// install K
	ret = snprintf(_dst, sizeof(_dst), "%s/etc/rc.d/K%02d%s", dst, 100 - start, name);
	if (ret <= 0 || ret >= sizeof(_dst))
		fatal("path overflow\n");

	if (lstat(_dst, &statbuf) == 0)
		debug("Skip etc/rc.d/K%d%s\n", 100-start, name);
	else if (symlink(link, _dst) == 0)
		debug("Link etc/rc.d/K%d%s\n", 100-start, name);
	else
		fatal("symlink %s failed: %s\n", src, strerror(errno));
}

static void install(int srcfd, char *src, char *dst, unsigned mode, unsigned flags)
{
	if ((flags & RC) && (flags & KEEP_HIERARCHY))
		install_rc(srcfd, src, dst);

	do_install(srcfd, src, dst, mode, flags);
}

static void install_sym(char *src, char *dst, char *sub)
{
	char rel[512];
	char _dst[512];
	int ret;
	int n = 0;
	char *c0, *c1, *last;
	int p0 = 0, p1 = 0;

	ret = snprintf(_dst, sizeof(_dst), "%s/%s", dst, sub);
	if (ret <= 0 || ret >= sizeof(_dst))
		fatal("path overflow");

	while (1) {
		c0 = next_component(dst, &p0);
		c1 = next_component(src, &p1);
		if (!c0 || !c1 || strcmp(c0, c1))
			break;
	}
	while (c0) {
		n += snprintf(rel + n, sizeof(rel) - n, "../");
		c0 = next_component(dst, &p0);
	}

	last = strrchr(sub, '/');
	if (last && last != sub) {
		*last = '\0';
		p0 = 0;
		c0 = next_component(sub, &p0);
		while (c0) {
			n += snprintf(rel + n, sizeof(rel) - n, "../");
			c0 = next_component(sub, &p0);
		}
		*last = '/';
	}

	while (c1) {
		n += snprintf(rel + n, sizeof(rel) - n, "%s/", c1);
		c1 = next_component(src, &p1);
	}

	n += snprintf(rel + n, sizeof(rel) - n, "%s", sub);
	if (n >= sizeof(rel))
		fatal("path overflow\n");

	mkdir_l(_dst, 0755);
	ret = symlink(rel, _dst);
	if (ret < 0 && errno != EEXIST)
		fatal("symlink failed: %s\n", strerror(errno));
}

void batch_install(char *src, char *dst, int flags)
{
	int i, n = 0, len, ret;
	struct {
		DIR *dir;
		char name[128];
	} dirs[16];
	struct dirent *dirent;
	char parent[512];
	char sub_path[512];
	int src_fd;

	if (flags & INSTALL_SYM) {
		src = strdup(src);
		dst = strdup(dst);
		if (!src || !dst)
			fatal("strdup failed\n");
	}

	src_fd = open(src, O_RDONLY|O_DIRECTORY);
	if (src_fd < 0)
		fatal("open directory %s failed: %s\n", src, strerror(errno));

	dirs[0].dir = opendir(src);
	if (dirs[0].dir == NULL)
		fatal("opendir %s failed: %s\n", src, strerror(errno));

	n = 1;

	while (n > 0) {

		parent[0] = 0;
		len = 0;
		for (i = 1; i < n; i++) {
			ret = snprintf(parent + len, sizeof(parent) - len, "%s/", dirs[i].name);
			if (ret <= 0 || ret >= sizeof(parent) - len)
				fatal("path overflow\n");
			len += ret;
		}

		while ((dirent = readdir(dirs[n - 1].dir))) {
			if (!strcmp(dirent->d_name, ".")
				|| !strcmp(dirent->d_name, ".."))
				continue;

			ret = snprintf(sub_path, sizeof(sub_path), "%s%s", parent, dirent->d_name);
			if (ret <= 0 || ret >= sizeof(sub_path))
				fatal("path overflow\n");

			// some filesystem doesn't set d_type
			if (dirent->d_type == DT_UNKNOWN) {
				struct stat statbuf;
				if (fstatat(src_fd, sub_path, &statbuf, AT_SYMLINK_NOFOLLOW) < 0)
					fatal("lstat %s failed: %s\n", sub_path, strerror(errno));
				if (S_ISDIR(statbuf.st_mode))
					dirent->d_type = DT_DIR;
				else if (S_ISREG(statbuf.st_mode))
					dirent->d_type = DT_REG;
				else if (S_ISLNK(statbuf.st_mode))
					dirent->d_type = DT_LNK;
			}

			if (dirent->d_type == DT_DIR)
				break;
			if (flags & INSTALL_SYM)
				install_sym(src, dst, sub_path);
			else if (dirent->d_type == DT_LNK || dirent->d_type == DT_REG)
				install(src_fd, sub_path, dst, 0, flags);
			else
				fatal("unsupported file type: %s\n", dirent->d_name);
		}

		if (dirent) {
			if (n >= sizeof(dirs)/sizeof(dirs[0]))
				fatal("path too depth\n");

			if (strlen(dirent->d_name) >= sizeof(dirs[0].name))
				fatal("dirname length overflow\n");

			strcpy(dirs[n].name, dirent->d_name);

			ret = snprintf(sub_path, sizeof(sub_path), "%s/%s%s", src, parent, dirent->d_name);
			if (ret <= 0 || ret >= sizeof(sub_path))
				fatal("path overflow\n");

			dirs[n].dir = opendir(sub_path);
			if (dirs[n].dir == NULL)
				fatal("opendir %s failed: %s\n", sub_path, strerror(errno));

			n++;
		}
		else
			n--;
	}
}

static int is_dir(char *file)
{
	struct stat statbuf;
	if (stat(file, &statbuf) < 0)
		return 0;
	return S_ISDIR(statbuf.st_mode);
}

static void usage()
{
	fprintf(stderr,
		"Xrouter install program.\n"
		"This INSTALL skip install if the timestamp of dest is newer\n"
		"\n"

		"install -d DIRECTORY ... \n"
		"    create all components of the given directory(ies)\n"
		"\n"

		"install [-D] [-T] SOURCE DEST\n"
		"    install SOURCE as DEST or child of existing DEST directory\n"
		"    -T, no target directory (force install as DEST)\n"
		"    -D, create leading component of DEST or all components if DEST end with /\n"
		"\n"

		"install [-D] SOURCE ... DEST\n"
		"    install multiple SOURCEs to DEST directory\n"
		"    -D, create all components of DEST\n"
		"\n"

		"install [-D] -t DEST SOURCE ... \n"
		"    install SOURCE to DEST directory\n"
		"    -D, create all components of DEST\n"
		"\n"

		"install [-D] [-l] -b SOURCE ... DEST\n"
		"    install all sub files in SOURCE to DEST (batch mode)\n"
		"    -D, create all components of DEST\n"
		"    -l, install as symbol link\n"
		"    this mode ignore -s, -m, -k option\n"
		"\n"

		"options:\n"
		"    -C dir, change working directory\n"
		"    -s, STRIP binary, need STRIPPROG env and optional STRIPOPT env\n"
		"    -f, force install\n"
		"    -i, install rc.d for init scripts under /etc/init.d/, depends on -k\n"
		"    -m mode, file mode\n"
		"    -k, keep directory hierarchy\n"
		"    -v, enable debug output\n"
		"    -h, show this usage\n"
		"\n"
	);

	exit(1);
}

int main(int argc, char **argv)
{
	int opt;
	unsigned mode = 0, flags = 0;
	char *cwd = NULL;
	char *target = NULL;
	int directory_mode = 0;
	int batch_mode = 0;
	int no_target_directory = 0;
	int create_dir = 0;
	int i;

	while ((opt = getopt(argc, argv, "cC:sfkim:Dt:Tdblvh")) != -1 ) {
		switch(opt) {
		case 'c': break;
		case 'C': cwd = optarg; break;
		case 's': flags |= STRIP; break;
		case 'f': flags |= FORCE; break;
		case 'k': flags |= KEEP_HIERARCHY; break;
		case 'i': flags |= RC; break;
		case 'm': mode = strtoul(optarg, NULL, 8); break;
		case 'D': create_dir = 1; break;
		case 't': target = optarg; break;
		case 'T': no_target_directory = 1; break;
		case 'd': directory_mode = 1; break;
		case 'b': batch_mode = 1; break;
		case 'l': flags |= INSTALL_SYM; break;
		case 'v': dbg_level++; break;
		case 'h': usage();
		default: fprintf(stderr, "invalid argument\n"); return 1;
		}
	}

	if (cwd) {
		if (chdir(cwd) < 0)
			fatal("chdir() to %s failed: %s\n", cwd, strerror(errno));
	}

	umask(0);

	/* -d directory ... */
	if (directory_mode) {
		if (mode == 0)
			mode = 0755;
		for (i = optind; i < argc; i++)
			mkdir_p(argv[i], mode);
	}

	else if (batch_mode) {
		if (argc - optind < 2)
			fatal("batch mode: insufficient argument\n");
		if (create_dir)
			mkdir_p(argv[argc - 1], 0755);
		flags &= (FORCE|RC|INSTALL_SYM);
		flags |= KEEP_HIERARCHY;
		for (i = optind; i < argc - 1; i++)
			batch_install(argv[i], argv[argc - 1], flags);
	}

	/* [-D] -t target_directory source ... */
	else if (target) {
		if (no_target_directory)
			fatal("can't combine -t and -T\n");
		if (argc - optind == 0)
			fatal("no source\n");
		if (create_dir)
			mkdir_p(target, 0755);
		for (i = optind; i < argc; i++)
			install(AT_FDCWD, argv[i], target, mode, flags|TODIR);
	}

	/* [-T] src dst (dst may be file or directory) */
	else if (argc - optind == 2 ) {
		if (create_dir)
			mkdir_l(argv[optind+1], 0755);
		if (is_dir(argv[optind+1])) {
			if (no_target_directory)
				fatal("%s is a directory\n", argv[optind+1]);
			flags |= TODIR;
		}
		install(AT_FDCWD, argv[optind], argv[optind+1], mode, flags);
	}

	else if (argc - optind > 2 ) {
		if (create_dir)
			mkdir_p(argv[argc - 1], 0755);
		flags |= TODIR;
		for (i = optind; i < argc - 1; i++)
			install(AT_FDCWD, argv[i], argv[argc - 1], mode, flags);
	}

	else
		fatal("no source or dest\n");

	return 0;
}

