/*
 * clock.c - Routines for using the cycle counters on x86,
 *           Alpha, and Sparc boxes.
 *
 * Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
 * May not be used, modified, or copied without permission.
 *
 * Modified (with permission) by William Killian (2019)
 */

#include "clock.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/times.h>
#include <unistd.h>

static uint64_t cyc = 0;

inline static uint64_t
rdtsc ()
{
  register uint32_t lo, hi;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t)lo | ((uint64_t)hi << 32));
}

/* Record the current value of the cycle counter. */
void
start_counter ()
{
  cyc = rdtsc ();
}

/* Return the number of cycles since the last call to start_counter. */
long double
get_counter ()
{
  uint64_t curr = rdtsc ();
  long double result = curr - cyc;
  if (result < 0)
  {
    fprintf (stderr, "Error: counter returns neg value: %.0Lf\n", result);
  }
  return result;
}

/*******************************
 * Machine-independent functions
 ******************************/
long double
ovhd ()
{
  /* Do it twice to eliminate cache effects */
  int i;
  long double result;

  for (i = 0; i < 2; i++)
  {
    start_counter ();
    result = get_counter ();
  }
  return result;
}

/* Estimate the clock rate by measuring the cycles that elapse */
/* while sleeping for sleeptime seconds */
long double
mhz_full (int verbose, unsigned sleeptime)
{
  long double rate;

  start_counter ();
  sleep (sleeptime);
  rate = get_counter () / (1e6 * sleeptime);
  if (verbose)
    printf ("Processor clock rate ~= %.1Lf MHz\n", rate);
  return rate;
}


/* Version using a default sleeptime */
long double
mhz (int verbose)
{
  return mhz_full (verbose, 2);
}

/** Special counters that compensate for timer interrupt overhead */

static long double cyc_per_tick = 0.0;

#define NEVENT 100
#define THRESHOLD 1000
#define RECORDTHRESH 3000

/* Attempt to see how much time is used by timer interrupt */
static void
callibrate (int verbose)
{
  long double oldt;
  struct tms t;
  clock_t oldc;
  int e = 0;

  times (&t);
  oldc = t.tms_utime;
  start_counter ();
  oldt = get_counter ();
  while (e < NEVENT)
  {
    long double newt = get_counter ();

    if (newt - oldt >= THRESHOLD)
    {
      clock_t newc;
      times (&t);
      newc = t.tms_utime;
      if (newc > oldc)
      {
        long double cpt = (newt - oldt) / (newc - oldc);
        if ((cyc_per_tick == 0.0 || cyc_per_tick > cpt) && cpt > RECORDTHRESH)
        {
          cyc_per_tick = cpt;
        }
        e++;
        oldc = newc;
      }
      oldt = newt;
    }
  }
  if (verbose)
    printf ("Setting cyc_per_tick to %Lf\n", cyc_per_tick);
}

static clock_t start_tick = 0;

void
start_comp_counter ()
{
  struct tms t;
  if (cyc_per_tick == 0.0)
    callibrate (0);
  times (&t);
  start_tick = t.tms_utime;
  start_counter ();
}

long double
get_comp_counter ()
{
  long double time = get_counter ();
  long double ctime;
  struct tms t;
  clock_t ticks;
  times (&t);
  ticks = t.tms_utime - start_tick;
  ctime = time - ticks * cyc_per_tick;
  return ctime;
}
