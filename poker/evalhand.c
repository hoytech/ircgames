/*

  Returns a value corresponding to the best possible hand you can make
  from the flop and the given cards

  8004-8012 = straight flush (7012 = royal straight flush)
  7000-7012 = four of a kind
  6000-6167 = full house
  5004-5012 = flush
  4004-4012 = straight
  3000-3012 = three of a kind
  2000-2167 = two pair
  1000-1012 = one pair
  0-12 = high card

 */

#define rank(x) ((x)%13)
#define suite(x) ((x)/13)
#define max(x,y) ((x)>(y) ? (x) : (y))

int evalhand(int *flop, int *hand) {

  int c[7];
  int ranksum[13];
  int suitesum[4];
  int matrix[13][4]; // rank x suite
  int i,j;
  int t1,t2;

  c[0] = flop[0]; c[1] = flop[1]; c[2] = flop[2]; c[3] = flop[3]; c[4] = flop[4];
  c[5] = hand[0]; c[6] = hand[1];

  for (i=0; i<13; i++) {
    for (j=0; j<4; j++) matrix[i][j] = 0;
    ranksum[i] = suitesum[i] = 0;
  }

  for (i=0; i<7; i++) {
    ranksum[rank(c[i])]++;
    suitesum[suite(c[i])]++;
    matrix[rank(c[i])][suite(c[i])]++;
  }


  // Straight flush
  for (i=0; i<4; i++) {
    t1 = 0;
    for (j=12; j>=0; j--) {
      if (matrix[j][i] > 0) t1++;
      else t1 = 0;

      if (t1 == 5) return 8000 + 4 + j;
    }
  }


  // Four of a kind
  for (i=12; i>=0; i--) if (ranksum[i] >= 4) return 7000+i;


  // Full house
  t1 = t2 = -1;
  for (i=12; i>=0; i--) {
    if (t1 == -1 && ranksum[i] == 3) t1 = i;
    if (t2 == -1 && ranksum[i] == 2) t2 = i;
  }
  if (t1 != -1 && t2 != -1) return 6000 + (t1*13) + t2;  


  // Flush
  for (i=3; i>=0; i--) {
    if (suitesum[i] >= 5) {
      for (j=12; j>=0; j--) {
        if (matrix[j][i] > 0) return 5000 + j; 
      }
    }
  }

  // Straight
  t1 = 0;
  for (i=12; i>=0; i--) {
    if (ranksum[i]) t1++;
    else t1 = 0;

    if (t1 == 5) return 4000 + 4 + i;
  }

  // Three of a kind
  for (i=12; i>=0; i--) if (ranksum[i] >= 3) return 3000+i;

  // Two of a kind
  for (i=12; i>=0; i--) if (ranksum[i] >= 2) return 1000+i;

  // High card
  for (i=12; i>=0; i--) if (ranksum[i] >= 1 ) return i;

  return -1;

}


/*
main(int argc, char *argv[]) {
  int flop[5];
  int hand[] = { 13, 14 };

  flop[0] = atoi(argv[1]);
  flop[1] = atoi(argv[2]);
  flop[2] = atoi(argv[3]);
  flop[3] = atoi(argv[4]);
  flop[4] = atoi(argv[5]);

  printf("%d\n", evalhand(&flop[0], &hand[0]));

}
*/
