#include <NinePea.h>

Fcall ofcall;
char errstr[64];
char Snone[] = "a";
char Sroot[] = "/";
char Sdigital[] = "dXX";

#define Qroot 0

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
  int i, pin;
  char *ep;

  if (!ent) {
    ofcall.type = RError;
    ofcall.ename = Enofile;

    return &ofcall;
  }

  path = ent->data;

  for (i = 0; i < ifcall->nwname; i++) {
    switch(ent->data) {
    case Qroot:
      if (ifcall->wname[i][0] == 'd') {
        if (strlen(ifcall->wname[i]) != 3)
          goto WALKERROR;
        ep = ifcall->wname[i] + 3;
        pin = strtod(ifcall->wname[i] + 1, &ep);
        if (pin >= 0 && pin < NUM_DIGITAL_PINS) {
          ofcall.wqid[i].type = QTFILE;
          ofcall.wqid[i].version = 0;
          ofcall.wqid[i].path = path = pin + 1;
        }
        else {
          goto WALKERROR;
        }
      }
      else if (!strcmp(ifcall->wname[i], ".")) {
        ofcall.wqid[i].type = QTDIR;
        ofcall.wqid[i].version = 0;
        ofcall.wqid[i].path = path = Qroot;
      } 
      else {
WALKERROR:
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
  ofcall.stat.mode = 0666 | DMTMP;
  ofcall.stat.atime = ofcall.stat.mtime = ofcall.stat.length = 0;
  ofcall.stat.uid = Snone;
  ofcall.stat.gid = Snone;
  ofcall.stat.muid = Snone;

  if (ent->data == Qroot) {
    ofcall.stat.qid.type |= QTDIR;
    ofcall.stat.qid.path = Qroot;
    ofcall.stat.mode |= 0111 | DMDIR;
    ofcall.stat.name = Sroot;
  }
  else if (ent->data > 0 && ent->data <= NUM_DIGITAL_PINS) {
    ofcall.stat.qid.path = ent->data;
    ofcall.stat.name = Sdigital;
    sprintf(Sdigital, "d%02d", ent->data - 1);
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

Fcall*
fs_read(Fcall *ifcall, unsigned char *out) {
  struct hentry *cur = fs_fid_find(ifcall->fid);
  Stat stat;
  char tmpstr[32];
  unsigned char i;
  unsigned long value;

  ofcall.count = 0;

  if (cur == NULL) {
    ofcall.type = RError;
    ofcall.ename = Enofile;
  }
  // offset?  too hard, sorry
  else if (ifcall->offset != 0) {
    out[0] = '\0';
  }
  else if (((unsigned long)cur->data) == Qroot) {
    stat.type = 0;
    stat.dev = 0;
    stat.qid.type = QTFILE;
    stat.mode = 0666;
    stat.atime = 0;
    stat.mtime = 0;
    stat.length = 0;

    for (i = 1; i <= NUM_DIGITAL_PINS; i++) {
      stat.qid.path = i;
      stat.name = Sdigital;
      sprintf(Sdigital, "d%02d", i - 1);
      stat.uid = Snone;
      stat.gid = Snone;
      stat.muid = Snone;
      ofcall.count += putstat(out, ofcall.count, &stat);
    }
  }
  else if (((unsigned long)cur->data) > 0 && ((unsigned long)cur->data) <= NUM_DIGITAL_PINS) {
    i = (unsigned long)cur->data;
    pinMode(i - 1, INPUT);
    sprintf((char*)out, "%01d\n", digitalRead(i - 1));
    ofcall.count = strlen((const char*)out);
  }
  else {
    ofcall.type = RError;
    ofcall.ename = Enofile;
  }

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
  struct hentry *cur = fs_fid_find(ifcall->fid);
  int i, j;

  ofcall.count = ifcall->count;

  if (cur == NULL) {
    ofcall.type = RError;
    ofcall.ename = Enofile;
  }
  else if (((unsigned long)cur->data) == Qroot) {
    ofcall.type = RError;
    ofcall.ename = Eperm;
  }
  else if (((unsigned long)cur->data) > 0 && ((unsigned long)cur->data) <= NUM_DIGITAL_PINS) {
    i = ((unsigned long)cur->data) - 1;
    if (strcmp((const char*)in, "1\n") == 0)
      j = HIGH;
    else if (strcmp((const char*)in, "0\n") == 0)
      j = LOW;
    else
      goto WRITEERROR;
    pinMode(i, OUTPUT);
    digitalWrite(i, j);
  }
  else {
WRITEERROR:
    ofcall.type = RError;
    ofcall.ename = Eperm;
  }

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
