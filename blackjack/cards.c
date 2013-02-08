#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "bj.h"


struct playerstruct *players=NULL;

int shoe[52*8];
int currcard;
int decks;
int dealerhand[21];
int dealercards;


char *translate(int cd) {

  if (cd == 2) return "2";
  if (cd == 3) return "3";
  if (cd == 4) return "4";
  if (cd == 5) return "5";
  if (cd == 6) return "6";
  if (cd == 7) return "7";
  if (cd == 8) return "8";
  if (cd == 9) return "9";
  if (cd == 10) return "10";
  if (cd == 11) return "J";
  if (cd == 12) return "Q";
  if (cd == 13) return "K";
  if (cd == 14) return "A";

  return "";

}



int handvalcore(int numcards, int *hand) {

  int numaces=0,sum=0,i;

  for(i=0; i<numcards; i++) {
    if (hand[i] < 10) {
      sum += hand[i];
    } else if (hand[i] == 14) {
      numaces++;
      sum += 11;
    } else {
      sum += 10;
    }
  }

  while (sum>21 && numaces>0) {
    numaces--;
    sum-=10;
  }

  return sum;

}


int handval(struct playerstruct *tp) {

  return handvalcore(tp->numcards, tp->hand);

}



int getcard() {
  currcard++;

  return shoe[currcard-1];
}



void shuffle(int numswaps) {

  int tp, i, j, swap1, swap2;

  for (j=0;j<decks;j++) {
    for (i=0;i<52;i++) {

      tp = (i % 13) + 2;

      shoe[(j*52)+i] = tp;

    }
  }

  for (i=0; i<numswaps; i++) {

    tp = rand();

    swap1 = (tp & 0xFF) % (52*decks);
    swap2 = ((tp >> 8) & 0xFF) % (52*decks);

    tp = shoe[swap1];
    shoe[swap1] = shoe[swap2];
    shoe[swap2] = tp;

  }

  currcard = 0;

}


struct playerstruct *newplayer(char *name) {

  struct playerstruct *tp;

  tp = (struct playerstruct *)malloc(sizeof(struct playerstruct));

  tp->next = players;
  players = tp;

  strncpy(tp->name, name, 100);

  tp->money = INITIAL_CASH;
  tp->currbet = 0;

  return tp;

}



void disphand(struct playerstruct *tp, char *dest) {

  char buf[2048];
  int i;

  snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039[\00313%s\0039's HAND:] (\0034 %d \0039) \0039[\00311 ", dest, tp->name, handval(tp));

  for(i=0;i<tp->numcards;i++) {
    if (i!=0) strcat(buf, ",");
    strcat(buf, translate(tp->hand[i]));
  }

  strcat(buf, " \0039]\n");

  write(msock, buf, strlen(buf));

}



void deal() {

  struct playerstruct *tp;
  char buf[1024];

  tp = players;

  while(tp != NULL) {
    tp->numcards = 0;

    if (tp->currbet > 0) {
      tp->numcards = 2;
      tp->hand[0] = getcard();
      tp->hand[1] = getcard();
    }

    tp = tp->next;
  }

  dealercards = 2;
  dealerhand[0] = getcard();
  dealerhand[1] = getcard();

  snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039--BETS ARE \0034CLOSED\0039--\n", CHANNEL);
  write(msock, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039DEALER IS SHOWING [\00311 %s \0039]\n",
           CHANNEL, translate(dealerhand[0]));
  write(msock, buf, strlen(buf));

  tp = players;

  while(tp != NULL) {
    if (tp->currbet > 0) disphand(tp, CHANNEL);
    if (handval(tp) == 21) {
      int bjval;

      bjval = (int) (tp->currbet * 1.5);
      snprintf(buf, sizeof(buf), "PRIVMSG %s :\00313%s \0039got a \0034BLACKJACK\0039! Paying\0034 $%d\n", CHANNEL, tp->name, bjval);
      write(msock, buf, strlen(buf));
      tp->money += bjval + tp->currbet;
      tp->currbet = 0;
    }
    tp = tp->next;
  }

}



void procdealerhand() {

  char buf[1024];
  int i;

  snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039[DEALER'S HAND:] (\0034 %d \0039) \0039[\00311 ", CHANNEL, handvalcore(dealercards, dealerhand));

  for(i=0;i<dealercards;i++) {
    if (i!=0) strcat(buf, ",");
    strcat(buf, translate(dealerhand[i]));
  }

  strcat(buf, " \0039]\n");

  write(msock, buf, strlen(buf));


  while(handvalcore(dealercards, dealerhand) < 17) {

    dealerhand[dealercards++] = getcard();

    snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039[DEALER'S HAND:] (\0034 %d \0039) \0039[\00311 ", CHANNEL, handvalcore(dealercards, dealerhand));

    for(i=0;i<dealercards;i++) {
      if (i!=0) strcat(buf, ",");
      strcat(buf, translate(dealerhand[i]));
    }

    strcat(buf, " \0039]\n");

    write(msock, buf, strlen(buf));

  }


  if (handvalcore(dealercards, dealerhand) > 21) {
    snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039DEALER \0034BUSTS\n", CHANNEL);
    write(msock, buf, strlen(buf));
  }

  return;

}


void payoff() {

  char buf[1024];
  struct playerstruct *tp;
  int dealerval, playerval;

  snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039--PLAYING TIME IS \0034UP\0039--\n", CHANNEL);
  write(msock, buf, strlen(buf));

  procdealerhand();

  dealerval = handvalcore(dealercards, dealerhand);

  tp = players;

  while(tp != NULL) {
    if (tp->currbet > 0) {

      playerval = handval(tp);

      if (playerval > dealerval || dealerval > 21) {
        snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039PAID \00311%s \0034$%d\n", CHANNEL, tp->name, tp->currbet);
        write(msock, buf, strlen(buf));
        tp->money += tp->currbet * 2;
        tp->currbet = 0;
      } else if (playerval == dealerval) {
        snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039PUSH with \00311%s\0039. Giving back \0034$%d\n", CHANNEL, tp->name, tp->currbet);
        write(msock, buf, strlen(buf));
        tp->money += tp->currbet;
        tp->currbet = 0;
      } else {
        snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039TOOK \00311%s\0039's \0034$%d\n", CHANNEL, tp->name, tp->currbet);
        write(msock, buf, strlen(buf));
        tp->currbet = 0;
      }

    }

    if (tp->insbet > 0 && dealerval == 21 && dealercards == 2) {
      snprintf(buf, sizeof(buf), "PRIVMSG %s :\00311INSURANCE \0039PAID \00311%s\0039's \0034$%d\n", CHANNEL, tp->name, tp->insbet * 2);
      write(msock, buf, strlen(buf));
      tp->money += tp->insbet * 2;
      tp->insbet = 0;
    }
      
    tp = tp->next;
  }

}



void nullifyplayers() {

  struct playerstruct *tp;

  tp = players;

  while(tp != NULL) {
    tp->currbet = 0;
    tp->locked = 0;
    tp->insbet = 0;
    tp = tp->next;
  }

}



void procalarm() {

  char buf[1024];

  if (lock) {
    alarm(1);
    return;
  }

  if (state == STATE_BETTING) {
    if (((int)(52*decks*.75)) <= currcard) {
      snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039**RESHUFFLING**\n", CHANNEL);
      write(msock, buf, strlen(buf));
      shuffle(400*decks);
      currcard = 0;
    }
    deal();
    alarm(PLAYING_TIME);
    state = STATE_PLAYING;
  } else if (state == STATE_PLAYING) {
    payoff();
    nullifyplayers();
    state = STATE_IDLE;
  }

  return;

}




void procinsurance(char *source, char *nm, int tval) {

  struct playerstruct *tp;
  char buf[1024];

  tp = players;

  while (tp != NULL) {

    if (strcmp(nm, tp->name)==0) {
      if (tp->currbet <= 0) {
        snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039Uh, \00311%s\0039, you kinda have to be in the game to buy insurance.\n", source, nm);
        write(msock, buf, strlen(buf));
        return;
      }

      if (dealerhand[0] != 14) {
        snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039Uh, \00311%s\0039, the dealer must be showing an ace in order to buy insurance.\n", source, nm);
        write(msock, buf, strlen(buf));
        return;
      }

      if (tval > tp->currbet / 2) {
        snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039\00311%s\0039, insurance can be at most 1/2 your bet.\n", source, nm);
        write(msock, buf, strlen(buf));
        return;
      }

      snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039\00311%s\0039 buys insurance worth [\0034$%d\0039]\n", source, nm, tval);
      write(msock, buf, strlen(buf));

      tp->money -= tval;
      tp->insbet = tval;

      return;
      
    }

    tp = tp->next;

  }

}



void procdouble(char *source, char *nm) {

  struct playerstruct *tp;
  char buf[1024];

  tp = players;

  while (tp != NULL) {

    if (strcmp(nm, tp->name)==0) {
      if (tp->numcards != 2) {
        snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039Sorry, \00311%s\0039, you can't double down now.\n", source, nm);
        write(msock, buf, strlen(buf));
        return;
      }

      if (tp->locked) {
        snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039Sorry, \00311%s\0039, you've doubled down.\n", source, nm);
        write(msock, buf, strlen(buf));
        return;
      }

      tp->currbet *= 2;
      snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039DOUBLING \00311%s\0039's bet to [\0034$%d\0039].\n", CHANNEL, nm, tp->currbet);
      write(msock, buf, strlen(buf));
      prochit(source, nm);
      tp->locked = 1;

      return;
    }

    tp = tp->next;
  }

}



void prochit(char *source, char *nm) {

  struct playerstruct *tp;
  char buf[1024];

  tp = players;

  while (tp != NULL) {

    if (strcmp(nm, tp->name)==0) {
      if (tp->currbet == 0) {
        snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039Sorry, \00311%s\0039, you're not in this one.\n", source, nm);
        write(msock, buf, strlen(buf));
        return;
      }
      if (tp->locked) {
        snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039Sorry, \00311%s\0039, you've doubled down.\n", source, nm);
        write(msock, buf, strlen(buf));
        return;
      }

      alarm(PLAYING_TIME);

      tp->hand[(tp->numcards)++] = getcard();

      disphand(tp, CHANNEL);

      if (handval(tp) > 21) {
        snprintf(buf, sizeof(buf), "PRIVMSG %s :\00311%s \0034BUSTED\n", CHANNEL, nm);
        write(msock, buf, strlen(buf));
        tp->currbet = 0;
        return;
      }
      
    }

    tp = tp->next;
  }

}


void makebet(char *source, char *nm, int bet) {

  struct playerstruct *tp;
  char buf[1024];

  if (state == STATE_IDLE) {
    snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039BETTING WILL END IN [\00310 %d \0039] SECONDS\n", CHANNEL, BETTING_TIME);
    write(msock, buf, strlen(buf));

    state = STATE_BETTING;

    alarm(BETTING_TIME);
  }

  if (state != STATE_BETTING) {
    snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039Bets are \0034closed\n", source);
    write(msock, buf, strlen(buf));
    return;
  }


  tp = players;
  while (tp != NULL) {
    if (strcmp(tp->name, nm) == 0) {
      if (tp->currbet) tp->money += tp->currbet;
      tp->currbet = bet;
      tp->money -= bet;
      snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039[\00313%s\0039 balance=\00313$%d\0039] \0034%s \0039[\0034$%d\0039]\n",
               CHANNEL, nm, tp->money, tp->currbet ? "CHANGED BET" : "BET", bet);
      write(msock, buf, strlen(buf));
      return;
    }
    tp = tp->next;
  }

  tp = newplayer(nm);
  tp->currbet = bet;
  tp->money -= bet;
  snprintf(buf, sizeof(buf), "PRIVMSG %s :\0039[\00313%s\0039 balance=\00313$%d\0039] \0034BET \0039[\0034$%d\0039]\n",
                             CHANNEL, nm, tp->money, bet);
  write(msock, buf, strlen(buf));

  return;

}
