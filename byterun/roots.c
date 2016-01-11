/***********************************************************************/
/*                                                                     */
/*                                OCaml                                */
/*                                                                     */
/*         Xavier Leroy and Damien Doligez, INRIA Rocquencourt         */
/*                                                                     */
/*  Copyright 1996 Institut National de Recherche en Informatique et   */
/*  en Automatique.  All rights reserved.  This file is distributed    */
/*  under the terms of the GNU Library General Public License, with    */
/*  the special exception on linking described in file ../LICENSE.     */
/*                                                                     */
/***********************************************************************/

/* To walk the memory roots for garbage collection */

#include "caml/finalise.h"
#include "caml/globroots.h"
#include "caml/major_gc.h"
#include "caml/memory.h"
#include "caml/minor_gc.h"
#include "caml/misc.h"
#include "caml/mlvalues.h"
#include "caml/roots.h"
#include "caml/fiber.h"

CAMLexport struct caml__roots_block *caml_local_roots = NULL;

CAMLexport void (*caml_scan_roots_hook) (scanning_action f, int) = NULL;

/* FIXME should rename to [caml_oldify_minor_roots] and synchronise with
   asmrun/roots.c */
/* Call [caml_oldify_one] on (at least) all the roots that point to the minor
   heap. */
void caml_oldify_local_roots (void)
{
  struct caml__roots_block *lr;
  intnat i, j;
  value * sp;

  /* The stacks */
  caml_oldify_one (caml_current_stack, &caml_current_stack);

  /* Local C roots */  /* FIXME do the old-frame trick ? */
  for (lr = caml_local_roots; lr != NULL; lr = lr->next) {
    for (i = 0; i < lr->ntables; i++){
      for (j = 0; j < lr->nitems; j++){
        sp = &(lr->tables[i][j]);
        caml_oldify_one (*sp, sp);
      }
    }
  }
  /* Global C roots */
  caml_scan_global_young_roots(&caml_oldify_one);
  /* Finalised values */
  caml_final_do_young_roots (&caml_oldify_one);
  /* Hook */
  if (caml_scan_roots_hook != NULL) (*caml_scan_roots_hook)(&caml_oldify_one, 0);
}


/* Call [caml_darken] on all roots */

void caml_darken_all_roots_start (void)
{
  caml_do_roots (caml_darken, 1, 0);
}

uintnat caml_incremental_roots_count = 1;

intnat caml_darken_all_roots_slice (intnat work)
{
  return work;
}

/* Note, in byte-code there is only one global root, so [do_globals] is
   ignored and [caml_darken_all_roots_slice] does nothing. */
void caml_do_roots (scanning_action f, int do_globals, int is_compaction)
{
  CAML_INSTR_SETUP (tmr, "major_roots");
  /* Global variables */
  f(caml_global_data, &caml_global_data);
  CAML_INSTR_TIME (tmr, "major_roots/global");
  /* The stack and the local C roots */
  caml_do_local_roots(f, caml_local_roots, is_compaction);
  CAML_INSTR_TIME (tmr, "major_roots/local");
  /* Global C roots */
  caml_scan_global_roots(f);
  CAML_INSTR_TIME (tmr, "major_roots/C");
  /* Finalised values */
  caml_final_do_strong_roots (f);
  CAML_INSTR_TIME (tmr, "major_roots/finalised");
  /* Hook */
  if (caml_scan_roots_hook != NULL) (*caml_scan_roots_hook)(f, is_compaction);
  CAML_INSTR_TIME (tmr, "major_roots/hook");
}

CAMLexport void caml_do_local_roots (scanning_action f,
                                     struct caml__roots_block *local_roots,
                                     int is_compaction)
{
  struct caml__roots_block *lr;
  int i, j;
  value * sp;

  if (!is_compaction) caml_scan_stack (f, caml_current_stack);
  f (caml_current_stack, &caml_current_stack);

  for (lr = local_roots; lr != NULL; lr = lr->next) {
    for (i = 0; i < lr->ntables; i++){
      for (j = 0; j < lr->nitems; j++){
        sp = &(lr->tables[i][j]);
        f (*sp, sp);
      }
    }
  }
}
