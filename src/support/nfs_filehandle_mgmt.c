/*
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
 * \file    nfs_filehandle_mgmt.c
 * \author  $Author: deniel $
 * \date    $Date: 2006/01/24 11:43:05 $
 * \version $Revision: 1.12 $
 * \brief   Some tools for managing the file handles. 
 *
 * nfs_filehandle_mgmt.c : Some tools for managing the file handles.
 *
 * $Header: /cea/S/home/cvs/cvs/SHERPA/BaseCvs/GANESHA/src/support/nfs_filehandle_mgmt.c,v 1.12 2006/01/24 11:43:05 deniel Exp $
 *
 * $Log: nfs_filehandle_mgmt.c,v $
 * Revision 1.12  2006/01/24 11:43:05  deniel
 * Code cleaning in progress
 *
 * Revision 1.11  2006/01/11 08:12:18  deniel
 * Added bug track and warning for badly formed handles
 *
 * Revision 1.10  2005/11/28 17:03:03  deniel
 * Added CeCILL headers
 *
 * Revision 1.9  2005/09/07 08:58:30  deniel
 * NFSv2 FH was only 31 byte long instead of 32
 *
 * Revision 1.8  2005/09/07 08:16:07  deniel
 * The checksum is filled with zeros before being computed to avoid 'dead beef' values
 *
 * Revision 1.7  2005/08/11 12:37:28  deniel
 * Added statistics management
 *
 * Revision 1.6  2005/08/09 12:35:37  leibovic
 * setting file_handle to 0 in nfs3_FSALToFhandle, before writting into it.
 *
 * Revision 1.5  2005/08/08 14:09:25  leibovic
 * setting checksum to 0 before writting in it.
 *
 * Revision 1.4  2005/08/04 08:34:32  deniel
 * memset management was badly made
 *
 * Revision 1.3  2005/08/03 13:23:43  deniel
 * Possible incoherency in CVS or in Emacs
 *
 * Revision 1.2  2005/08/03 13:13:59  deniel
 * memset to zero before building the filehandles
 *
 * Revision 1.1  2005/08/03 06:57:54  deniel
 * Added a libsupport for miscellaneous service functions
 *
 * Revision 1.4  2005/07/28 12:26:57  deniel
 * NFSv3 PROTOCOL Ok
 *
 * Revision 1.3  2005/07/26 07:39:15  deniel
 * Integration of NFSv2/NFSv3 In progress
 *
 * Revision 1.2  2005/07/21 09:18:42  deniel
 * Structure of the file handles was redefined
 *
 * Revision 1.1  2005/07/20 12:56:54  deniel
 * Reorganisation of the source files
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
#include <sys/types.h>
#include <ctype.h>              /* for having isalnum */
#include <stdlib.h>             /* for having atoi */
#include <dirent.h>             /* for having MAXNAMLEN */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/file.h>           /* for having FNDELAY */
#include <pwd.h>
#include <grp.h>
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
#include "nfs_core.h"
#include "nfs23.h"
#include "nfs4.h"
#include "fsal.h"
#include "nfs_tools.h"
#include "nfs_exports.h"
#include "nfs_file_handle.h"

extern time_t ServerBootTime;
extern nfs_parameter_t nfs_param;

/**
 *
 *  nfs4_FhandleToFSAL: converts a nfs4 file handle to a FSAL file handle.
 *
 * Converts a nfs4 file handle to a FSAL file handle.
 *
 * @param pfh4 [IN] pointer to the file handle to be converted
 * @param pfsalhandle [OUT] pointer to the extracted FSAL handle
 *
 * @return 1 if successful, 0 otherwise
 *
 */
int nfs4_FhandleToFSAL(nfs_fh4 * pfh4, fsal_handle_t * pfsalhandle,
                       fsal_op_context_t * pcontext)
{
  fsal_status_t fsal_status;
  file_handle_v4_t *pfile_handle;
  unsigned long long checksum;

#ifdef _DEBUG_FILEHANDLE
  print_fhandle4(*pfh4);
#endif

  /* Verify the len */
  if(pfh4->nfs_fh4_len != sizeof(file_handle_v4_t))
    return 0;                   /* Corrupted FH */

  /* Cast the fh as a non opaque structure */
  pfile_handle = (file_handle_v4_t *) (pfh4->nfs_fh4_val);

  /* The filehandle should not be related to pseudo fs */
  if(pfile_handle->pseudofs_id != 0 || pfile_handle->pseudofs_flag != FALSE)
    return 0;                   /* Bad FH */

  /* Fill in the fs opaque part */
  fsal_status =
      FSAL_ExpandHandle(pcontext->export_context, FSAL_DIGEST_NFSV4,
                        (caddr_t) & (pfile_handle->fsopaque), pfsalhandle);
  if(FSAL_IS_ERROR(fsal_status))
    return 0;                   /* Corrupted (or stale) FH */
#ifdef _DEBUG_FILEHANDLE
  print_buff((char *)pfsalhandle, sizeof(fsal_handle_t));
#endif
  return 1;
}                               /* nfs4_FhandleToFSAL */

/**
 *
 *  nfs3_FhandleToFSAL: converts a nfs3 file handle to a FSAL file handle.
 *
 * Converts a nfs3 file handle to a FSAL file handle.
 *
 * @param pfh3 [IN] pointer to the file handle to be converted
 * @param pfsalhandle [OUT] pointer to the extracted FSAL handle
 *
 * @return 1 if successful, 0 otherwise
 *
 */
int nfs3_FhandleToFSAL(nfs_fh3 * pfh3, fsal_handle_t * pfsalhandle,
                       fsal_op_context_t * pcontext)
{
  fsal_status_t fsal_status;
  file_handle_v3_t *pfile_handle;
  unsigned long long checksum;
#ifdef _DEBUG_FILEHANDLE
  print_fhandle3(*pfh3);
#endif
  /* Verify the len */
  if(pfh3->data.data_len != sizeof(file_handle_v3_t))
    return 0;                   /* Corrupted FH */

  /* Cast the fh as a non opaque structure */
  pfile_handle = (file_handle_v3_t *) (pfh3->data.data_val);

  /* Check the cksum */
  memcpy((char *)&checksum, pfile_handle->checksum, sizeof(checksum));
  if(checksum != nfs3_FhandleCheckSum(pfile_handle))
    {
      DisplayLog
          ("Invalid checksum in NFSv3 handle. checksum from handle:%llX, expected:%llX",
           checksum, nfs3_FhandleCheckSum(pfile_handle));
      return 0;                 /* Corrupted FH */
    }

  /* Fill in the fs opaque part */
  fsal_status =
      FSAL_ExpandHandle(pcontext->export_context, FSAL_DIGEST_NFSV3,
                        (caddr_t) & (pfile_handle->fsopaque), pfsalhandle);
  if(FSAL_IS_ERROR(fsal_status))
    return 0;                   /* Corrupted FH */

#ifdef _DEBUG_FILEHANDLE
  print_buff((char *)pfsalhandle, sizeof(fsal_handle_t));
#endif
  return 1;
}                               /* nfs3_FhandleToFSAL */

/**
 *
 *  nfs2_FhandleToFSAL: converts a nfs2 file handle to a FSAL file handle.
 *
 * Converts a nfs2 file handle to a FSAL file handle.
 *
 * @param pfh2 [IN] pointer to the file handle to be converted
 * @param pfsalhandle [OUT] pointer to the extracted FSAL handle
 *
 * @return 1 if successful, 0 otherwise
 *
 */
int nfs2_FhandleToFSAL(fhandle2 * pfh2, fsal_handle_t * pfsalhandle,
                       fsal_op_context_t * pcontext)
{
  fsal_status_t fsal_status;
  file_handle_v2_t *pfile_handle;

  /* Cast the fh as a non opaque structure */
  pfile_handle = (file_handle_v2_t *) pfh2;
#ifdef _DEBUG_FILEHANDLE
  print_fhandle2(*pfh2);
#endif
  /* Check the cksum */
  if(pfile_handle->checksum != nfs2_FhandleCheckSum(pfile_handle))
    return 0;                   /* Corrupted FH */

  /* Fill in the fs opaque part */
  fsal_status =
      FSAL_ExpandHandle(pcontext->export_context, FSAL_DIGEST_NFSV2,
                        (caddr_t) & (pfile_handle->fsopaque), pfsalhandle);
  if(FSAL_IS_ERROR(fsal_status))
    return 0;                   /* Corrupted FH */

#ifdef _DEBUG_FILEHANDLE
  print_buff((char *)pfsalhandle, sizeof(fsal_handle_t));
#endif
  return 1;
}                               /* nfs2_FhandleToFSAL */

/**
 *
 *  nfs4_FSALToFhandle: converts a FSAL file handle to a nfs4 file handle.
 *
 * Converts a nfs4 file handle to a FSAL file handle.
 *
 * @param pfh4 [OUT] pointer to the extracted file handle 
 * @param pfsalhandle [IN] pointer to the FSAL handle to be converted
 * @param data [IN] pointer to NFSv4 compound data structure. 
 *
 * @return 1 if successful, 0 otherwise
 *
 */
int nfs4_FSALToFhandle(nfs_fh4 * pfh4, fsal_handle_t * pfsalhandle,
                       compound_data_t * data)
{
  fsal_status_t fsal_status;
  file_handle_v4_t file_handle;

  /* zero-ification of the buffer to be used as handle */
  memset(pfh4->nfs_fh4_val, 0, sizeof(file_handle_v4_t));

  /* Fill in the fs opaque part */
  fsal_status =
      FSAL_DigestHandle(&data->pexport->FS_export_context, FSAL_DIGEST_NFSV4, pfsalhandle,
                        (caddr_t) & file_handle.fsopaque);
  if(FSAL_IS_ERROR(fsal_status))
    return 0;

  /* keep track of the export id */
  file_handle.exportid = data->pexport->id;

  /* No Pseudo fs here */
  file_handle.pseudofs_id = 0;
  file_handle.pseudofs_flag = FALSE;
  file_handle.ds_flag = 0;
  file_handle.refid = 0;

  /* if FH expires, set it there */
  if(nfs_param.nfsv4_param.fh_expire == TRUE)
    {
      printf("Un fh expirable a ete cree\n");
      file_handle.srvboot_time = ServerBootTime;
    }
  else
    {
      /* Non expirable FH */
      file_handle.srvboot_time = 0;
    }

  /* Set the last byte */
  file_handle.xattr_pos = 0;

  /* Set the len */
  pfh4->nfs_fh4_len = sizeof(file_handle_v4_t);

  /* Set the data */
  memcpy(pfh4->nfs_fh4_val, &file_handle, sizeof(file_handle_v4_t));

  return 1;
}                               /* nfs4_FSALToFhandle */

/**
 *
 *  nfs3_FSALToFhandle: converts a FSAL file handle to a nfs3 file handle.
 *
 * Converts a nfs3 file handle to a FSAL file handle.
 *
 * @param pfh3 [OUT] pointer to the extracted file handle 
 * @param pfsalhandle [IN] pointer to the FSAL handle to be converted
 * @param pexport [IN] pointer to the export list entry the FH belongs to
 *
 * @return 1 if successful, 0 otherwise
 *
 */
int nfs3_FSALToFhandle(nfs_fh3 * pfh3, fsal_handle_t * pfsalhandle,
                       exportlist_t * pexport)
{
  fsal_status_t fsal_status;
  file_handle_v3_t file_handle;
  unsigned long long cksum;

#ifdef _DEBUG_FILEHANDLE
  print_buff((char *)pfsalhandle, sizeof(fsal_handle_t));
#endif

  /* zero-ification of the buffer to be used as handle */
  memset(pfh3->data.data_val, 0, NFS3_FHSIZE);
  memset((caddr_t) & file_handle, 0, sizeof(file_handle_v3_t));

  /* Fill in the fs opaque part */
  fsal_status =
      FSAL_DigestHandle(&pexport->FS_export_context, FSAL_DIGEST_NFSV3, pfsalhandle,
                        (caddr_t) & file_handle.fsopaque);
  if(FSAL_IS_ERROR(fsal_status))
    return 0;

  /* keep track of the export id */
  file_handle.exportid = pexport->id;

  /* Set the last byte */
  file_handle.xattr_pos = 0;

  /* A very basic checksum */
  memset((char *)&file_handle.checksum, 0, 16); /* NFSv3 checksum is 16 bytes long */
  cksum = nfs3_FhandleCheckSum(&file_handle);
  memcpy((char *)&file_handle.checksum, &cksum, sizeof(cksum));

  /* Set the len */
  pfh3->data.data_len = sizeof(file_handle_v3_t);

  /* Set the data */
  memcpy(pfh3->data.data_val, &file_handle, sizeof(file_handle_v3_t));

#ifdef _DEBUG_FILEHANDLE
  print_fhandle3(*pfh3);
#endif

  return 1;
}                               /* nfs3_FSALToFhandle */

/**
 *
 *  nfs2_FSALToFhandle: converts a FSAL file handle to a nfs2 file handle.
 *
 * Converts a nfs2 file handle to a FSAL file handle.
 *
 * @param pfh2 [OUT] pointer to the extracted file handle 
 * @param pfsalhandle [IN] pointer to the FSAL handle to be converted
 * @param pfsalhandle [IN] pointer to the FSAL handle to be converted
 *
 * @return 1 if successful, 0 otherwise
 *
 */
int nfs2_FSALToFhandle(fhandle2 * pfh2, fsal_handle_t * pfsalhandle,
                       exportlist_t * pexport)
{
  fsal_status_t fsal_status;
  file_handle_v2_t file_handle;
  unsigned int checksum;
#ifdef _DEBUG_FILEHANDLE
  print_buff((char *)pfsalhandle, sizeof(fsal_handle_t));
#endif

  /* zero-ification of the buffer to be used as handle */
  memset(pfh2, 0, NFS2_FHSIZE);

  /* Fill in the fs opaque part */
  fsal_status =
      FSAL_DigestHandle(&pexport->FS_export_context, FSAL_DIGEST_NFSV2, pfsalhandle,
                        (caddr_t) & file_handle.fsopaque);
  if(FSAL_IS_ERROR(fsal_status))
    return 0;

  /* keep track of the export id */
  file_handle.exportid = pexport->id;

  /* A very basic checksum */
  checksum = nfs2_FhandleCheckSum(&file_handle);
  memcpy((char *)&file_handle.checksum, &checksum, sizeof(checksum));

  /* Set the last byte */
  file_handle.xattr_pos = 0;

  /* Set the data */
  memcpy((caddr_t) pfh2, &file_handle, sizeof(file_handle_v2_t));

#ifdef _DEBUG_FILEHANDLE
  print_fhandle2(*pfh2);
#endif
  return 1;
}                               /* nfs2_FSALToFhandle */

/**
 *
 * nfs4_FhandleToExportId
 *
 * This routine extracts the export id from the file handle NFSv4
 *
 * @param pfh4 [IN] file handle to manage.
 * 
 * @return the export id.
 *
 */
short nfs4_FhandleToExportId(nfs_fh4 * pfh4)
{
  file_handle_v4_t *pfile_handle = NULL;

  pfile_handle = (file_handle_v4_t *) (pfh4->nfs_fh4_val);

  if(pfile_handle == NULL)
    return -1;                  /* Badly formed arguments */

  return pfile_handle->exportid;
}                               /* nfs4_FhandleToExportId */

/**
 *
 * nfs3_FhandleToExportId
 *
 * This routine extracts the export id from the file handle NFSv3
 *
 * @param pfh3 [IN] file handle to manage.
 * 
 * @return the export id.
 *
 */
short nfs3_FhandleToExportId(nfs_fh3 * pfh3)
{
  file_handle_v3_t *pfile_handle = NULL;

  pfile_handle = (file_handle_v3_t *) (pfh3->data.data_val);

  if(pfile_handle == NULL)
    return -1;                  /* Badly formed argument */

#ifdef _DEBUG_FILEHANDLE
  print_buff((char *)pfh3->data.data_val, pfh3->data.data_len);
#endif
  return pfile_handle->exportid;
}                               /* nfs3_FhandleToExportId */

/**
 *
 * nfs2_FhandleToExportId
 *
 * This routine extracts the export id from the file handle NFSv2
 *
 * @param pfh2 [IN] file handle to manage.
 * 
 * @return the export id.
 *
 */
short nfs2_FhandleToExportId(fhandle2 * pfh2)
{
  file_handle_v2_t *pfile_handle = NULL;

  pfile_handle = (file_handle_v2_t *) (*pfh2);

  if(pfile_handle == NULL)
    return -1;                  /* Badly formed argument */

  return pfile_handle->exportid;
}                               /* nfs2_FhandleToExportId */

/**
 *    
 * nfs4_Is_Fh_Xattr
 * 
 * This routine is used to test is a fh refers to a Xattr related stuff
 *
 * @param pfh [IN] file handle to test.
 * 
 * @return TRUE if in pseudo fh, FALSE otherwise 
 *
 */
int nfs3_Is_Fh_Xattr(nfs_fh3 * pfh)
{
  file_handle_v3_t *pfhandle3;

  if(pfh == NULL)
    return 0;

  pfhandle3 = (file_handle_v3_t *) (pfh->data.data_val);

  return (pfhandle3->xattr_pos != 0) ? 1 : 0;
}                               /* nfs4_Is_Fh_Xattr */

/**
 *
 * nfs4_Is_Fh_Empty
 *
 * This routine is used to test if a fh is empty (contains no data).
 *
 * @param pfh [IN] file handle to test.
 * 
 * @return NFS4_OK if successfull, NFS4ERR_NOFILEHANDLE is fh is empty.  
 *
 */
int nfs4_Is_Fh_Empty(nfs_fh4 * pfh)
{
  if(pfh == NULL)
    return NFS4ERR_NOFILEHANDLE;

  if(pfh->nfs_fh4_len == 0)
    return NFS4ERR_NOFILEHANDLE;

  return 0;
}                               /* nfs4_Is_Fh_Empty */

/**
 *  
 *  nfs4_Is_Fh_Xattr
 *
 *  This routine is used to test is a fh refers to a Xattr related stuff
 *
 * @param pfh [IN] file handle to test.
 * 
 * @return TRUE if in pseudo fh, FALSE otherwise 
 *
 */
int nfs4_Is_Fh_Xattr(nfs_fh4 * pfh)
{
  file_handle_v4_t *pfhandle4;

  if(pfh == NULL)
    return 0;

  pfhandle4 = (file_handle_v4_t *) (pfh->nfs_fh4_val);

  return (pfhandle4->xattr_pos != 0) ? 1 : 0;
}                               /* nfs4_Is_Fh_Xattr */

/**
 *
 * nfs4_Is_Fh_Pseudo
 *
 * This routine is used to test if a fh refers to pseudo fs
 *
 * @param pfh [IN] file handle to test.
 * 
 * @return TRUE if in pseudo fh, FALSE otherwise 
 *
 */
int nfs4_Is_Fh_Pseudo(nfs_fh4 * pfh)
{
  file_handle_v4_t *pfhandle4;

  if(pfh == NULL)
    return 0;

  pfhandle4 = (file_handle_v4_t *) (pfh->nfs_fh4_val);

  return pfhandle4->pseudofs_flag;
}                               /* nfs4_Is_Fh_Pseudo */

/**
 *
 * nfs4_Is_Fh_Expired
 *
 * This routine is used to test if a fh is expired
 *
 * @param pfh [IN] file handle to test.
 * 
 * @return NFS4_OK if successfull. All the FH are persistent for now. 
 *
 */
int nfs4_Is_Fh_Expired(nfs_fh4 * pfh)
{
  file_handle_v4_t *pfilehandle4;

  if(pfh == NULL)
    return NFS4ERR_BADHANDLE;

  pfilehandle4 = (file_handle_v4_t *) pfh;

  if((nfs_param.nfsv4_param.fh_expire == TRUE)
     && (pfilehandle4->srvboot_time != (unsigned int)ServerBootTime))
    {
      if(nfs_param.nfsv4_param.returns_err_fh_expired == TRUE)
        return NFS4ERR_FHEXPIRED;
    }

  return NFS4_OK;
}                               /* nfs4_Is_Fh_Expired */

/**
 *
 * nfs4_Is_Fh_Invalid
 *
 * This routine is used to test if a fh is invalid.
 *
 * @param pfh [IN] file handle to test.
 * 
 * @return NFS4_OK if successfull. 
 *
 */
int nfs4_Is_Fh_Invalid(nfs_fh4 * pfh)
{
  if(pfh == NULL)
    return NFS4ERR_BADHANDLE;

  if(pfh->nfs_fh4_len > sizeof(file_handle_v4_t))
    return NFS4ERR_BADHANDLE;

  if(pfh->nfs_fh4_val == NULL)
    return NFS4ERR_BADHANDLE;

  return NFS4_OK;
}                               /* nfs4_Is_Fh_Invalid */

/**
 * 
 * nfs4_Is_Fh_Referral
 *
 * This routine is used to identify fh related to a pure referral
 *
 * @param pfh [IN] file handle to test.
 *
 * @return TRUE is fh is a referral, FALSE otherwise
 *
 */
int nfs4_Is_Fh_Referral(nfs_fh4 * pfh)
{
  file_handle_v4_t *pfhandle4;

  if(pfh == NULL)
    return 0;

  pfhandle4 = (file_handle_v4_t *) (pfh->nfs_fh4_val);

  /* Referrals are fh whose pseudofs_id is set without pseudofs_flag set */
  if(pfhandle4->refid > 0)
    {
      return TRUE;
    }

  return FALSE;
}                               /* nfs4_Is_Fh_Referral */

/**
 *
 * nfs2_FhandleCheckSum
 *
 * Computes the checksum associated with a nfsv2 file handle.
 *
 * @param pfh [IN] pointer to the file handle whose checksum is to be computed.
 *
 * @return the computed checksum.
 *
 */
unsigned int nfs2_FhandleCheckSum(file_handle_v2_t * pfh)
{
  unsigned int cksum;

  cksum =
      (unsigned int)pfh->exportid + (unsigned int)pfh->fsopaque[1] +
      (unsigned int)pfh->fsopaque[3] + (unsigned int)pfh->fsopaque[5];

  return cksum;
}                               /* nfs2_FhandleCheckSum */

/**
 *
 * nfs3_FhandleCheckSum
 *
 * Computes the checksum associated with a nfsv3 file handle.
 *
 * @param pfh [IN] pointer to the file handle whose checksum is to be computed.
 *
 * @return the computed checksum.
 *
 */
unsigned long long nfs3_FhandleCheckSum(file_handle_v3_t * pfh)
{
  unsigned long long cksum;

  cksum =
      (unsigned long long)pfh->exportid + (unsigned long long)pfh->fsopaque[1] +
      (unsigned long long)pfh->fsopaque[3] + (unsigned long long)pfh->fsopaque[5];

  return cksum;
}                               /* nfs3_FhandleCheckSum */

/**
 *
 * nfs4_FhandleCheckSum
 *
 * Computes the checksum associated with a nfsv4 file handle.
 *
 * @param pfh [IN] pointer to the file handle whose checksum is to be computed.
 *
 * @return the computed checksum.
 *
 */
unsigned long long nfs4_FhandleCheckSum(file_handle_v4_t * pfh)
{
  unsigned long long cksum;

  cksum =
      (unsigned long long)pfh->exportid + (unsigned long long)pfh->fsopaque[1] +
      (unsigned long long)pfh->fsopaque[3] + (unsigned long long)pfh->fsopaque[5];

  return cksum;
}                               /* nfs4_FhandleCheckSum */

/**
 *
 * print_fhandle2
 *
 * This routine prints a NFSv2 file handle (for debugging purpose)
 *
 * @param fh [IN] file handle to print.
 * 
 * @return nothing (void function).
 *
 */
void print_fhandle2(fhandle2 fh)
{
  unsigned int i = 0;

  printf("File Handle V2: ");
  for(i = 0; i < 32; i++)
    printf("%02X", fh[i]);
  printf("\n");
}                               /* print_fhandle2 */

void sprint_fhandle2(char *str, fhandle2 fh)
{
  unsigned int i = 0;

  sprintf(str, "File Handle V2: ");
  for(i = 0; i < 32; i++)
    sprintf(str, "%02X", fh[i]);
}                               /* sprint_fhandle2 */

/**
 *
 * print_fhandle3
 *
 * This routine prints a NFSv3 file handle (for debugging purpose)
 *
 * @param fh [IN] file handle to print.
 * 
 * @return nothing (void function).
 *
 */
void print_fhandle3(nfs_fh3 fh)
{
  unsigned int i = 0;

  printf("  File Handle V3 : Len=%u ", fh.data.data_len);
  for(i = 0; i < fh.data.data_len; i++)
    printf("%02X", fh.data.data_val[i]);
  printf("\n");
}                               /* print_fhandle3 */

void sprint_fhandle3(char *str, nfs_fh3 fh)
{
  unsigned int i = 0;

  sprintf(str, "  File Handle V3 : Len=%u ", fh.data.data_len);
  for(i = 0; i < fh.data.data_len; i++)
    sprintf(str, "%02X", fh.data.data_val[i]);
}                               /* sprint_fhandle3 */

/**
 *
 * print_fhandle4
 *
 * This routine prints a NFSv4 file handle (for debugging purpose)
 *
 * @param fh [IN] file handle to print.
 * 
 * @return nothing (void function).
 *
 */
void print_fhandle4(nfs_fh4 fh)
{
  unsigned int i = 0;

  printf("  File Handle V4 : Len=%u ", fh.nfs_fh4_len);
  for(i = 0; i < fh.nfs_fh4_len; i++)
    printf("%02X", fh.nfs_fh4_val[i]);
  printf("\n");
}                               /* print_fhandle4 */

void sprint_fhandle4(char *str, nfs_fh4 fh)
{
  unsigned int i = 0;

  sprintf(str, "  File Handle V4 : Len=%u ", fh.nfs_fh4_len);
  for(i = 0; i < fh.nfs_fh4_len; i++)
    sprintf(str, "%02X", fh.nfs_fh4_val[i]);
}                               /* sprint_fhandle4 */

/**
 *
 * print_buff
 *
 * This routine prints the content of a buffer.
 *
 * @param buff [IN] buffer to print.
 * @param len  [IN] length of the buffer.
 * 
 * @return nothing (void function).
 *
 */
void print_buff(char *buff, int len)
{
  int i = 0;

  printf("  Len=%u Buff=%p Val: ", len, buff);
  for(i = 0; i < len; i++)
    printf("%02X", buff[i]);
  printf("\n");
}                               /* print_buff */

void sprint_buff(char *str, char *buff, int len)
{
  int i = 0;

  sprintf(str, "  Len=%u Buff=%p Val: ", len, buff);
  for(i = 0; i < len; i++)
    sprintf(str, "%02X", buff[i]);
}                               /* sprint_buff */

/**
 *
 * print_compound_fh
 *
 * This routine prints all the file handle within a compoud request's data structure.
 * 
 * @param data [IN] compound's data to manage.
 *
 * @return nothing (void function).
 *
 */
void print_compound_fh(compound_data_t * data)
{
  printf("Current FH  ");
  print_fhandle4(data->currentFH);

  printf("Saved FH    ");
  print_fhandle4(data->savedFH);

  printf("Public FH   ");
  print_fhandle4(data->publicFH);

  printf("Root FH     ");
  print_fhandle4(data->rootFH);
}                               /* print_compoud_fh */

/**
 * nfs4_sprint_fhandle : converts a file handle v4 to a string.
 *
 * Converts a file handle v4 to a string. This will be used mostly for debugging purpose. 
 * 
 * @param fh4p [OUT]   pointer to the file handle to be converted to a string.
 * @param data [INOUT] pointer to the char * resulting from the operation.
 * 
 * @return nothing (void function).
 *
 */

void nfs4_sprint_fhandle(nfs_fh4 * fh4p, char *outstr)
{
  unsigned int i = 0;
  char tmpstr[LEN_FH_STR];

  sprintf(outstr, "File handle V4 = { Length = %d  Val = ", fh4p->nfs_fh4_len);

  for(i = 0; i < fh4p->nfs_fh4_len; i++)
    {
      sprintf(&(tmpstr[i * 2]), "%02X", (unsigned char)fh4p->nfs_fh4_val[i]);
    }

  sprintf(outstr, "File handle V4 = { Length = %d  Val = %s }", fh4p->nfs_fh4_len,
          tmpstr);

}                               /* nfs4_sprint_fhandle */