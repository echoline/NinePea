#ifndef NINEPEA_H
#define NINEPEA_H

#ifdef Plan9
#include <u.h>
#include <libc.h>
#define NULL nil
#else
#include <string.h>
#include <stdlib.h>
#endif

#define put2(buffer, index, value) \
	buffer[index++] = value & 0xFF; \
	buffer[index++] = (value >> 8) & 0xFF; \

#define put4(buffer, index, value) \
	put2(buffer, index, value); \
	put2(buffer, index, (unsigned long)value >> 16UL); \

#define put8(buffer, index, lvalue, hvalue) \
	put4(buffer, index, lvalue); \
	put4(buffer, index, hvalue); \

#define get2(buffer, index, value) \
	value = buffer[index++]; \
	value |= buffer[index++] << 8; \

#define get4(buffer, index, value) \
	get2(buffer, index, value); \
	value |= (unsigned long)buffer[index++] << 16UL; \
	value |= (unsigned long)buffer[index++] << 24UL; \

#define get8(buffer, index, lvalue, hvalue) \
	get4(buffer, index, lvalue); \
	get4(buffer, index, hvalue); \

// might have to change these depending on memory allocated
#define MAX_IO 4096	// blatant lie?
#define MAX_MSG MAX_IO+128
#define MAX_WELEM 16
#define MAX_PGMBUF 80
#define NOTAG ~0

/* 9P message types */
enum {
	TVersion = 'd',
	RVersion,
	TAuth = 'f',
	RAuth,
	TAttach = 'h',
	RAttach,
	TError = 'j', /* illegal */
	RError,
	TFlush = 'l',
	RFlush,
	TWalk = 'n',
	RWalk,
	TOpen = 'p',
	ROpen,
	TCreate = 'r',
	RCreate,
	TRead = 't',
	RRead,
	TWrite = 'v',
	RWrite,
	TClunk = 'x',
	RClunk,
	TRemove = 'z',
	RRemove,
	TStat = 124,
	RStat,
	TWStat = 126,
	RWStat,
};
#ifndef Plan9
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
#endif

typedef struct {
	unsigned int	type;
	unsigned long	dev;
	Qid	qid;
	unsigned long	mode;
	unsigned long	atime;
	unsigned long	mtime;
	unsigned long	length;
	char	*name;
	char	*uid;
	char	*gid;
	char	*muid;
} Stat;

typedef struct {
	unsigned char type;
	unsigned long tag;
	unsigned long fid;

	union {
		struct {/* Tversion, Rversion */
			unsigned long	msize;
//			char	*version;
		};
		struct { /* Tflush */
			unsigned int	oldtag;
		};
		struct { /* Rerror */
			char	*ename;
		};
		struct { /* Ropen, Rcreate */
			Qid	qid; /* +Rattach */
//			unsigned long	iounit;
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
//			char	*data; /* Twrite, Rread */
		};
		struct { /* Rstat */
			unsigned long	nstat;
			Stat	stat;
		};
		struct { /* Twstat */
			Stat	st;
		};
	};
} Fcall;

typedef struct {
//	Fcall* (*version)(Fcall*);
//	Fcall* (*auth)(Fcall*);
	Fcall* (*attach)(Fcall*);
	Fcall* (*flush)(Fcall*);
	Fcall* (*walk)(Fcall*);
	Fcall* (*open)(Fcall*);
	Fcall* (*create)(Fcall*);
	Fcall* (*read)(Fcall*, unsigned char*);
	Fcall* (*write)(Fcall*, unsigned char*);
	Fcall* (*clunk)(Fcall*);
	Fcall* (*remove)(Fcall*);
	Fcall* (*stat)(Fcall*);
	Fcall* (*wstat)(Fcall*);
} Callbacks;

unsigned long putstat(unsigned char *buffer, unsigned long index, Stat *stat);
unsigned long proc9p(unsigned char *msg, unsigned long size, Callbacks *cb);

/* fid mapping functions */

struct hentry {
	unsigned long id;
	unsigned long data;
	struct hentry *next;
	struct hentry *prev;
	void *aux;
};

struct htable {
	unsigned char length;
	struct hentry **data;
};

struct hentry* fs_fid_find(unsigned long id);
struct hentry* fs_fid_add(unsigned long id, unsigned long data);
void fs_fid_del(unsigned long id);
void fs_fid_init(int l);

extern char Etoobig[];
extern char Ebadtype[];
extern char Enofile[];
extern char Eperm[];

#endif
