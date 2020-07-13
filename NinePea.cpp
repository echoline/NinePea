#include "NinePea.h"

struct htable *fs_fids;

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

unsigned long
putstat(unsigned char *buffer, unsigned long index, Stat *stat) {
	unsigned int namelen = strlen(stat->name);
	unsigned int uidlen = strlen(stat->uid);
	unsigned int gidlen = strlen(stat->gid);
	unsigned int muidlen = strlen(stat->muid);

	unsigned int size = 2 + 4 + (1 + 4 + 8) + 4 + 4 + 4 + 8 + (2 * 4) + namelen + uidlen + gidlen + muidlen;
	put2(buffer, index, size);

	put2(buffer, index, stat->type);
	put4(buffer, index, stat->dev);
	buffer[index++] = stat->qid.type;
	put4(buffer, index, 0);
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

	return (size + 2);
}

unsigned long
getstat(unsigned char *buffer, unsigned long index, Stat *stat) {
	unsigned int size;
	unsigned long tmp;
	unsigned int namelen;
	unsigned int uidlen;
	unsigned int gidlen;
	unsigned int muidlen;

	get2(buffer, index, size);

	get2(buffer, index, stat->type);
	get4(buffer, index, stat->dev);
	stat->qid.type = buffer[index++];
	index += 4; //get4(buffer, index, 0);
	get8(buffer, index, stat->qid.path, tmp);
	get4(buffer, index, stat->mode);
	get4(buffer, index, stat->atime);
	get4(buffer, index, stat->mtime);
	get8(buffer, index, stat->length, tmp);

	get2(buffer, index, namelen);
	stat->name = (char*)malloc (sizeof (char) * (namelen + 1));
	stat->name[namelen] = '\0';
	memcpy(stat->name, &buffer[index], namelen);
	index += namelen;

	get2(buffer, index, uidlen);
	stat->uid = (char*)malloc (sizeof (char) * (uidlen + 1));
	stat->uid[uidlen] = '\0';
	memcpy(stat->uid, &buffer[index], uidlen);
	index += uidlen;

	get2(buffer, index, gidlen);
	stat->gid = (char*)malloc (sizeof (char) * (gidlen + 1));
	stat->gid[gidlen] = '\0';
	memcpy(stat->gid, &buffer[index], gidlen);
	index += gidlen;

	get2(buffer, index, muidlen);
	stat->muid = (char*)malloc (sizeof (char) * (muidlen + 1));
	stat->muid[muidlen] = '\0';
	memcpy(stat->muid, &buffer[index], muidlen);
	index += muidlen;

	return index;
}

char Etoobig[] = "TMI";
char Ebadtype[] = "9P?";
char Enofile[] = "file not found";
char Eperm[] = "permission denied";

unsigned long
proc9p(unsigned char *msg, unsigned long size, Callbacks *cb) {
	Fcall ifcall;
	Fcall *ofcall = NULL;
	unsigned long slen, tmp;
	unsigned long index;
	unsigned char i;

	msg[size] = '\0';
	index = 4;
	ifcall.type = msg[index++];
	get2(msg, index, ifcall.tag);

	if (size > MAX_MSG) {
		return mkerr(msg, ifcall.tag, Etoobig);
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
	case TAuth:
		index = mkerr(msg, ifcall.tag, (char*)"no auth");
		break;
	case TAttach:
		get4(msg, index, ifcall.fid);
		get4(msg, index, ifcall.afid);

		get2(msg, index, slen);
		index += slen;

		get2(msg, index, slen);
		msg[index-2] = '\0';
		ifcall.uname = strdup((char*)&msg[index-2-slen]);

		index += slen;
		msg[index] = '\0';
		ifcall.aname = strdup((char*)&msg[index-slen]);

		ofcall = cb->attach(&ifcall);

		free(ifcall.uname);
		free(ifcall.aname);

		if (ofcall->type == RError) {
			index = mkerr(msg, ifcall.tag, ofcall->ename);

			break;
		}

		index = 7;
		msg[index++] = ofcall->qid.type;
		put4(msg, index, 0); //ofcall->qid.version);
		put8(msg, index, ofcall->qid.path, 0);
		puthdr(msg, 0, RAttach, ifcall.tag, index);

		break;
	case TWalk:
		get4(msg, index, ifcall.fid);
		get4(msg, index, ifcall.newfid);
		get2(msg, index, ifcall.nwname);

		if (ifcall.nwname > MAX_WELEM)
			ifcall.nwname = MAX_WELEM;

		get2(msg, index, slen);
		for (i = 0; i < ifcall.nwname; i++) {
			index += slen;
			get2(msg, index, tmp);
			msg[index-2] = '\0';
			ifcall.wname[i] = strdup((char*)&msg[index-2-slen]);
			slen = tmp;
		}

		ofcall = cb->walk(&ifcall);

		for (i = 0; i < ifcall.nwname; i++)
			free(ifcall.wname[i]);

		if (ofcall->type == RError) {
			index = mkerr(msg, ifcall.tag, ofcall->ename);

			break;
		}

		index = puthdr(msg, 0, RWalk, ifcall.tag, 9 + ofcall->nwqid * 13);
		put2(msg, index, ofcall->nwqid);

		for (i = 0; i < ofcall->nwqid; i++) {
			msg[index++] = ofcall->wqid[i].type;
			index += 4; //put4(msg, index, ofcall->wqid[i].version);
			put8(msg, index, ofcall->wqid[i].path, 0);
		}

		break;
	case TStat:
		get4(msg, index, ifcall.fid);

		ofcall = cb->stat(&ifcall);

		if (ofcall->type == RError) {
			index = mkerr(msg, ifcall.tag, ofcall->ename);

			break;
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

			break;
		}

		index = puthdr(msg, 0, RClunk, ifcall.tag, 7);

		break;
	case TOpen:
		get4(msg, index, ifcall.fid);
		ifcall.mode = msg[index++];

		ofcall = cb->open(&ifcall);

		if (ofcall->type == RError) {
			index = mkerr(msg, ifcall.tag, ofcall->ename);

			break;
		}

		index = puthdr(msg, 0, ROpen, ifcall.tag, 24);
		msg[index++] = ofcall->qid.type;
		index += 4; //put4(msg, index, ofcall->qid.version);
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

			break;
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

			break;
		}

		index = puthdr(msg, 0, RCreate, ifcall.tag, 24);
		msg[index++] = ofcall->qid.type;
		index += 4; //put4(msg, index, ofcall->qid.version);
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

			break;
		}

		index = puthdr(msg, 0, RWrite, ifcall.tag, 11);
		put4(msg, index, ofcall->count);

		break;
	case TRemove:
		get4(msg, index, ifcall.fid);

		ofcall = cb->remove(&ifcall);

		if (ofcall->type == RError) {
			index = mkerr(msg, ifcall.tag, ofcall->ename);

			break;
		}

		index = puthdr(msg, 0, RRemove, ifcall.tag, 7);
		
		break;
	case TFlush:
		get2(msg, index, ifcall.oldtag);

		ofcall = cb->flush(&ifcall);

		if (ofcall->type == RError) {
			index = mkerr(msg, ifcall.tag, ofcall->ename);

			break;
		}

		index = puthdr(msg, 0, RFlush, ifcall.tag, 7);

		break;
	case TWStat:
		get4(msg, index, ifcall.fid);
		get2(msg, index, slen);
		index = getstat(msg, index, &ifcall.st);

		ofcall = cb->wstat(&ifcall);

		free (ifcall.st.name);
		free (ifcall.st.uid);
		free (ifcall.st.gid);
		free (ifcall.st.muid);
		
		if (ofcall->type == RError) {
			index = mkerr(msg, ifcall.tag, ofcall->ename);

			break;
		}

		index = puthdr(msg, 0, RWStat, ifcall.tag, 7);
		break;
	default:
		index = mkerr(msg, ifcall.tag, Ebadtype);
		break;
	}

	if (index > MAX_MSG) {
		index = mkerr(msg, ifcall.tag, Etoobig);
	}

	return index;
}

/* fid mapping functions */

unsigned long
hashf(struct htable *tbl, unsigned long id) {
	return id % tbl->length;
}

struct hentry*
fs_fid_find(unsigned long id) {
	struct hentry **cur;

	for (cur = &(fs_fids->data[hashf(fs_fids, id)]);
				*cur != NULL; cur = &(*cur)->next) {
			if ((*cur)->id == id)
				break;
	}

	return *cur;
}

struct hentry*
fs_fid_add(unsigned long id, unsigned long data) {
	struct hentry *cur = fs_fid_find(id);
	unsigned char h;

	if (cur == NULL) {
		cur = (struct hentry*)calloc(1, sizeof(*cur));
		cur->id = id;
		h = hashf(fs_fids, id);
		
		if (fs_fids->data[h]) {
			cur->next = fs_fids->data[h];
			cur->next->prev = cur;
		}
		fs_fids->data[h] = cur;
	}

	cur->data = data;

	return cur;
}

void
fs_fid_del(unsigned long id) {
	unsigned char h = hashf(fs_fids, id);
	struct hentry *cur = fs_fids->data[h];
	
	if (cur->id == id)
		fs_fids->data[h] = cur->next;
	else {
		cur = cur->next;
		while (cur) {
			if (cur->id == id)
				break;

			cur = cur->next;
		}
	}

	if (cur == NULL) {
		return;
	}

	if (cur->prev)
		cur->prev->next = cur->next;
	if (cur->next)
		cur->next->prev = cur->prev;

	free(cur);
}

void
fs_fid_init(int l) {
	fs_fids = (struct htable*)malloc(sizeof(struct htable));
	fs_fids->length = l;
	fs_fids->data = (struct hentry**)calloc(fs_fids->length, sizeof(struct hentry*));
}
