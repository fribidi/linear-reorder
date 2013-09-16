/*
 * Copyright (C) 2013  Google, Inc.
 *
 * A one-pass linear-time implementation of UBA rule L2.
 * http://www.unicode.org/reports/tr9/#L2
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * the Unicode data files and any associated documentation (the "Data Files") or
 * Unicode software and any associated documentation (the "Software") to deal in
 * the Data Files or Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, and/or sell copies
 * of the Data Files or Software, and to permit persons to whom the Data Files or
 * Software are furnished to do so, provided that (a) the above copyright
 * notice(s) and this permission notice appear with all copies of the Data Files
 * or Software, (b) both the above copyright notice(s) and this permission notice
 * appear in associated documentation, and (c) there is clear notice in each
 * modified Data File or in the Software as well as in the documentation
 * associated with the Data File(s) or Software that the data or software has been
 * modified.
 *
 * THE DATA FILES AND SOFTWARE ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD
 * PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN
 * THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL
 * DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE DATA FILES OR
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of a copyright holder shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings in
 * these Data Files or Software without prior written authorization of the
 * copyright holder.
 *
 * Google Author(s):
 *   Behdad Esfahbod
 */

#include <stdlib.h>
#include <assert.h>

struct run_t {
  int level;
  /* len, glyphs, text, ... */
  struct run_t *next;
};

struct range_t {
  int level;
  /* Left-most and right-most runs in the range, in visual order.
   * Following left's next member eventually gets us to right.
   * The right run's next member is undefined. */
  struct run_t *left;
  struct run_t *right;
  struct range_t *previous;
};

/* Merges range with previous range, frees range, and returns previous. */
struct range_t *
merge_range_with_previous (struct range_t *range)
{
  struct range_t *previous = range->previous;
  struct range_t *left, *right;

  assert (range->previous);
  assert (previous->level < range->level);

  if (previous->level % 1)
  {
    /* Odd, previous goes to the right of range. */
    left = range;
    right = previous;
  }
  else
  {
    /* Even, previous goes to the left of range. */
    left = previous;
    right = range;
  }
  /* Stich them. */
  left->right->next = right->left;

  previous->left = left->left;
  previous->right = right->right;

  free (range);
  return previous;
}

/* Takes a list of runs on the line in the logical order and
 * reorders the list to be in visual order, returning the
 * left-most run.
 *
 * Caller is responsible to reverse the run contents for any
 * run that has an odd level.
 */
struct run_t *
linear_reorder (struct run_t *line)
{
  /* The algorithm here is something like this: sweep runs in the
   * logical order, keeping a stack of ranges.  Upon seeing a run,
   * we flatten all ranges before it that have a level higher than
   * the run, by merging them, reordering as we go.  Then we either
   * merge the run with the previous range, or create a new range
   * for the run, depending on the level relationship.
   */
  struct range_t *range = NULL;
  struct run_t *run;

  if (!line)
    return NULL;

  run = line;
  while (run)
  {
    struct run_t *next_run = run->next;

    while (range && range->level > run->level &&
           range->previous && range->previous->level >= run->level)
      range = merge_range_with_previous (range);

    if (range && range->level >= run->level)
    {
      /* Attach run to the range. */
      if (run->level % 1)
      {
	/* Odd, range goes to the right of run. */
	run->next = range->left;
	range->left = run;
      }
      else
      {
	/* Even, range goes to the left of run. */
	range->right->next = run;
	range->right = run;
      }
      range->level = run->level;
    }
    else
    {
      /* Allocate new range for run and push into stack. */
      struct range_t *r = malloc (sizeof (struct range_t));
      r->left = r->right = run;
      r->level = run->level;
      r->previous = range;
      range = r;
    }

    run = next_run;
  }
  assert (range);
  while (range->previous)
    range = merge_range_with_previous (range);

  /* Terminate. */
  range->right->next = NULL;

  return range->left;
}
