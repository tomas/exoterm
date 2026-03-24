/*----------------------------------------------------------------------*
 * File:	rxvt.C
 *----------------------------------------------------------------------*
 *
 * All portions of code are copyright by their respective author/s.
 * Copyright (c) 2003-2006 Marc Lehmann <schmorp@schmorp.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *----------------------------------------------------------------------*/

#include "../config.h"
#include "rxvt.h"

int
main(int argc, char** argv)
{
  rxvt_term* term;

  try
  {
    ptytty::init();
    rxvt_init();

    term = new rxvt_term();
    term->init(argc, argv, environ);
    ev_run();

    return EXIT_SUCCESS;
  }
  catch (const class rxvt_failure_exception& e)
  {
    return EXIT_FAILURE;
  }
}
