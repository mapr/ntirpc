/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright CEA/DAM/DIF  (2008)
 * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
 *                Thomas LEIBOVICI  thomas.leibovici@cea.fr
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * ---------------------------------------
 */

/**
 * \file    nfs4_op_putrootfh.c
 * \author  $Author: deniel $
 * \date    $Date: 2005/12/20 10:52:14 $
 * \version $Revision: 1.13 $
 * \brief   Routines used for managing the NFS4_OP_PUTROOTFH operation. 
 *
 * nfs4_op_restorefh.c : Routines used for managing the NFS4_OP_PUTROOTFH operation.
 * 
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _SOLARIS
#include "solaris_port.h"
#endif

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/file.h>           /* for having FNDELAY */
#include "HashData.h"
#include "HashTable.h"
#ifdef _USE_GSSRPC
#include <gssrpc/types.h>
#include <gssrpc/rpc.h>
#include <gssrpc/auth.h>
#include <gssrpc/pmap_clnt.h>
#else
#include <rpc/types.h>
#include <rpc/rpc.h>
#include <rpc/auth.h>
#include <rpc/pmap_clnt.h>
#endif

#include "log_functions.h"
#include "stuff_alloc.h"
#include "nfs23.h"
#include "nfs4.h"
#include "mount.h"
#include "nfs_core.h"
#include "cache_inode.h"
#include "cache_content.h"
#include "nfs_exports.h"
#include "nfs_creds.h"
#include "nfs_proto_functions.h"
#include "nfs_file_handle.h"
#include "nfs_tools.h"

/**
 *
 * CreateROOTFH4: create the pseudo fs root filehandle .
 *
 * Creates the pseudo fs root filehandle .
 * 
 * @param fh   [OUT]   the file handle to be built.
 * @param data [INOUT] request compound data
 *
 * @return NFS4_OK is successful, NFS4ERR_BADHANDLE otherwise (for an error).
 *
 * @see nfs4_op_putrootfh
 *
 */

int CreateROOTFH4(nfs_fh4 * fh, compound_data_t * data)
{
  pseudofs_entry_t psfsentry;
  int status = 0;
#ifdef _DEBUG_NFS_V4
  char fhstr[LEN_FH_STR];
#endif

  psfsentry = *(data->pseudofs->reverse_tab[0]);

  if((status = nfs4_AllocateFH(&(data->rootFH))) != NFS4_OK)
    return status;

  if(!nfs4_PseudoToFhandle(&(data->rootFH), &psfsentry))
    {
      return NFS4ERR_BADHANDLE;
    }

  /* Test */
#ifdef _DEBUG_NFS_V4
  nfs4_sprint_fhandle(&data->rootFH, fhstr);
  DisplayLog("CREATE ROOTFH: %s", fhstr);
#endif

  return NFS4_OK;
}                               /* CreateROOTFH4 */

/**
 *
 * nfs4_op_putroothfh: The NFS4_OP_PUTROOTFH operation.
 *
 * This functions handles the NFS4_OP_PUTROOTFH operation in NFSv4. This function can be called only from nfs4_Compound. 
 *
 * @param op    [IN]    pointer to nfs4_op arguments
 * @param data  [INOUT] Pointer to the compound request's data
 * @param resp  [IN]    Pointer to nfs4_op results
 *
 * @return NFS4_OK if successfull, other values show an error. 
 *
 * @see CreateROOTFH4
 *
 */

#define arg_PUTROOTFH4 op->nfs_argop4_u.opputrootfh
#define res_PUTROOTFH4 resp->nfs_resop4_u.opputrootfh

int nfs4_op_putrootfh(struct nfs_argop4 *op,
                      compound_data_t * data, struct nfs_resop4 *resp)
{
  int error;

  char __attribute__ ((__unused__)) funcname[] = "nfs4_op_putrootfh";
#ifdef _DEBUG_NFS_V4
  char fhstr[LEN_FH_STR];
#endif

  /* This NFS4 Operation has no argument, it just get then ROOTFH (replace MOUNTPROC3_MNT) */

  /* First of all, set the reply to zero to make sure it contains no parasite information */
  memset(resp, 0, sizeof(struct nfs_resop4));

  resp->resop = NFS4_OP_PUTROOTFH;
  resp->nfs_resop4_u.opputrootfh.status = NFS4_OK;

  if((error = CreateROOTFH4(&(data->rootFH), data)) != NFS4_OK)
    {
      res_PUTROOTFH4.status = error;
      return res_PUTROOTFH4.status;
    }
  data->current_entry = NULL;   /* No cache inode entry for the directory within pseudo fs */
  data->current_filetype = DIR_BEGINNING;       /* Only directory in the pseudo fs */

  /* I copy the root FH to the currentFH and, if not already done, to the publicFH */
  /* For the moment, I choose to have rootFH = publicFH */
  /* For initial mounted_on_FH, I'll use the rootFH, this will change at junction traversal */
  if(data->currentFH.nfs_fh4_len == 0)
    {
      if((error = nfs4_AllocateFH(&(data->currentFH))) != NFS4_OK)
        {
          resp->nfs_resop4_u.opputrootfh.status = error;
          return error;
        }
    }
  memcpy((char *)(data->currentFH.nfs_fh4_val), (char *)(data->rootFH.nfs_fh4_val),
         data->rootFH.nfs_fh4_len);
  data->currentFH.nfs_fh4_len = data->rootFH.nfs_fh4_len;

  if(data->mounted_on_FH.nfs_fh4_len == 0)
    {
      if((error = nfs4_AllocateFH(&(data->mounted_on_FH))) != NFS4_OK)
        {
          resp->nfs_resop4_u.opputrootfh.status = error;
          return error;
        }
    }
  memcpy((char *)(data->mounted_on_FH.nfs_fh4_val), (char *)(data->rootFH.nfs_fh4_val),
         data->rootFH.nfs_fh4_len);
  data->mounted_on_FH.nfs_fh4_len = data->rootFH.nfs_fh4_len;

  if(data->publicFH.nfs_fh4_len == 0)
    {
      if((error = nfs4_AllocateFH(&(data->publicFH))) != NFS4_OK)
        {
          resp->nfs_resop4_u.opputrootfh.status = error;
          return error;
        }
    }
  /* Copy the data where they are supposed to be */
  memcpy((char *)(data->publicFH.nfs_fh4_val), (char *)(data->rootFH.nfs_fh4_val),
         data->rootFH.nfs_fh4_len);
  data->publicFH.nfs_fh4_len = data->rootFH.nfs_fh4_len;

  /* Test */
#ifdef _DEBUG_NFS_V4
  nfs4_sprint_fhandle(&data->rootFH, fhstr);
  DisplayLog("NFS4 PUTROOTFH: rootFH=%s", fhstr);
  nfs4_sprint_fhandle(&data->currentFH, fhstr);
  DisplayLog("NFS4 PUTROOTFH: currentFH=%s", fhstr);
#endif

  DisplayLogJdLevel(data->pclient->log_outputs,
                    NIV_FULL_DEBUG, "NFS4 PUTROOTFH: Ending on status %d",
                    resp->nfs_resop4_u.opputrootfh.status);

  return resp->nfs_resop4_u.opputrootfh.status;
}                               /* nfs4_op_putrootfh */

/**
 * nfs4_op_putrootfh_Free: frees what was allocared to handle nfs4_op_putrootfh.
 * 
 * Frees what was allocared to handle nfs4_op_putrootfh.
 *
 * @param resp  [INOUT]    Pointer to nfs4_op results
 *
 * @return nothing (void function )
 * 
 */
void nfs4_op_putrootfh_Free(PUTROOTFH4res * resp)
{
  /* Nothing to be done */
  return;
}                               /* nfs4_op_putrootfh_Free */