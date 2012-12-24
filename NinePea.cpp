#include "NinePea.h"
#include <avr/pgmspace.h>

char pgmbuf[MAX_PGMBUF];

int
puthdr(unsigned char *buf, unsigned long index, unsigned char type, unsigned int tag, unsigned long size) {
	put4(buf, index, size);
	buf[index++] = type;
	put2(buf, index, tag);

	return index;
}

int
mkerr(unsigned char *buffer, unsigned char tag, char *errstr) {
	int index;
	int slen = strlen(errstr);
	int size = slen + 9;

	index = puthdr(buffer, 0, RError, tag, size);

	put2(buffer, index, slen);
	memcpy(&buffer[index], errstr, slen);

	return size;
}

int
putstat(unsigned char *buffer, unsigned long index, Stat *stat) {
	unsigned int namelen = strlen(stat->name);
	unsigned int uidlen = strlen(stat->uid);
	unsigned int gidlen = strlen(stat->gid);
	unsigned int muidlen = strlen(stat->muid);

	unsigned int size = 2 + 4 + (1 + 4 + 8) + 4 + 4 + 4 + 8 + (2 * 4) + namelen + uidlen + gidlen + muidlen;
//	put2(buffer, index, size);

	put2(buffer, index, stat->type);
	put4(buffer, index, stat->dev);
	buffer[index++] = stat->qid.type;
	put4(buffer, index, stat->qid.version);
	put8(buffer, index, stat->qid.path, 0);
	put4(buffer, index, stat->mode);
	put4(buffer, index, stat->atime);
	put4(buffer, index, stat->mtime);
	put8(buffer, index, stat->length, 0);

	put2(buffer, index, namelen);
	memcpy(&buffer[index], stat->name, namelen);
	index += namelen;

	put2(buffer, index, uidlen);
	memcpy(&buffer[index], stat->uid, uidlen);
	index += uidlen;

	put2(buffer, index, gidlen);
	memcpy(&buffer[index], stat->gid, gidlen);
	index += gidlen;

	put2(buffer, index, muidlen);
	memcpy(&buffer[index], stat->muid, muidlen);
	index += muidlen;

	return (size);
}

prog_char Etoobig[] PROGMEM = "message too big";
prog_char Ebadtype[] PROGMEM = "unknown message type";

unsigned long
proc9p(unsigned char *msg, unsigned long size, Callbacks *cb) {
	Fcall ifcall;
	Fcall *ofcall = NULL;
	unsigned long slen;
	unsigned long index;
	unsigned char i;

	index = 4;
	ifcall.type = msg[index++];
	get2(msg, index, ifcall.tag);

	if (size > MAX_MSG) {
		strcpy_P(pgmbuf, Etoobig);
		index = mkerr(msg, ifcall.tag, pgmbuf);
		goto END;
	}

	// if it isn't here, it isn't implemented
	switch(ifcall.type) {
	case TVersion:
		i = index;
		index = 7;

		get4(msg, i, ifcall.msize);
		get2(msg, i, slen);

		if (ifcall.msize > MAX_MSG)
			ifcall.msize = MAX_MSG;

		put4(msg, index, ifcall.msize);
		put2(msg, index, slen);

		index += slen;
		puthdr(msg, 0, RVersion, ifcall.tag, index);

		break;
	case TAttach:
		get4(msg, index, ifcall.fid);
		get4(msg, index, ifcall.afid);

		get2(msg, index, slen);
		ifcall.uname = (char*)&msg[index];
		index += slen;

		get2(msg, index, slen);
		msg[index-2] = '\0';

		ifcall.aname = (char*)&msg[index];
		index += slen;
		msg[index-2] = '\0';

		ofcall = cb->attach(&ifcall);

		if (ofcall->type == RError) {
			index = mkerr(msg, ifcall.tag, ofcall->ename);

			goto END;
		}

		index = 7;
		msg[index++] = ofcall->qid.type;
		put4(msg, index, ofcall->qid.version);
		put8(msg, index, ofcall->qid.path, 0);
		puthdr(msg, 0, RAttach, ifcall.tag, index);

		break;
	case TWalk:
		get4(msg, index, ifcall.fid);
		get4(msg, index, ifcall.newfid);
		get2(msg, index, ifcall.nwname);

		if (ifcall.nwname > MAX_WELEM)
			ifcall.nwname = MAX_WELEM;

		for (i = 0; i < ifcall.nwname; i++) {
			get2(msg, index, slen);
			msg[index-2] = '\0';
			ifcall.wname[i] = (char*)&msg[index];
			index += slen;
		}
		msg[index] = '\0';

		ofcall = cb->walk(&ifcall);

		if (ofcall->type == RError) {
			index = mkerr(msg, ifcall.tag, ofcall->ename);

			goto END;
		}

		index = puthdr(msg, 0, RWalk, ifcall.tag, 9 + ofcall->nwqid * 13);
		put2(msg, index, ofcall->nwqid);

		for (i = 0; i < ofcall->nwqid; i++) {
			msg[index++] = ofcall->wqid[i].type;
			put4(msg, index, ofcall->wqid[i].version);
			put8(msg, index, ofcall->wqid[i].path, 0);
		}

		break;
	case TStat:
		get4(msg, index, ifcall.fid);

		ofcall = cb->stat(&ifcall);

		if (ofcall->type == RError) {
			index = mkerr(msg, ifcall.tag, ofcall->ename);

			goto END;
		}

		slen = putstat(msg, 9, &(ofcall->stat));
		index = puthdr(msg, 0, RStat, ifcall.tag, slen + 9);
		put2(msg, index, slen);	// bleh?
		index += slen;

		break;
	case TClunk:
		get4(msg, index, ifcall.fid);

		ofcall = cb->clunk(&ifcall);

		if (ofcall->type == RError) {
			index = mkerr(msg, ifcall.tag, ofcall->ename);

			goto END;
		}

		index = puthdr(msg, 0, RClunk, ifcall.tag, 7);

		break;
	case TOpen:
		get4(msg, index, ifcall.fid);
		ifcall.mode = msg[index++];

		ofcall = cb->open(&ifcall);

		if (ofcall->type == RError) {
			index = mkerr(msg, ifcall.tag, ofcall->ename);

			goto END;
		}

		index = puthdr(msg, 0, ROpen, ifcall.tag, 24);
		msg[index++] = ofcall->qid.type;
		put4(msg, index, ofcall->qid.version);
		put8(msg, index, ofcall->qid.path, 0);
		put4(msg, index, MAX_IO);

		break;
	case TRead:
		get4(msg, index, ifcall.fid);
		get4(msg, index, ifcall.offset);
		index += 4; // :(
		get4(msg, index, ifcall.count);

		ofcall = cb->read(&ifcall, &msg[11]);

		if (ofcall->type == RError) {
			index = mkerr(msg, ifcall.tag, ofcall->ename);

			goto END;
		}

		// No response
		if (ofcall == NULL) {
			index = 0;

			goto END;
		}

		index = puthdr(msg, 0, RRead, ifcall.tag, 11 + ofcall->count);
		put4(msg, index, ofcall->count);
		index += ofcall->count;

		break;
	case TCreate:
		get4(msg, index, ifcall.fid);
		get2(msg, index, slen);
		ifcall.name = (char*)&msg[index];
		index += slen;
		get4(msg, index, ifcall.perm);
		msg[index-4] = '\0';
		ifcall.mode = msg[index++];

		ofcall = cb->create(&ifcall);

		if (ofcall->type == RError) {
			index = mkerr(msg, ifcall.tag, ofcall->ename);

			goto END;
		}

		index = puthdr(msg, 0, RCreate, ifcall.tag, 24);
		msg[index++] = ofcall->qid.type;
		put4(msg, index, ofcall->qid.version);
		put8(msg, index, ofcall->qid.path, 0);
		put4(msg, index, MAX_IO);

		break;
	case TWrite:
		get4(msg, index, ifcall.fid);
		get4(msg, index, ifcall.offset);
		index += 4; // bleh... again
		get4(msg, index, ifcall.count);

		ofcall = cb->write(&ifcall, &msg[index]);

		if (ofcall->type == RError) {
			index = mkerr(msg, ifcall.tag, ofcall->ename);

			goto END;
		}

		index = puthdr(msg, 0, RWrite, ifcall.tag, 11);
		put4(msg, index, ofcall->count);

		break;
	case TRemove:
		get4(msg, index, ifcall.fid);

		ofcall = cb->remove(&ifcall);

		if (ofcall->type == RError) {
			index = mkerr(msg, ifcall.tag, ofcall->ename);

			goto END;
		}

		index = puthdr(msg, 0, RRemove, ifcall.tag, 7);
		
		break;
	case TFlush:
		get2(msg, index, ifcall.oldtag);

		ofcall = cb->flush(&ifcall);

		if (ofcall->type == RError) {
			index = mkerr(msg, ifcall.tag, ofcall->ename);

			goto END;
		}

		index = puthdr(msg, 0, RFlush, ifcall.tag, 7);

		break;
	default:
		strcpy_P(pgmbuf, Ebadtype);
		index = mkerr(msg, ifcall.tag, pgmbuf);
		break;
	}

END:
	if (index > MAX_MSG) {
		strcpy_P(pgmbuf, Etoobig);
		index = mkerr(msg, ifcall.tag, pgmbuf);
	}

	return index;
}
