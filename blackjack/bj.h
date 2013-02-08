#define JACK  11
#define QUEEN 12
#define KING  13
#define ACE   14

#define STATE_IDLE 0
#define STATE_BETTING 1
#define STATE_PLAYING 2

#define INITIAL_CASH 1000
#define MAXBET 10000
#define BETTING_TIME 10
#define PLAYING_TIME 10

#define SERVER "irc.dks.ca"
#define PORT 6667
#define CHANNEL "#mychan"
// Note the leading space. Use "" for no key at all
#define KEY " mykey"
#define IDENT "bj"
#define IRCNAME "!help"
#define NICK "blaqjaq"



struct playerstruct {

  char name[100];

  int money;
  int currbet;
  int locked;
  int insbet;

  int numcards;
  int hand[21];

  struct playerstruct *next;

};

extern int msock;
extern int state;
extern int shoe[52*8];
extern int dealerhand[21];
extern int dealercards;
extern int decks;
extern int lock;



void nullifyplayers();
void shuffle(int numswaps);
void procalarm();
void procinsurance(char *source, char *nm, int tval);
void prochit(char *source, char *nm);
void procdouble(char *source, char *nm);
void makebet(char *source, char *nm, int tval);
