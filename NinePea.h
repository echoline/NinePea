#ifndef NINEPEA_H
#define NINEPEA_H

#include <string.h>
#include <stdlib.h>

#define put2(buffer, index, value) \
	buffer[index++] = value & 0xFF; \
	buffer[index++] = (value >> 8) & 0xFF; \

#define put4(buffer, index, value) \
	put2(buffer, index, value) \
	put2(buffer, index, value >> 16) \

#define put8(buffer, index, lvalue, hvalue) \
	put4(buffer, index, lvalue) \
	put4(buffer, index, hvalue) \

#define get4(buffer, index, value) \
	value = buffer[index++]; \
	value |= buffer[index++] << 8; \
	value |= buffer[index++] << 16; \
	value |= buffer[index++] << 24; \

#define get2(buffer, index, value) \
	value = buffer[index++]; \
	value |= buffer[index++] << 8; \

#define MAX_MSG 256
#define MAX_WELEM 16
#define NOTAG ~0

/* 9P message types */
enum {
	TVersion = 100,
	RVersion,
	TAuth = 102,
	RAuth,
	TAttach = 104,
	RAttach,
	TError = 106, /* illegal */
	RError,
	TFlush = 108,
	RFlush,
	TWalk = 110,
	RWalk,
	TOpen = 112,
	ROpen,
	TCreate = 114,
	RCreate,
	TRead = 116,
	RRead,
	TWrite = 118,
	RWrite,
	TClunk = 120,
	RClunk,
	TRemove = 122,
	RRemove,
	TStat = 124,
	RStat,
	TWStat = 126,
	RWStat,
};

/* bits in Qid.type */
enum {
	QTDIR	= 0x80,	/* type bit for directories */
	QTAPPEND	= 0x40,	/* type bit for append only files */
	QTEXCL	= 0x20,	/* type bit for exclusive use files */
	QTMOUNT	= 0x10,	/* type bit for mounted channel */
	QTAUTH	= 0x08,	/* type bit for authentication file */
	QTTMP	= 0x04,	/* type bit for non-backed-up file */
	QTSYMLINK	= 0x02,	/* type bit for symbolic link */
	QTFILE	= 0x00	/* type bits for plain file */
};

/* from libc.h in p9p */
enum {
	OREAD	= 0,	/* open for read */
	OWRITE	= 1,	/* write */
	ORDWR	= 2,	/* read and write */
	OEXEC	= 3,	/* execute, == read but check execute permission */
	OTRUNC	= 16,	/* or'ed in (except for exec), truncate file first */
	OCEXEC	= 32,	/* or'ed in, close on exec */
	ORCLOSE	= 64,	/* or'ed in, remove on close */
	ODIRECT	= 128,	/* or'ed in, direct access */
	ONONBLOCK	= 256,	/* or'ed in, non-blocking call */
	OEXCL	= 0x1000,	/* or'ed in, exclusive use (create only) */
	OLOCK	= 0x2000,	/* or'ed in, lock after opening */
	OAPPEND	= 0x4000	/* or'ed in, append only */
};

/* Larger than int, can't be enum */
#define DMDIR		0x80000000	/* mode bit for directories */
#define DMAPPEND	0x40000000	/* mode bit for append only files */
#define DMEXCL		0x20000000	/* mode bit for exclusive use files */
#define DMMOUNT		0x10000000	/* mode bit for mounted channel */
#define DMAUTH		0x08000000	/* mode bit for authentication file */
#define DMTMP		0x04000000	/* mode bit for non-backed-up file */

typedef struct {
	unsigned char	type;
	unsigned long	version;
	unsigned long	path;
} Qid;

typedef struct {
	unsigned int	type;
	unsigned long	dev;
	Qid	qid;
	unsigned long	mode;
	unsigned long	atime;
	unsigned long	mtime;
	unsigned long	length;
	char*	name;
	char*	uid;
	char*	gid;
	char*	muid;
} Stat;

typedef struct {
	unsigned char type;
	unsigned int tag;
	unsigned long fid;

	union {
		struct {/* Tversion, Rversion */
			unsigned long	msize;
			char	*version;
		};
		struct { /* Tflush */
			unsigned int	oldtag;
		};
		struct { /* Rerror */
			char	*ename;
		};
		struct { /* Ropen, Rcreate */
			Qid	qid; /* +Rattach */
			unsigned long	iounit;
		};
		struct { /* Rauth */
			Qid	aqid;
		};
		struct { /* Tauth, Tattach */
			unsigned long	afid;
			char	*uname;
			char	*aname;
		};
		struct { /* Tcreate */
			unsigned long	perm;
			char	*name;
			unsigned char	mode; /* +Topen */
		};
		struct { /* Twalk */
			unsigned long	newfid;
			unsigned int	nwname;
			char	*wname[MAX_WELEM];
		};
		struct { /* Rwalk */
			unsigned int	nwqid;
			Qid	wqid[MAX_WELEM];
		};
		struct {
			unsigned long	offset; /* Tread, Twrite */
			unsigned long	count; /* Tread, Twrite, Rread */
			char	*data; /* Twrite, Rread */
		};
		struct { /* Rstat */
			unsigned int	nstat;
			Stat	stat;
		};
		struct { /* Twstat */
			Stat	st;
		};
	};
} Fcall;

typedef struct {
	Fcall* (*version)(Fcall*);
//	Fcall* (*auth)(Fcall*);
	Fcall* (*attach)(Fcall*);
	Fcall* (*flush)(Fcall*);
	Fcall* (*walk)(Fcall*);
	Fcall* (*open)(Fcall*);
	Fcall* (*create)(Fcall*);
	Fcall* (*read)(Fcall*);
	Fcall* (*write)(Fcall*);
	Fcall* (*clunk)(Fcall*);
	Fcall* (*remove)(Fcall*);
	Fcall* (*stat)(Fcall*);
} Callbacks;

int putstat(unsigned char *buffer, int index, Stat *stat);
unsigned char* proc9p(unsigned char *msg, unsigned long *size, Callbacks *cb);

#endif