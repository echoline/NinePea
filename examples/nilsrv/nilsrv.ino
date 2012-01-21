#include <NinePea.h>

Fcall*
fs_version(Fcall *ifcall) {
  if (ifcall->msize > MAX_MSG)
    ifcall->msize = MAX_MSG;

  return ifcall;
}

Fcall*
fs_attach(Fcall *ifcall) {
  Fcall *ofcall = (Fcall*)calloc(1, sizeof(Fcall));

  ofcall->qid.type = QTDIR | QTTMP;
  
  return ofcall;
}

Fcall*
fs_walk(Fcall *ifcall) {
  Fcall *ofcall = (Fcall*)calloc(1, sizeof(Fcall));

  if (ifcall->nwname != 0) {
    ofcall->type = RError;
    ofcall->ename = strdup("unable to walk");
  }

  return ofcall;
}

Fcall*
fs_stat(Fcall *ifcall) {
  Fcall *ofcall = (Fcall*)calloc(1, sizeof(Fcall));

  ofcall->stat.qid.type = QTDIR | QTTMP;
  ofcall->stat.mode = 0444 | DMDIR | DMTMP;
  ofcall->stat.atime = 0;
  ofcall->stat.mtime = 0;
  ofcall->stat.length = 0;
  ofcall->stat.name = strdup("/");
  ofcall->stat.uid = strdup("none");
  ofcall->stat.gid = strdup("none");
  ofcall->stat.muid = strdup("none");
  
  return ofcall;
}

Fcall*
fs_clunk(Fcall *ifcall) {
  return ifcall;
}

Fcall*
fs_open(Fcall *ifcall) {
  Fcall *ofcall;

  ofcall = (Fcall*)calloc(1, sizeof(Fcall));
  ofcall->qid.type = QTDIR | QTTMP;

  return ofcall;
}

Fcall*
fs_read(Fcall *ifcall, unsigned char *out) {
  Fcall *ofcall = (Fcall*)calloc(1, sizeof(Fcall));

  ofcall->count = 0;
  out[0] = '\0';
    
  return ofcall;
}

Fcall*
fs_create(Fcall *ifcall) {
  Fcall *ofcall = (Fcall*)calloc(1, sizeof(Fcall));

  ofcall->type = RError;
  ofcall->ename = strdup("create not allowed");

  return ofcall;
}

Fcall*
fs_write(Fcall *ifcall, unsigned char *in) {
  Fcall *ofcall = (Fcall*)calloc(1, sizeof(Fcall));

  ofcall->type = RError;
  ofcall->ename = strdup("write not allowed");

  return ofcall;
}

Fcall*
fs_remove(Fcall *ifcall) {
  Fcall *ofcall = (Fcall*)calloc(1, sizeof(Fcall));

  ofcall->type = RError;
  ofcall->ename = strdup("remove not allowed");

  return ofcall;
}

Fcall*
fs_flush(Fcall *ifcall) {
  return ifcall;
}

Callbacks callbacks;

void
die(unsigned char code) {
  pinMode(13, OUTPUT);	

  while(true) {
    digitalWrite(13, HIGH);
    delay(1000);
    digitalWrite(13, LOW);
    delay(code * 1000);
  }
}

void setup() {
  Serial.begin(115200);
  
  callbacks.version = fs_version;
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
}

unsigned int len = 0;
unsigned char msg[MAX_MSG+1];
unsigned int msglen = 0;
unsigned char *out;

void loop() {
  unsigned int i;

  while (Serial.available()) {
    msg[len++] = Serial.read();

    if (len == msglen)
      break;

    if (len >= MAX_MSG) {
      die(1);
    }
  }

  if (len < msglen || len < 4)
    return;

  if (msglen == 0) {
    i = 0;
    get4(msg, i, msglen);

    if (msglen > MAX_MSG) {
      die(3);
    }

    return;
  }

  if (len == msglen) {
     msglen = proc9p(msg, msglen, &callbacks, out);

     for (i = 0; i < msglen; i++)
       Serial.write(out[i]);
     
     len = msglen = 0;
     
     return;
  }

  die(5);
}