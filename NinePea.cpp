#include "NinePea.h"

unsigned char str[MAX_MSG+1];

int
puthdr(unsigned char *buf, int index, unsigned char type, unsigned int tag, unsigned long size) {
	put4(buf, index, size);
	buf[index++] = type;
	put2(buf, index, tag);

	return index;
}

int
mkerr(unsigned char *buffer, unsigned int tag, char *errstr) {
	int index;
	int slen = strlen(errstr);
	int size = slen + 9;

	index = puthdr(buffer, 0, RError, tag, size);

	put2(buffer, index, slen);
	memcpy(&buffer[index], errstr, slen);

	return size;
}

int
putstat(unsigned char *buffer, int index, Stat *stat) {
	unsigned int namelen = strlen(stat->name);
	unsigned int uidlen = strlen(stat->uid);
	unsigned int gidlen = strlen(stat->gid);
	unsigned int muidlen = strlen(stat->muid);

	unsigned int size = 2 + 4 + (1 + 4 + 8) + 4 + 4 + 4 + 8 + (2 * 4) + namelen + uidlen + gidlen + muidlen;
	put2(buffer, index, size);

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
	free(stat->name);

	put2(buffer, index, uidlen);
	memcpy(&buffer[index], stat->uid, uidlen);
	index += uidlen;
	free(stat->uid);

	put2(buffer, index, gidlen);
	memcpy(&buffer[index], stat->gid, gidlen);
	index += gidlen;
	free(stat->gid);

	put2(buffer, index, muidlen);
	memcpy(&buffer[index], stat->muid, muidlen);
	index += muidlen;
	free(stat->muid);

	return size;
}

unsigned char*
proc9p(unsigned char *msg, unsigned long *size, Callbacks *cb) {
	Fcall *ifcall = (Fcall*)calloc(1, sizeof(Fcall));
	Fcall *ofcall = NULL;
	unsigned int slen;
	int index = 4, tmp;

	ifcall->type = msg[index++];
	get2(msg, index, ifcall->tag);

	if (*size > MAX_MSG) {
		index = mkerr(str, ifcall->tag, "message too big");
		goto END;
	}

	// if it isn't here, it isn't implemented
	switch(ifcall->type) {
	case TVersion:
		get4(msg, index, ifcall->msize);
		get2(msg, index, slen);

		ifcall->version = (char*)malloc(slen+1);
		ifcall->version[slen] = '\0';
		memcpy(ifcall->version, &msg[index], slen);

		if (ifcall->msize > MAX_MSG)
			ifcall->msize = MAX_MSG;

		ofcall = cb->version(ifcall);

		if (ofcall->type == RError) {
			index = mkerr(str, ifcall->tag, ofcall->ename);
			free(ofcall->ename);

			goto END;
		}

		index = 7;
		slen = strlen(ofcall->version);
		
		put4(str, index, ofcall->msize);
		put2(str, index, slen);
		memcpy(&str[index], ofcall->version, slen);
		index += slen;

		puthdr(str, 0, RVersion, ifcall->tag, index);

		free(ofcall->version);
		break;

	case TAttach:
		get4(msg, index, ifcall->fid);
		get4(msg, index, ifcall->afid);

		get2(msg, index, slen);
		ifcall->uname = (char*)malloc(slen+1);
		memcpy(ifcall->uname, &msg[index], slen);
		index += slen;

		get2(msg, index, slen);
		ifcall->aname = (char*)malloc(slen+1);
		memcpy(ifcall->aname, &msg[index], slen);
		index += slen;

		ofcall = cb->attach(ifcall);

		free(ifcall->uname);
		free(ifcall->aname);

		if (ofcall->type == RError) {
			index = mkerr(str, ifcall->tag, ofcall->ename);
			free(ofcall->ename);

			goto END;
		}

		index = 7;
		str[index++] = ofcall->qid.type;
		put4(str, index, ofcall->qid.version);
		put8(str, index, ofcall->qid.path, 0);
		puthdr(str, 0, RAttach, ifcall->tag, index);

		break;
	case TWalk:
		get4(msg, index, ifcall->fid);
		get4(msg, index, ifcall->newfid);
		get2(msg, index, ifcall->nwname);

		if (ifcall->nwname > MAX_WELEM)
			ifcall->nwname = MAX_WELEM;

		tmp = 0;
		while (tmp < ifcall->nwname) {
			get2(msg, index, slen);
			ifcall->wname[tmp] = (char*)malloc(slen+1);
			memcpy(ifcall->wname[tmp], &msg[index], slen);
			ifcall->wname[tmp][slen] = '\0';
			index += slen;
			tmp++;
		}

		ofcall = cb->walk(ifcall);
	
		for (index = 0; index < ifcall->nwname; index++)
			free(ifcall->wname[index]);

		if (ofcall->type == RError) {
			index = mkerr(str, ifcall->tag, ofcall->ename);
			free(ofcall->ename);

			goto END;
		}

		index = puthdr(str, 0, RWalk, ifcall->tag, 9 + ofcall->nwqid * 13);
		put2(str, index, ofcall->nwqid);

		tmp = 0;
		while (tmp < ofcall->nwqid) {
			str[index++] = ofcall->wqid[tmp].type;
			put4(str, index, ofcall->wqid[tmp].version);
			put8(str, index, ofcall->wqid[tmp].path, 0);

			tmp++;
		}

		break;
	case TStat:
		get4(msg, index, ifcall->fid);

		ofcall = cb->stat(ifcall);

		if (ofcall->type == RError) {
			index = mkerr(str, ifcall->tag, ofcall->ename);
			free(ofcall->ename);

			goto END;
		}

		slen = putstat(str, 9, &(ofcall->stat));
		slen += 2;
		index = puthdr(str, 0, RStat, ifcall->tag, slen + 9);
		put2(str, index, slen);	// bleh?
		index += slen;

		break;
	case TClunk:
		get4(msg, index, ifcall->fid);

		ofcall = cb->clunk(ifcall);

		if (ofcall->type == RError) {
			index = mkerr(str, ifcall->tag, ofcall->ename);
			free(ofcall->ename);

			goto END;
		}

		index = puthdr(str, 0, RClunk, ifcall->tag, 7);
		
		break;
	case TOpen:
		get4(msg, index, ifcall->fid);
		ifcall->mode = msg[index++];

		ofcall = cb->open(ifcall);

		if (ofcall->type == RError) {
			index = mkerr(str, ifcall->tag, ofcall->ename);
			free(ofcall->ename);

			goto END;
		}

		index = puthdr(str, 0, ROpen, ifcall->tag, 24);
		str[index++] = ofcall->qid.type;
		put4(str, index, ofcall->qid.version);
		put8(str, index, ofcall->qid.path, 0);
		put4(str, index, ofcall->iounit);

		break;
	case TRead:
		get4(msg, index, ifcall->fid);
		get4(msg, index, ifcall->offset);
		index += 4; // :(
		get4(msg, index, ifcall->count);

		ofcall = cb->read(ifcall);

		if (ofcall->type == RError) {
			index = mkerr(str, ifcall->tag, ofcall->ename);
			free(ofcall->ename);

			goto END;
		}

		index = puthdr(str, 0, RRead, ifcall->tag, 11 + ofcall->count);
		put4(str, index, ofcall->count);
		memcpy(&str[index], ofcall->data, ofcall->count);
		index += ofcall->count;
		free(ofcall->data);

		break;
	case TCreate:
		get4(msg, index, ifcall->fid);
		get2(msg, index, slen);
		ifcall->name = (char*)malloc(slen+1);
		ifcall->name[slen] = '\0';
		memcpy(ifcall->name, &msg[index], slen);
		index += slen;
		get4(msg, index, ifcall->perm);
		ifcall->mode = msg[index++];

		ofcall = cb->create(ifcall);

		free(ifcall->name);

		if (ofcall->type == RError) {
			index = mkerr(str, ifcall->tag, ofcall->ename);
			free(ofcall->ename);

			goto END;
		}

		index = puthdr(str, 0, RCreate, ifcall->tag, 24);
		str[index++] = ofcall->qid.type;
		put4(str, index, ofcall->qid.version);
		put8(str, index, ofcall->qid.path, 0);
		put4(str, index, ofcall->iounit);

		break;
	case TWrite:
		get4(msg, index, ifcall->fid);
		get4(msg, index, ifcall->offset);
		index += 4; // bleh... again
		get4(msg, index, ifcall->count);

		if (ifcall->count > (MAX_MSG - 23)) {
			index = mkerr(str, ifcall->tag, "message length overrun");
			goto END;
		}

		ifcall->data = (char*)malloc(ifcall->count+1);
		ifcall->data[ifcall->count] = '\0';
		memcpy(ifcall->data, &msg[index], ifcall->count);
		index += ifcall->count;

		ofcall = cb->write(ifcall);

		free(ifcall->data);

		if (ofcall->type == RError) {
			index = mkerr(str, ifcall->tag, ofcall->ename);
			free(ofcall->ename);

			goto END;
		}

		index = puthdr(str, 0, RWrite, ifcall->tag, 11);
		put4(str, index, ofcall->count);

		break;
	case TRemove:
		get4(msg, index, ifcall->fid);

		ofcall = cb->remove(ifcall);

		if (ofcall->type == RError) {
			index = mkerr(str, ifcall->tag, ofcall->ename);
			free(ofcall->ename);

			goto END;
		}

		index = puthdr(str, 0, RRemove, ifcall->tag, 7);
		
		break;
	case TFlush:
		get2(msg, index, ifcall->oldtag);

		ofcall = cb->flush(ifcall);

		if (ofcall->type == RError) {
			index = mkerr(str, ifcall->tag, ofcall->ename);
			free(ofcall->ename);

			goto END;
		}

		index = puthdr(str, 0, RFlush, ifcall->tag, 7);

		break;
	default:
		index = mkerr(str, ifcall->tag, "unknown message type");
		break;
	}

END:
	if (ofcall && (ofcall != ifcall))
		free(ofcall);
	free(ifcall);

	if (index > MAX_MSG)
		index = mkerr(str, ifcall->tag, "resulting message too big");

	*size = index;

	return str;
}
