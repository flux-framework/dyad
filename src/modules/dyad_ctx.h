/*****************************************************************************\
 *  Copyright (c) 2018 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
\*****************************************************************************/

#ifndef DYAD_MOD_CTX_H
#define DYAD_MOD_CTX_H

#include <stdbool.h>
#include <flux/core.h>
#include "dyad_cpa.h"

struct dyad_mod_ctx {
    flux_t *h;
    bool debug;
    flux_msg_handler_t **handlers;
    const char *dyad_path;
  #if DYAD_PROFILE
    double t_sync_io_mod;  // Local file IO cost used
    double t_rpc_mod; // Total time to send files to consumer
  #endif //DYAD_PROFILE
  #if DYAD_CPA
    dyad_cpa_t *t_rec;
    unsigned int t_rec_capacity;
    unsigned int t_rec_size;
    FILE* cpa_outfile;
  #endif // DYAD_CPA
} dyad_mod_ctx_default =
  #if DYAD_PROFILE
    {NULL, false, NULL, NULL, 0.0, 0.0};
  #else
    {NULL, false, NULL, NULL};
  #endif //DYAD_PROFILE

typedef struct dyad_mod_ctx dyad_mod_ctx_t;

#endif // DYAD_MOD_CTX_H
