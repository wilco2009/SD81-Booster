#include "WILDCARD.h"
#include <string.h>
#include <ctype.h>

bool matches(const char *string, const char *pattern)
{
  int maxstates = strlen(pattern);
  char patternupper[maxstates + 1];
  const char *states[maxstates];

  for (int i = 0; i <= maxstates; ++i)
  {
    patternupper[i] = toupper(pattern[i]);
  }

  int nstates = 1;
  states[0] = patternupper;

  for (const char *p = string; *p; ++p)
  {
    char c = toupper(*p);
    unsigned i = nstates;
    do
    {
      --i;
      if (*states[i] == 0)
      {
        states[i] = states[--nstates];
      }
      else if (*states[i] == '*')
      {
        // We can't follow unlabeled edges, so we have to manage several
        // '*' in a row as a special case, skipping to the last one
        while (*(states[i] + 1) == '*') ++states[i];
        // Add new state if the next character matches
        if (*(states[i] + 1) == c || *(states[i] + 1) == '?')
        {
          // Check we're not going to insert a state that is already there
          bool rep = false;
          for (int j = nstates; j; )
          {
            if (states[--j] == states[i] + 2)
            {
              rep = true;
              break;
            }
          }
          if (!rep)
            states[nstates++] = states[i] + 2;
        }
      }
      else if (*states[i] == '?' || *states[i] == c)
      {
        ++states[i];
      }
      else
      {
        states[i] = states[--nstates];
      }
    }
    while (i);
    if (!nstates)
      return false;
  }

  for (int i = nstates; i > 0; )
  {
    --i;
    while (*states[i] == '*')
      ++states[i];
    if (*states[i] == 0)
      return true;
  }
  return false;
}
