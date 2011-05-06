/*
 * A test program for GeekOS user mode
 */

#include <process.h>
#include <conio.h>

int main(int argc, char** argv)
{
  Print_String("Yeah man!");	
  Null();
  int i=1;
  for(;i>0;++i);

  return 0;
}
