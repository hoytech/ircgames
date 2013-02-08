#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#include "bj.h"


int lock;
int msock;
int state;
FILE *mfp;



int split(char *tp, char *sp[], int maxwords) {

  int i=0;

  while(1) {
    /* Skip leading whitespace */
    while(*tp == ' ') tp++;

    if (*tp == '\0') {
      sp[i] = NULL;
      break;
    }
    if (i==maxwords-2) {
      sp[maxwords-1] = NULL;
      break;
    }

    sp[i] = tp;

    while(*tp != ' ' && *tp != '\0') tp++;
    if (*tp == ' ') *tp++ = '\0';
    i++;

  }

  return i;

}


void stripcrlf(char *buf) {

  register char *p=buf;

  while (p && *p) {
    if (*p=='\n' || *p=='\r') *p='\0';
     p++;
  }

}





int openconnection(char *toconn, int port) {

  struct hostent *he;
  struct sockaddr_in their_addr; /* connector's address information */
  int sd;

  if ((he=gethostbyname(toconn)) == NULL) {  /* get the host info */
    return -1;
  }

  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    return -1;
  }

  their_addr.sin_family = AF_INET;         /* host byte order */
  their_addr.sin_port = htons(port);     /* short, network byte order */
  their_addr.sin_addr = *((struct in_addr *)he->h_addr);
  memset(&(their_addr.sin_zero),'\0',8);  /* zero rest of struct */

  if (connect(sd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1) {
    return -1;
  }

  return sd;

}


void connecttoserver() {

  char tp[512];

  msock = openconnection(SERVER, PORT);
  if (msock == -1) {
    fprintf(stdout,"Unable to connect to %s on port %d!\n", SERVER, PORT);
    exit(-1);
  }

  mfp = fdopen(msock, "r");

  snprintf(tp, sizeof(tp), "USER %s +iw %s :%s\nNICK %s\n",IDENT,IDENT,IRCNAME,NICK);
  write(msock, tp, strlen(tp));

  snprintf(tp, sizeof(tp), "JOIN %s%s\n", CHANNEL, KEY);
  write(msock, tp, strlen(tp));

}


char *getnick(char *tp) {

  int i;

  for(i=0; tp[i] != '!'; i++) ;
  tp[i] = '\0';

  // Skip the leading ':'
  return tp+1;

}



void process() {

  char tp[1024];
  char buf[1024];
  char *segs[10];

  while((lock=0)==0 &&
        fgets(tp, sizeof(tp), mfp)!=NULL &&
        (lock=1)==1) {

    stripcrlf(tp);
    split(tp, segs, 10);

    if (segs[0] == NULL) continue;
    // 1 words:


    if (segs[1] == NULL) continue;
    // 2 words:

    if (strcmp(segs[0], "PING")==0) {
      snprintf(buf, sizeof(buf), "PONG %s\n", segs[1]);
      write(msock, buf, strlen(buf));
      continue;
    }

    if (strcmp(segs[1], "PRIVMSG")==0) {
      char *nick, *source;

      nick = getnick(segs[0]);
      if (*segs[2] == '#') source = segs[2];
      else source = nick;

      if (strcmp(segs[3]+1, "!bet")==0) {
        int tval;

        if (segs[4] == NULL) continue;

        tval = atoi(segs[4]);

        if (tval <= 0 || tval > MAXBET) {
          snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039Bets must be between \0034$1\0039 and \0034$%d\n", source, MAXBET);
          write(msock, buf, strlen(buf));
          continue;
        }

        makebet(source, nick, tval);

        continue;
      }

      if (strcmp(segs[3]+1, "!hit")==0) {
        if (state == STATE_IDLE) {
          snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039Uh.. don't you want to place a bet first, \00311%s\0039?\n", source, nick);
          write(msock, buf, strlen(buf));
          continue;
        }
        if (state == STATE_BETTING) {
          snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039Easy there \00311%s\0039, we're still betting.\n", source, nick);
          write(msock, buf, strlen(buf));
          continue;
        }

        prochit(source, nick);

        continue;
      }

      if (strcmp(segs[3]+1, "!double")==0) {

        if (state == STATE_IDLE) {
          snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039Uh.. don't you want to place a bet first, \00311%s\0039?\n", source, nick);
          write(msock, buf, strlen(buf));
          continue;
        }
        if (state == STATE_BETTING) {
          snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039Easy there \00311%s\0039, we're still betting.\n", source, nick);
          write(msock, buf, strlen(buf));
          continue;
        }

        procdouble(source, nick);

        continue;

      }

      if (strcmp(segs[3]+1, "!insurance")==0) {

        int tval;

        if (state != STATE_PLAYING) {
          snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039Uh \00311%s\0039, you can't buy insurance now.\n", source, nick);
          write(msock, buf, strlen(buf));
          continue;
        }

        if (segs[4] == NULL || !isdigit(*segs[4])) continue;

        tval = atoi(segs[4]);

        if (tval <= 0 || tval > MAXBET) {
          snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039Bets must be between \0034$1\0039 and \0034$%d\n", source, MAXBET);
          write(msock, buf, strlen(buf));
          continue;
        }

        procinsurance(source, nick, tval);

      }

      if (strcmp(segs[3]+1, "!help")==0) {
        snprintf(buf, sizeof(buf), "PRIVMSG %s :HCSW's IRC Blackjack (www.hcsw.org)\n", nick);
        write(msock, buf, strlen(buf));
        snprintf(buf, sizeof(buf), "PRIVMSG %s :To play: /join %s\n", nick, CHANNEL);
        write(msock, buf, strlen(buf));
        snprintf(buf, sizeof(buf), "PRIVMSG %s :         !bet <amount>\n", nick);
        write(msock, buf, strlen(buf));
        snprintf(buf, sizeof(buf), "PRIVMSG %s :         !hit\n", nick);
        write(msock, buf, strlen(buf));
        snprintf(buf, sizeof(buf), "PRIVMSG %s :         !double\n", nick);
        write(msock, buf, strlen(buf));
        snprintf(buf, sizeof(buf), "PRIVMSG %s :         !help\n", nick);
        write(msock, buf, strlen(buf));
        continue;
      }

    }

  }

}



int main() {

  signal(SIGALRM, procalarm);

  srand(time(NULL)+getpid());

  decks=4;

  shuffle(400*decks);
  nullifyplayers();

  connecttoserver();

  process();

  return 0;

}
