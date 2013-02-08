#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#define SERVER "irc.dks.ca"
#define PORT 6667
#define IDENT "l33tpoker"
#define IRCNAME "l33tpoker"
#define NICK "l33tpoker"
#define CHANNEL "#mychan"
// Note leading space. "" for no key at all.
#define KEY " mykey"

#define STARTING_BANK 100
#define MAXBET 10
#define DECKS 1
#define ANTE 2


int evalhand(int *, int *);


struct player_s {
  char name[20];
  int bank;

  int in;
  int owed;

  int cards[2];

  struct player_s *next;
  struct player_s *prev;
};



#define STATE_WAITING_FOR_PLAYERS 0
#define STATE_BETTING 1



int msock;
FILE *mfp;
int state=STATE_WAITING_FOR_PLAYERS;

int shoe[52*8];
int currcard;

int flop[5];

int pot;
int numin;
int currbet;
int currbetround;

struct player_s *dealer=NULL;
struct player_s *lastraiser, *currbetter;




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



struct player_s *add_player(char *name) {

  struct player_s *tp;

  if (dealer == NULL) {
    dealer = (struct player_s *) malloc(sizeof(struct player_s));
    strncpy(dealer->name, name, sizeof(tp->name));
    dealer->bank = STARTING_BANK;
    dealer->in = 0;
    dealer->next = dealer->prev = dealer;
    return dealer;
  }

  tp = (struct player_s *) malloc(sizeof(struct player_s));
  tp->bank = 100;
  tp->in = 0;
  strncpy(tp->name, name, sizeof(tp->name));

  tp->next = dealer;
  tp->prev = dealer->prev;
  dealer->prev->next = tp;
  dealer->prev = tp;

  return tp;

}


struct player_s *find_player(char *name) {

  struct player_s *tp=dealer;

  if (tp == NULL) return NULL;
  if (strcasecmp(dealer->name, name) == 0) return dealer;

  tp = dealer->next;
  while(tp != dealer) {
    if (strcasecmp(tp->name, name) == 0) return tp;
    tp = tp->next;
  }

  return NULL;

}


int getcard() {
  currcard++;

  return shoe[currcard-1];
}



void cardname(int c, char *obuf) {

  static char nbuf[] = "23456789TJQKA";
  static char sbuf[] = "cdhs";

  obuf[0] = nbuf[c%13];
  obuf[1] = sbuf[c/13];
  obuf[2] = '\0';

}


void shuffle(int numswaps) {

  int tp, i, j, swap1, swap2;

  for (j=0;j<DECKS;j++) {
    for (i=0;i<52;i++) {

      shoe[(j*52)+i] = i;

    }
  }

  for (i=0; i<numswaps; i++) {

    tp = rand();

    swap1 = (tp & 0xFF) % (52*DECKS);
    swap2 = ((tp >> 8) & 0xFF) % (52*DECKS);

    tp = shoe[swap1];
    shoe[swap1] = shoe[swap2];
    shoe[swap2] = tp;

  }

  currcard = 0;

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


void nullify_players() {

  struct player_s *tp;

  if (dealer == NULL) return;

  dealer->in = 0;
  dealer->owed = 0;

  tp = dealer->next;
  while (tp != dealer) {
    tp->in = 0;
    tp->owed = 0;

    tp = tp->next;
  }

}



void send_to_chan(const char *fmt, ...) {

  va_list ap;
  char buf[2048];
  char buf2[2048];

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  snprintf(buf2, sizeof(buf2), "PRIVMSG %s :%s\n", CHANNEL, buf);
  write(msock, buf2, strlen(buf2));

}


void send_to_player_s(struct player_s *p, const char *fmt, ...) {

  va_list ap;
  char buf[2048];
  char buf2[2048];

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  snprintf(buf2, sizeof(buf2), "PRIVMSG %s :%s\n", p->name, buf);
  write(msock, buf2, strlen(buf2));

}

// It might be good in the future to be able to send the bankroll
// automatically to the player in all messages and such...

void send_to_player(char *name, const char *fmt, ...) {

  struct player_s *p;
  va_list ap;
  char buf[2048];
  char buf2[2048];

  p = find_player(name);
  if (p == NULL) return;

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  snprintf(buf2, sizeof(buf2), "PRIVMSG %s :%s\n", p->name, buf);
  write(msock, buf2, strlen(buf2));

}



void award_pot_to(struct player_s *tp) {

  send_to_chan("[%s] collects $%d from the pot", tp->name, pot);

  tp->bank += pot;

  send_to_chan("Waiting for players to !ante...");
  state = STATE_WAITING_FOR_PLAYERS;
  numin = pot = 0;

}



struct player_s *next_in_player(struct player_s *tp) {

  tp = tp->next;
  while (tp->in == 0) tp = tp->next;

  return tp;

}

// There must (obviously) be more than 1 player to call this function
void do_deal() {

  struct player_s *tp;
  char c1[3], c2[3];

  dealer = next_in_player(dealer);

  send_to_chan("[%s] deals the cards! Let's play!", dealer->name);

  shuffle(400*DECKS);

  dealer->cards[0] = getcard();
  dealer->cards[1] = getcard();
  cardname(dealer->cards[0], &c1[0]);
  cardname(dealer->cards[1], &c2[0]);
  send_to_player_s(dealer, "You have been dealt [%s, %s]\n", c1, c2);

  tp = dealer->next;
  while (tp != dealer) {

    cardname(tp->cards[0] = getcard(), &c1[0]);
    cardname(tp->cards[1] = getcard(), &c2[0]);

    send_to_player_s(tp, "You have been dealt [%s, %s]", c1, c2);

    tp = tp->next;

  }

  lastraiser = currbetter = next_in_player(dealer);

  send_to_chan("[%s] please !bet or !fold", currbetter);

  state = STATE_BETTING;
  currbet = 0;
  currbetround = 0;

}


void raise_amount_owed_by_everyone_except(struct player_s *tp, int owed) {
  struct player_s *tp2=next_in_player(tp);

  while (tp2 != tp) {
    tp2->owed += owed;
    tp2 = next_in_player(tp2);
  }

}



void next_betting_round() {

  char c1[3], c2[3], c3[3], c4[3], c5[3];
  struct player_s *tp, *highest;
  int score, tscore, nscore;

  if (currbetround == 0) {
    cardname(flop[0] = getcard(), &c1[0]);
    cardname(flop[1] = getcard(), &c2[0]);
    cardname(flop[2] = getcard(), &c3[0]);

    send_to_chan("The dealer deals 3 cards: [%s, %s, %s]", c1, c2, c3);

    currbetter = lastraiser = next_in_player(dealer);
    send_to_chan("[%s] please !bet or !fold", currbetter);

    currbet = 0;
    currbetround++;
  } else if (currbetround == 1) {
    cardname(flop[0], &c1[0]);
    cardname(flop[1], &c2[0]);
    cardname(flop[2], &c3[0]);
    cardname(flop[3] = getcard(), &c4[0]);

    send_to_chan("The dealer deals another card: [%s, %s, %s, %s]", c1, c2, c3, c4);
    currbetter = lastraiser = next_in_player(dealer);
    send_to_chan("[%s] please !bet or !fold", currbetter);

    currbet = 0;
    currbetround++;
  } else if (currbetround == 2) {
    cardname(flop[0], &c1[0]);
    cardname(flop[1], &c2[0]);
    cardname(flop[2], &c3[0]);
    cardname(flop[3], &c4[0]);
    cardname(flop[4] = getcard(), &c5[0]);

    send_to_chan("The dealer deals the last card: [%s, %s, %s, %s, %s]", c1, c2, c3, c4, c5);
    currbetter = lastraiser = next_in_player(dealer);
    send_to_chan("[%s] please !bet or !fold", currbetter);

    currbet = 0;
    currbetround++;
  } else if (currbetround == 3) {

    tp = currbetter;

    nscore = 1;
    score = evalhand(&flop[0], &(currbetter->cards[0]));
    highest = tp = next_in_player(tp);

    while (tp != currbetter) {
      tscore = evalhand(&flop[0], &(tp->cards[0]));
      if (tscore == score) {
        nscore++;
      } else if (tscore > score) {
        nscore = 1;
        score = tscore;
        highest = tp;
      }
      tp = next_in_player(tp);
    }

    // score now contains the highest score
    // nscore now contains the number of players that scored that

    if (nscore == 1) {
      award_pot_to(highest);
    } else {
      // FIXME: pot splitting
    }

  }

}



void do_bet(char cmd, char *arg) {

  int tp;

  if (currbet == 0) {
    if (cmd == 'b') {
      if (arg == NULL) {
        send_to_chan("[%s] How much do you want to bet?", currbetter->name);
        return;
      }

      tp = atoi(arg);

      if (tp <= 0 || tp > MAXBET) {
        send_to_chan("[%s] bets must be between $1 and $%d", currbetter->name, MAXBET);
        return;
      }

      pot = currbet = tp;
      currbetter->bank -= tp;
      send_to_chan("[%s] bets $%d", currbetter, tp);

      raise_amount_owed_by_everyone_except(currbetter, tp);
      lastraiser = currbetter;
      currbetter = next_in_player(currbetter);

      send_to_chan("[%s] you owe $%d to the pot. Please either !raise, !call, or !fold", currbetter->name, currbetter->owed);
      return;

    } else if (cmd == 'f') {

      send_to_chan("[%s] folds.", currbetter->name);
      currbetter->in = 0;
      numin--;

      lastraiser = currbetter = next_in_player(currbetter);

      if (numin > 1) send_to_chan("[%s] please !bet or !fold", currbetter->name);
      return;

    } else {
      send_to_chan("[%s] you must either !bet or !fold", currbetter->name);
      return;
    }
  } else {

    if (cmd == 'r') {

      if (currbet >= MAXBET) {
        send_to_chan("[%s] the bet this round has reached its limit of $%d", currbetter->name, MAXBET);
        return;
      }

      tp = atoi(arg);

      if (tp <= 0 || tp > MAXBET-currbet) {
        send_to_chan("[%s] a raise must be between $1 and $%d", currbetter->name, MAXBET-currbet);
        return;
      }


      pot += currbetter->owed + tp;
      send_to_chan("[%s] throws $%d into the pot, raising the bet by $%d", currbetter->name, currbetter->owed + tp, tp);

      currbetter->bank -= currbetter->owed + tp;
      currbet += tp;
      currbetter->owed = 0;

      raise_amount_owed_by_everyone_except(currbetter, tp);
      lastraiser = currbetter;
      currbetter = next_in_player(currbetter);

      send_to_chan("[%s] you owe $%d to the pot. Please either !raise, !call, or !fold", currbetter->name, currbetter->owed);
      return;

    } else if (cmd == 'c') {

      pot += currbetter->owed;
      send_to_chan("[%s] calls, throwing $%d into the pot", currbetter->name, currbetter->owed);

      currbetter->bank -= currbetter->owed;
      currbetter->owed = 0;
      currbetter = next_in_player(currbetter);

      if (currbetter == lastraiser) {
        next_betting_round();
        return;
      }

      send_to_chan("[%s] you owe $%d to the pot. Please either !raise, !call, or !fold", currbetter->name, currbetter->owed);
      return;

    } else if (cmd == 'f') {

      send_to_chan("[%s] folds.", currbetter->name);

      numin--;
      currbetter->in = 0;
      currbetter = next_in_player(currbetter);

      if (numin > 1) send_to_chan("[%s] you owe $%d to the pot. Please either !raise, !call, or !fold", currbetter->name, currbetter->owed);
      return;

    } else {
      send_to_chan("[%s] you owe $%d to the pot. Please either !raise, !call, or !fold", currbetter->name, currbetter->owed);
      return;
    }

  }

}




void handle_privmsg(char *sender, char *msg, char *arg) {

  struct player_s *tp;

  // FIXME: VERSION reply goes here

  if (state == STATE_WAITING_FOR_PLAYERS) {
    if (msg[0] == '!' && msg[1] == 'a') {
      tp = find_player(sender);

      if (tp == NULL) {
        send_to_chan("NEW PLAYER! [%s] now has $%d", sender, STARTING_BANK);
        tp = add_player(sender);
      }

      if (numin == 0) {
        nullify_players();
        pot = 0;
      }

      send_to_chan("[%s] has anted $%d! Waiting for more players!", sender, ANTE);
      tp->bank -= ANTE;
      tp->in = 1;

      pot += ANTE;
      numin++;
    }

    if (msg[0] == '!' && msg[1] == 'd') {
      if (numin <= 1) send_to_chan("[%s] idiotically requested a deal but there aren't enough players anted", sender);
      else do_deal();
    }

  } else if (state == STATE_BETTING) {
    if (strcasecmp(sender, currbetter->name) == 0) {
      if (msg[0] == '!') do_bet(msg[1], arg);
    }

    if (numin == 1) {
      tp = next_in_player(dealer);

      award_pot_to(tp);
    }
  }


}



void process() {

  char buf[2048];
  char buf2[2048];
  char *segs[10];
  char *tp;

  while(fgets(buf, sizeof(buf), mfp) != NULL) {

    // DEBUGGING:
    printf("%s",buf);

    stripcrlf(buf);
    split(buf, segs, 10);

    if (segs[0] == NULL) continue;

    if (segs[1] == NULL) continue;

    if (strcmp(segs[0], "PING") == 0) {
      snprintf(buf2, sizeof(buf2), "PONG %s\n", segs[1]);
      write(msock, buf2, strlen(buf2));
      continue;
    }

    if (segs[2] == NULL) continue;
    if (segs[3] == NULL) continue;

    if (strcmp(segs[1], "PRIVMSG") == 0) {
      segs[0]++; // skip the :
      tp = segs[0];
      while (*tp != '\0' && *tp != '!') tp++;
      *tp = '\0';
      handle_privmsg(segs[0], segs[3]+1, segs[4]);
    }

  }

}




int main() {

  srand(time(NULL)+getpid());

  connecttoserver();

  process();

  return 0;

}
