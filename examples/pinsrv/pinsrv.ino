#include <NinePea.h>

Fcall ofcall;
char errstr[64];
char Snone[] = "none";
char Sroot[] = "/";
char Sctl[] = "arduinoctl";
char Sdata[] = "arduino";

/* paths */

enum {
  Qroot = 0,
  Qctl,
  Qdata,
  QNUM,
};

/* 9p handlers */

Fcall*
fs_attach(Fcall *ifcall) {
  ofcall.qid.type = QTDIR | QTTMP;
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
    strcpy(errstr, "file not found");
    ofcall.ename = errstr;

    return &ofcall;
  }

  path = ent->data;

  for (i = 0; i < ifcall->nwname; i++) {
    switch(ent->data) {
    case Qroot:
      if (!strcmp(ifcall->wname[i], "arduinoctl")) {
        ofcall.wqid[i].type = QTTMP;
        ofcall.wqid[i].path = path = Qctl;
      } 
      else if (!strcmp(ifcall->wname[i], "arduino")) {
        ofcall.wqid[i].type = QTTMP;
        ofcall.wqid[i].path = path = Qdata;
      } 
      else if (!strcmp(ifcall->wname[i], ".")) {
        ofcall.wqid[i].type = QTTMP | QTDIR;
        ofcall.wqid[i].path = path = Qroot;
      } 
      else {
        ofcall.type = RError;
        strcpy(errstr, "file not found");
        ofcall.ename = errstr;

        return &ofcall;
      }
      break;
    case Qdata:
    case Qctl:
      if (!strcmp(ifcall->wname[i], "..")) {
        ofcall.wqid[i].type = QTTMP | QTDIR;
        ofcall.wqid[i].path = path = Qroot;
      } 
      else {
        ofcall.type = RError;
        strcpy(errstr, "file not found");
        ofcall.ename = errstr;

        return &ofcall;
      }
      break;
    }
  }

  ofcall.nwqid = i;

  fs_fid_add(ifcall->newfid, path);

  return &ofcall;
}

Fcall*
fs_stat(Fcall *ifcall) {
  struct hentry *ent;

  if ((ent = fs_fid_find(ifcall->fid)) == NULL) {
    ofcall.type = RError;
    strcpy(errstr, "file not found");
    ofcall.ename = errstr;

    return &ofcall;
  }

  ofcall.stat.qid.type = QTTMP;
  ofcall.stat.mode = 0666 | DMTMP;
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
  case Qctl:
    ofcall.stat.qid.path = Qctl;
    ofcall.stat.name = Sctl;
    break;
  case Qdata:
    ofcall.stat.qid.path = Qdata;
    ofcall.stat.name = Sdata;
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

  if (!cur) {
    ofcall.type = RError;
    strcpy(errstr, "file not found");
    ofcall.ename = errstr;

    return &ofcall;
  }

  ofcall.qid.type = QTTMP;
  ofcall.qid.path = cur->data;

  if (cur->data == Qroot)
    ofcall.qid.type |= QTDIR;

  return &ofcall;
}

Fcall*
fs_read(Fcall *ifcall, unsigned char *out) {
  struct hentry *cur = fs_fid_find(ifcall->fid);
  Stat stat;
  char tmpstr[32];
  unsigned char i;
  unsigned long value;

  if (cur == NULL) {
    ofcall.type = RError;
    strcpy(errstr, "invalid fid");
    ofcall.ename = errstr;
  }
  else if (((unsigned long)cur->data) == Qdata) {
    snprintf((char*)out, MAX_IO - 1, "digital pins:\n");

    for (i = 2; i < 54; i++) {
      if (digitalRead(i))
        snprintf(tmpstr, sizeof(tmpstr), "\t%d:\tHIGH\n", i);
      else
        snprintf(tmpstr, sizeof(tmpstr), "\t%d:\t LOW\n", i);

      strlcat((char*)out, tmpstr, MAX_IO);
    }

    snprintf(tmpstr, sizeof(tmpstr), "analog pins:\n");
    strlcat((char*)out, tmpstr, MAX_IO);

    for (i = A0; i <= A15; i++) {
      value = analogRead(i - A0);
      snprintf(tmpstr, sizeof(tmpstr), "\t%d:\t%04d\n", i, value);
      strlcat((char*)out, tmpstr, MAX_IO);
    }

    ofcall.count = strlen((char*)out) - ifcall->offset;
    if (ofcall.count > ifcall->count)
      ofcall.count = ifcall->count;
    if (ifcall->offset != 0)
      memmove(out, &out[ifcall->offset], ofcall.count);
  } 
  // offset?  too hard, sorry
  else if (ifcall->offset != 0) {
    out[0] = '\0';
    ofcall.count = 0;
  }
  else if (((unsigned long)cur->data) == Qroot) {
    stat.type = 0;
    stat.dev = 0;
    stat.qid.type = QTTMP;
    stat.qid.path = Qdata;
    stat.mode = 0666 | DMTMP;
    stat.atime = 0;
    stat.mtime = 0;
    stat.length = 0;
    stat.name = Sdata;
    stat.uid = Snone;
    stat.gid = Snone;
    stat.muid = Snone;
    ofcall.count = putstat(out, 0, &stat);

    stat.qid.path = Qctl;
    stat.name = Sctl;
    stat.uid = Snone;
    stat.gid = Snone;
    stat.muid = Snone;
    
    ofcall.count += putstat(out, ofcall.count, &stat);
  }
  else if (((unsigned long)cur->data) == Qctl) {
    ofcall.type = RError;
    strcpy(errstr, "ctl file read does nothing...");
    ofcall.ename = errstr;
  }
  else {
    sprintf(errstr, "unknown path: %x\n", (unsigned int)cur->data);
    ofcall.type = RError;
    ofcall.ename = errstr;
  }

  return &ofcall;
}

Fcall*
fs_create(Fcall *ifcall) {
  ofcall.type = RError;
  strcpy(errstr, "create not allowed");
  ofcall.ename = errstr;

  return &ofcall;
}

/* interlude...? */

void
readpinvalue(char *line, unsigned char *pin, unsigned char *value) {
  char *p = line;
  char *q = line;

  *pin = *value = -1;

  while (isdigit(*p) || isspace(*p))
    p++;

  if (*p == '=')
    *p = '\0';
  else
    return;

  p++;
  q = p;

  while (isdigit(*p) || isspace(*p))
    p++;

  if (*p != '\0')
    return;

  *pin = atoi(line);
  *value = atoi(q);
}

void
setpindirs(char *in, Fcall *ofcall) {
  char *line = strtok(in, "\n");
  unsigned char pin, val;

  while (line != NULL) {
    readpinvalue(line, &pin, &val);

    if (pin < 2 || pin > 69 || val < 0 || val > 1) {
      ofcall->type = RError;
      strcpy(errstr, "format is [2,69]=[0,1]");
      ofcall->ename = errstr;

      return;
    }

    pinMode(pin, val);

    line = strtok(NULL, "\n");
  }
}

void
setpinvals(char *in, Fcall *ofcall) {
  char *line = strtok(in, "\n");
  unsigned char pin, val;

  while (line != NULL) {
    readpinvalue(line, &pin, &val);

    if (pin < 2 || pin > 69 || val < 0 || val > 255) {
      ofcall->type = RError;
      strcpy(errstr, "format is [2,69]=[0,255]");
      ofcall->ename = errstr;

      return;
    }

    if (pin < 14) {
      if (val > 1)
        analogWrite(pin, val);
      else
        digitalWrite(pin, val);
    }
    else if (val < 2) {
      digitalWrite(pin, val);
    }
    else {
      snprintf(errstr, 64, "pin %d does not support analog writes", pin);
      ofcall->type = RError;
      ofcall->ename = errstr;

      return;
    }

    line = strtok(NULL, "\n");
  }
}

Fcall*
fs_write(Fcall *ifcall, unsigned char *in) {
  struct hentry *cur = fs_fid_find(ifcall->fid);

  ofcall.count = ifcall->count;

  if (cur == NULL) {
    ofcall.type = RError;
    strcpy(errstr, "invalid fid");
    ofcall.ename = errstr;
  }
  else if (((unsigned long)cur->data) == Qroot) {
    ofcall.type = RError;
    strcpy(errstr, "is a directory");
    ofcall.ename = errstr;
  }
  else if (((unsigned long)cur->data) == Qdata) {
    setpinvals((char*)in, &ofcall);
  } 
  else if (((unsigned long)cur->data) == Qctl) {
    setpindirs((char*)in, &ofcall);
  }
  else {
    sprintf(errstr, "unknown path: %x\n", (unsigned int)cur->data);
    ofcall.type = RError;
    ofcall.ename = errstr;
  }

  return &ofcall;
}

Fcall*
fs_remove(Fcall *ifcall) {
  ofcall.type = RError;
  strcpy(errstr, "remove not allowed");
  ofcall.ename = errstr;

  return &ofcall;
}

Fcall*
fs_flush(Fcall *ifcall) {
  return ifcall;
}

Fcall*
fs_wstat(Fcall *ifcall) {
  return ifcall;
}

Callbacks callbacks;

/* arduino stuff */

void die(unsigned char code) {
  pinMode(13, OUTPUT);	

  while(true) {
    digitalWrite(13, HIGH);
    delay(1000);
    digitalWrite(13, LOW);
    delay(code * 1000);
  }
}

void setup() {
  Serial.begin(57600);

  fs_fid_init(16);

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
unsigned int msglen = 0;
unsigned int r = 0;
unsigned long lastread = 0;

void loop() {
  unsigned long i;

  while (Serial.available()) {
    msg[r++] = Serial.read();

    lastread = millis();

    if (r >= msglen)
      break;
  }
  
  // timeout
  if ((millis() - lastread) > 1000)
    r = msglen = lastread = 0;

  if (r < msglen || r < 5)
    return;

  if (msglen == 0) {
    i = 0;
    get4(msg, i, msglen);

    // sanity check
    if (msg[i] & 1 ||
	msglen > MAX_MSG ||
	msg[i] < TVersion ||
      msg[i] > TWStat) {
      r = msglen = lastread = 0;
    }

    if ((r != msglen) || (r == 0))
      return;
  }

  if (r == msglen) {
    memset(&ofcall, 0, sizeof(ofcall));

    // proc9p accepts valid 9P msgs of length msglen,
    // processes them using callbacks->various(functions);
    // returns variable out's msglen
    msglen = proc9p(msg, msglen, &callbacks);

    for (i = 0; i < msglen; i++)
      Serial.write(msg[i]);

    lastread = r = msglen = 0;

    return;
  }

  die(1);
}

