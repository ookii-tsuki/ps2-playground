#include <stdio.h>
#include <tamtypes.h>
#include <sifrpc.h>
#include <debug.h>
#include <unistd.h>
#include <kernel.h>
#include <string.h>
#include <timer.h>
#include <graph.h>
#include <gs_psm.h>

int main(int argc, char *argv[])
{
  sceSifInitRpc(0);
  init_scr();


  // Move cursor to 20, 20
  scr_setXY(20, 20);
  scr_printf("Hello, World!\n");

  sleep(2);

  scr_clear();

  char *msg = "Hello <3";

  for (int i = 0; i < strlen(msg); i++)
  {
    // progressively write the message letter by letter
    scr_setXY(20, 20);

    for (int j = 0; j <= i; j++)
    {
      scr_printf("%c", msg[j]);
    }

    sleep(1);

  }
  scr_setXY(20, 22);
    scr_printf("-EE core.");

    sleep(3);

  return 0;
}