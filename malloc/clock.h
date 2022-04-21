#ifndef MALLOC_LAB_CLOCK_H_
#define MALLOC_LAB_CLOCK_H_

/* Routines for using cycle counter */

/* Start the counter */
void start_counter();

/* Get # cycles since counter started */
long double get_counter();

/* Measure overhead for counter */
long double ovhd();

/* Determine clock rate of processor (using a default sleeptime) */
long double mhz(int verbose);

/* Determine clock rate of processor, having more control over accuracy */
long double mhz_full(int verbose, unsigned sleeptime);

/** Special counters that compensate for timer interrupt overhead */

void start_comp_counter();

long double get_comp_counter();

#endif
