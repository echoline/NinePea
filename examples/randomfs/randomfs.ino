#include <NinePea.h>

Fcall ofcall;
char errstr[64];
char Snone[] = "arduino";
char Sroot[] = "/";
char Srandom[] = "random";

/* paths */

enum {
  Qroot = 0,
  Qrandom,
  QNUM,
};

/* 9p handlers */

Fcall*
fs_attach(Fcall *ifcall) {
  ofcall.qid.type = QTDIR | QTTMP;
  ofcall.qid.version = 0;
  ofcall.qid.path = Qroot;

  fs_fid_add(ifcall->fid, Qroot);

  return &ofcall;
}

Fcall*
fs_walk(Fcall *ifcall) {
  unsigned long path;
  struct hentry *ent = fs_fid_find(ifcall->fid);
  int i;

  if (!ent) {
    ofcall.type = RError;
    ofcall.ename = Enofile;

    return &ofcall;
  }

  path = ent->data;

  for (i = 0; i < ifcall->nwname; i++) {
    switch(ent->data) {
    case Qroot:
      if (!strcmp(ifcall->wname[i], "random")) {
        ofcall.wqid[i].type = QTFILE;
        ofcall.wqid[i].version = 0;
        ofcall.wqid[i].path = path = Qrandom;
      } 
      else if (!strcmp(ifcall->wname[i], ".")) {
        ofcall.wqid[i].type = QTDIR;
        ofcall.wqid[i].version = 0;
        ofcall.wqid[i].path = path = Qroot;
      }
      else {
        ofcall.type = RError;
        ofcall.ename = Enofile;
        return &ofcall;
      }
      break;
    default:
      ofcall.type = RError;
      ofcall.ename = Enofile;

      return &ofcall;
      break;
    }
  }

  ofcall.nwqid = i;

  if (fs_fid_find(ifcall->newfid) != NULL) {
    ofcall.type = RError;
    strcpy(errstr, "new fid exists");
    ofcall.ename = errstr;
    return &ofcall;
  }

  fs_fid_add(ifcall->newfid, path);

  return &ofcall;
}

Fcall*
fs_stat(Fcall *ifcall) {
  struct hentry *ent;

  if ((ent = fs_fid_find(ifcall->fid)) == NULL) {
    ofcall.type = RError;
    ofcall.ename = Enofile;

    return &ofcall;
  }

  ofcall.stat.qid.type = QTTMP;
  ofcall.stat.mode = 0444 | DMTMP;
  ofcall.stat.atime = ofcall.stat.mtime = ofcall.stat.length = 0;
  ofcall.stat.uid = Snone;
  ofcall.stat.gid = Snone;
  ofcall.stat.muid = Snone;

  switch (ent->data) {
  case Qroot:
    ofcall.stat.qid.type |= QTDIR;
    ofcall.stat.qid.path = Qroot;
    ofcall.stat.mode |= 0111 | DMDIR;
    ofcall.stat.name = Sroot;
    break;
  case Qrandom:
    ofcall.stat.qid.path = Qrandom;
    ofcall.stat.name = Srandom;
    break;
  }

  return &ofcall;
}

Fcall*
fs_clunk(Fcall *ifcall) {
  fs_fid_del(ifcall->fid);

  return ifcall;
}

Fcall*
fs_open(Fcall *ifcall) {
  struct hentry *cur = fs_fid_find(ifcall->fid);

  if (cur == NULL) {
    ofcall.type = RError;
    ofcall.ename = Enofile;

    return &ofcall;
  }

  ofcall.qid.type = QTFILE;
  ofcall.qid.path = cur->data;

  if (cur->data == Qroot)
    ofcall.qid.type = QTDIR;

  return &ofcall;
}

#define ROTATE(v,c) ((unsigned long)((v) << (c)) | ((v) >> (32 - (c))))

#define QUARTERROUND(ia,ib,ic,id) { \
  unsigned long a, b, c, d, t; \
  a = x[ia]; b = x[ib]; c = x[ic]; d = x[id]; \
  a += b; t = d^a; d = ROTATE(t,16); \
  c += d; t = b^c; b = ROTATE(t,12); \
  a += b; t = d^a; d = ROTATE(t, 8); \
  c += d; t = b^c; b = ROTATE(t, 7); \
  x[ia] = a; x[ib] = b; x[ic] = c; x[id] = d; \
}

void
_chachablock(unsigned long x[16], int rounds)
{
  for(; rounds > 0; rounds -= 2) {
    QUARTERROUND(0, 4, 8,12)
    QUARTERROUND(1, 5, 9,13)
    QUARTERROUND(2, 6,10,14)
    QUARTERROUND(3, 7,11,15)

    QUARTERROUND(0, 5,10,15)
    QUARTERROUND(1, 6,11,12)
    QUARTERROUND(2, 7, 8,13)
    QUARTERROUND(3, 4, 9,14)
  }
}

Fcall*
fs_read(Fcall *ifcall, unsigned char *out) {
  struct hentry *cur = fs_fid_find(ifcall->fid);
  Stat stat;
  unsigned long i, j, k;
  unsigned long x[16];

  if (cur == NULL) {
    ofcall.type = RError;
    ofcall.ename = Enofile;
  }
  else if (((unsigned long)cur->data) == Qroot) {
    if (ifcall->offset != 0) {
      out[0] = '\0';
      ofcall.count = 0;
      goto ENDREAD;
    }
    stat.type = 0;
    stat.dev = 0;
    stat.qid.type = QTFILE;
    stat.mode = 0666;
    stat.atime = 0;
    stat.mtime = 0;
    stat.length = 0;

    stat.qid.path = Qrandom;
    stat.name = Srandom;
    stat.uid = Snone;
    stat.gid = Snone;
    stat.muid = Snone;
    ofcall.count = putstat(out, 0, &stat);
  }
  else if (((unsigned long)cur->data) == Qrandom) {
    for (i = 0; i < 16; i++) {
      x[i] = 0;
      for (j = A0; j < (NUM_ANALOG_INPUTS + A0); j++) {
        x[i] <<= 1;
        x[i] ^= analogRead(j) ^ micros();
      }
    }
    for (k = 0; k < ifcall->count; k++) {
      i = k & 15;
      if (i == 0)
        _chachablock(x, 20);
      out[k] = x[i] & 0xFF;
    }
    ofcall.count = ifcall->count;
  }
  else {
    ofcall.type = RError;
    ofcall.ename = Enofile;
  }
ENDREAD:

  return &ofcall;
}

Fcall*
fs_create(Fcall *ifcall) {
  ofcall.type = RError;
  ofcall.ename = Eperm;

  return &ofcall;
}

Fcall*
fs_write(Fcall *ifcall, unsigned char *in) {
  ofcall.type = RError;
  ofcall.ename = Eperm;

  return &ofcall;
}

Fcall*
fs_remove(Fcall *ifcall) {
  ofcall.type = RError;
  ofcall.ename = Eperm;

  return &ofcall;
}

Fcall*
fs_flush(Fcall *ifcall) {
  return ifcall;
}

Fcall*
fs_wstat(Fcall *ifcall) {
  ofcall.type = RError;
  ofcall.ename = Eperm;

  return &ofcall;
}

Callbacks callbacks;

void
sysfatal(int code)
{
  pinMode(13, OUTPUT);	

  while(true) {
    digitalWrite(13, HIGH);
    delay(1000);
    digitalWrite(13, LOW);
    delay(code * 1000);
  }
}

void
setup()
{
  int j;

  for (j = A0; j < (NUM_ANALOG_INPUTS+A0); j++)
    pinMode(j, INPUT);
  
  Serial.begin(115200);

  fs_fid_init(64);

  // this is REQUIRED by proc9p (see below)
  callbacks.attach = fs_attach;
  callbacks.flush = fs_flush;
  callbacks.walk = fs_walk;
  callbacks.open = fs_open;
  callbacks.create = fs_create;
  callbacks.read = fs_read;
  callbacks.write = fs_write;
  callbacks.clunk = fs_clunk;
  callbacks.remove = fs_remove;
  callbacks.stat = fs_stat;
  callbacks.wstat = fs_wstat;
}

unsigned char msg[MAX_MSG+1];
unsigned long msglen = 0;
unsigned long r = 0;

void
loop()
{
  unsigned long i;

  while (r < 5) {
    while (Serial.available() < 1);
    msg[r++] = Serial.read();
  }

  i = 0;
  get4(msg, i, msglen);

  // sanity check
  if (msg[i] & 1 || msglen > MAX_MSG || msg[i] < TVersion || msg[i] > TWStat) {
    sysfatal(3);
  }

  while (r < msglen) {
    while (Serial.available() < 1);
    msg[r++] = Serial.read();
  }

  memset(&ofcall, 0, sizeof(ofcall));

  // proc9p accepts valid 9P msgs of length msglen,
  // processes them using callbacks->various(functions);
  // returns variable out's msglen
  msglen = proc9p(msg, msglen, &callbacks);

  Serial.write(msg, msglen);

  r = msglen = 0;
}
