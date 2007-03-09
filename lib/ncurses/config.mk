# $FreeBSD$

# This Makefile is shared by libncurses, libform, libmenu, libpanel.

NCURSES_DIR=	${.CURDIR}/../../../contrib/ncurses

NCURSES_CFG_H=	${.CURDIR}/ncurses_cfg.h

CFLAGS+=	-I.
.if exists(${.OBJDIR}/../ncurses${LIB_SUFFIX})
CFLAGS+=	-I${.OBJDIR}/../ncurses${LIB_SUFFIX}
.endif
CFLAGS+=	-I${.CURDIR}/../ncurses${LIB_SUFFIX}

# for ${NCURSES_CFG_H}
CFLAGS+=	-I${.CURDIR}/../ncurses

CFLAGS+=	-I${NCURSES_DIR}/include
CFLAGS+=	-I${NCURSES_DIR}/ncurses

CFLAGS+=	-Wall

CFLAGS+=	-DNDEBUG

CFLAGS+=	-DHAVE_CONFIG_H

# everyone needs this
.PATH:		${NCURSES_DIR}/include

# tools and directories
AWK?=		awk
TERMINFODIR?=	${SHAREDIR}/misc

# Generate headers
ncurses_def.h:	MKncurses_def.sh ncurses_defs
	AWK=${AWK} sh ${NCURSES_DIR}/include/MKncurses_def.sh \
	    ${NCURSES_DIR}/include/ncurses_defs > ncurses_def.h
