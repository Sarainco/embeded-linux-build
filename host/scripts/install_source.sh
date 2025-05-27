#!/bin/bash

# 1. untar source code tarball
# 2. create git repository
# 3. apply patches, each patches applied as a commit

GIT_DIR=$1
SRC_DIR=$2
BUILD_DIR=$3
PATCH_DIR=$4
TARBALL=$5

cancel_install ()
{
	rm -fr "$GIT_DIR" "$SRC_DIR"
	exit 1
}

run ()
{
	echo "$@"
	"$@" >/dev/null || {
		cancel_install
		exit 1
	}
}

src_changed ()
{
	[ ! -f "$TARBALL" -a ! -d $SRC_DIR ] && {
		echo no src tar ball $TARBALL 1>&2
		exit 1
	}

	[ "$TARBALL" -nt $GIT_DIR/.prepare ] && return 0

	if [ -n "$PATCH_DIR" -a -d "$PATCH_DIR" ] ; then
		for f in "$PATCH_DIR"/*.patch ; do
			[ "$f" -nt $GIT_DIR/.prepare ] && return 0
		done
	fi

	return 1
}

src_dirty_check ()
{
	[ -d "$SRC_DIR" ] || return
	git -C "$SRC_DIR" diff-index --quiet --cached HEAD || {
		echo "`basename $SRC_DIR` index is dirty, skip install to avoid source code lost" 1>&2
		exit 1
	}
	git -C "$SRC_DIR" diff-files --quiet || {
		echo "`basename $SRC_DIR` worktree is dirty, skip install to avoid source code lost" 1>&2
		exit 1
	}
}

install_src ()
{
	src_changed || return 0
	src_dirty_check

	trap cancel_install INT

	run rm -fr "$GIT_DIR" "$SRC_DIR"

	SRC=`dirname "$SRC_DIR"`
	mkdir -p "$SRC" "$GIT_DIR"

	run tar xf "$TARBALL" -C "$SRC"

	run cd $SRC_DIR
	run git init --separate-git-dir="$GIT_DIR" "$SRC_DIR"
	# disable auto pack, to prevent git output auto pack messages to stderr
	run git config --local gc.auto 0
	run git add --force .
	run git commit -m "add original source"

	# patch
	if [ -d "$PATCH_DIR" ] ; then
		for f in "$PATCH_DIR"/*.patch ; do
			[ ! -f "$f" ] ||  run git am "$f"
		done
	fi

	run touch ${GIT_DIR}/.prepare

	trap "" INT
}

install_build ()
{
	[ -n "$BUILD_DIR" ] || return 0

	[ -f $BUILD_DIR/Makefile -a \
		$GIT_DIR/.prepare -nt $BUILD_DIR/Makefile ] && {
		run rm -fr $BUILD_DIR
	}

	[ -d $BUILD_DIR ] || run mkdir -p $BUILD_DIR
}

install_src
install_build

