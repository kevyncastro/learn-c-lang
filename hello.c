#include <stdio.h>

void FToC() {
  //declare F, C, upperlimit, lowerlimit, interval
  float F, C;
  int upperlimit, lowerlimit, interval;

  upperlimit = 300;
  lowerlimit = 0;
  interval = 20;

  F = lowerlimit;

  while(F <= upperlimit) {
    C = 5*(F-32)/9;
    printf("%2f C = %2f F \n", C, F);
    F = F + interval;
  }
}

void pointersExample() {
  int x = 42;
  int *p = &x;

  printf("%d", *p);
}

int main() {

  pointersExample();
  return 0;
}

