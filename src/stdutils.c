
/* -- Includes ------------------------------------------------------------ */
#include "stdutils.h"


/* -- Defines ------------------------------------------------------------- */


/* -- Types --------------------------------------------------------------- */


/* -- Module Global Function Prototypes ----------------------------------- */


/* -- Module Global Variables --------------------------------------------- */


/* -- Implementation ------------------------------------------------------ */

unsigned int stdutils_uitoa(char * destination, unsigned long value, unsigned int num)
{
   char * start = destination;
   unsigned long len = 0;

   //there must be buffer!
   if ((destination == 0) || (num == 0))
   {
      return 0;
   }


   //unsigned-integer to ascii string
   do
   {
      //check if still enough buffer is left
      if (num <= 1)
      {
         break;
      }

      //to ascii
      *destination++ = (char)('0' + (value % 10));
      value = value / 10;
      --num;
      ++len;
   } while (value != 0);
   //add zero termination
   *destination = 0;

   //swap-byte order
   --destination;
   while (start < destination)
   {
      char tmp = *start;
      *start++ = *destination;
      *destination-- = tmp;
   }

   //return length (without zero termination)
   return len;
}



unsigned int stdutils_strncpy(char * destination, const char * source, unsigned int num)
{
   unsigned long len = 0;
   char c;

   //there must be buffer!
   if ((destination == 0) || (num == 0))
   {
      return 0;
   }
   //
   if (source == 0)
   {
      *destination = 0;
      return 0;
   }

   //copy
   while (c = *source++)
   {
      //check if still enough buffer is left
      if (num <= 1)
      {
         break;
      }

      //copy chars
      *destination++ = c;
      --num;
      ++len;
   }
   //add zero termination
   *destination = 0;
   return len;
}



int stdutils_strncmp(const char * str1, const char * str2, unsigned int num)
{
   int cmp;
   int c1;
   int c2;

   //pointer to null?
   if ((str1 == 0) || (str2 == 0))
   {
      return (int)(str1 - str2);
   }
   if (num == 0)
   {
      return 0;
   }

   //for each char
   do
   {
      c1 = *str1++;
      c2 = *str2++;
      if (c1 != c2) //in case of in-equality
      {
         break;
      }
      --num;
   } while (c1 && c2 && num);
   return (c1 - c2);
}



void stdutils_memclr(void * memory, unsigned int num)
{
   unsigned char * mem = memory;
   while (num--)
   {
      *mem++ = 0;
   }
}
