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
 * \file    nfs_stat_mgmt.c
 * \author  $Author: deniel $
 * \date    $Date: 2005/11/28 17:03:03 $
 * \version $Revision: 1.4 $
 * \brief   routines for managing the nfs statistics.
 *
 * nfs_stat_mgmt.c : routines for managing the nfs statistics.
 *
 * $Header: /cea/S/home/cvs/cvs/SHERPA/BaseCvs/GANESHA/src/support/nfs_stat_mgmt.c,v 1.4 2005/11/28 17:03:03 deniel Exp $
 *
 * $Log: nfs_stat_mgmt.c,v $
 * Revision 1.4  2005/11/28 17:03:03  deniel
 * Added CeCILL headers
 *
 * Revision 1.3  2005/09/30 15:50:19  deniel
 * Support for mount and nfs protocol different from the default
 *
 * Revision 1.2  2005/09/30 14:27:34  deniel
 * Adding some configurationsa items in nfs_core
 *
 * Revision 1.1  2005/08/11 12:37:28  deniel
 * Added statistics management
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
#include "nfs_tools.h"
#include "nfs_proto_tools.h"
#include "nfs_stat.h"

extern nfs_parameter_t nfs_param;

/**
 *
 * nfs_stat_update: Update a client's statistics.
 *
 * Update a client's statistics.
 *
 * @param type    [IN]    type of the stat to dump
 * @param pclient [INOUT] client resource to be used
 * @param preq    [IN]    pointer to SVC request related to this call 
 *
 * @return nothing (void function)
 *
 */
void nfs_stat_update(nfs_stat_type_t type,
                     nfs_request_stat_t * pstat_req, struct svc_req *preq)
{
  nfs_request_stat_item_t *pitem = NULL;

  if(preq->rq_prog == nfs_param.core_param.nfs_program)
    {
      switch (preq->rq_vers)
        {
        case NFS_V2:
          pitem = &pstat_req->stat_req_nfs2[preq->rq_proc];
          pstat_req->nb_nfs2_req += 1;
          break;

        case NFS_V3:
          pitem = &pstat_req->stat_req_nfs3[preq->rq_proc];
          pstat_req->nb_nfs3_req += 1;
          break;

        case NFS_V4:
          pitem = &pstat_req->stat_req_nfs4[preq->rq_proc];
          pstat_req->nb_nfs4_req += 1;

          break;

        default:
          /* Bad vers ? */
          DisplayLog
              ("IMPLEMENTATION ERROR: /!\\ | you should never step here file %s, line %",
               __FILE__, __LINE__);
          return;
          break;
        }
    }
  else if(preq->rq_prog == nfs_param.core_param.mnt_program)
    {
      switch (preq->rq_vers)
        {
        case MOUNT_V1:
          pitem = &pstat_req->stat_req_mnt1[preq->rq_proc];
          pstat_req->nb_mnt1_req += 1;
          break;

        case MOUNT_V3:
          pitem = &pstat_req->stat_req_mnt3[preq->rq_proc];
          pstat_req->nb_mnt3_req += 1;
          break;

        default:
          /* Bad vers ? */
          DisplayLog
              ("IMPLEMENTATION ERROR: /!\\ | you should never step here file %s, line %",
               __FILE__, __LINE__);
          return;
          break;
        }
    }
  else if(preq->rq_prog == nfs_param.core_param.nlm_program)
    {
      switch (preq->rq_vers)
        {
        case NLM4_VERS:
          pitem = &pstat_req->stat_req_nlm4[preq->rq_proc];
          pstat_req->nb_nlm4_req += 1;
          break;
        default:
          /* Bad vers ? */
          DisplayLog
              ("IMPLEMENTATION ERROR: /!\\ | you should never step here file %s, line %s",
               __FILE__, __LINE__);
          return;
          break;
        }
    }

  else
    {
      /* Bad program ? */
      DisplayLog
          ("IMPLEMENTATION ERROR: /!\\ | you should never step here file %s, line %",
           __FILE__, __LINE__);
      return;
    }

  pitem->total += 1;

  switch (type)
    {
    case GANESHA_STAT_SUCCESS:
      pitem->success += 1;
      break;

    case GANESHA_STAT_DROP:
      pitem->dropped += 1;
      break;

    default:
      /* Bad type ? */
      DisplayLog
          ("IMPLEMENTATION ERROR: /!\\ | you should never step here file %s, line %",
           __FILE__, __LINE__);
      break;
    }

  return;

}                               /* nfs_stat_update */