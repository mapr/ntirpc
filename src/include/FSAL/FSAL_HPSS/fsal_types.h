/*
 *
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
 * \file    fsal_types.h
 * \author  $Author: leibovic $
 * \date    $Date: 2006/02/08 12:45:27 $
 * \version $Revision: 1.19 $
 * \brief   File System Abstraction Layer types and constants.
 *
 *
 *
 */

#ifndef _FSAL_TYPES_SPECIFIC_H
#define _FSAL_TYPES_SPECIFIC_H

/*
 * FS relative includes
 */
#include <hpss_version.h>

#if HPSS_MAJOR_VERSION == 5

#include <u_signed64.h>         /* for cast64 function */
#include <hpss_api.h>

#elif (HPSS_MAJOR_VERSION == 6) || (HPSS_MAJOR_VERSION == 7)

#include <u_signed64.h>         /* for cast64 function */
#include <hpss_api.h>
#include <acct_hpss.h>

#endif

/*
 * labels in the config file
 */

# define CONF_LABEL_FS_SPECIFIC   "HPSS"

/* -------------------------------------------
 *      HPSS dependant definitions
 * ------------------------------------------- */

#define FSAL_MAX_NAME_LEN   HPSS_MAX_FILE_NAME
#define FSAL_MAX_PATH_LEN   HPSS_MAX_PATH_NAME

/* prefered readdir size */
#define FSAL_READDIR_SIZE 2048

/** object name.  */

typedef struct fsal_name__
{
  char name[FSAL_MAX_NAME_LEN];
  unsigned int len;
} fsal_name_t;

/** object path.  */

typedef struct fsal_path__
{
  char path[FSAL_MAX_PATH_LEN];
  unsigned int len;
} fsal_path_t;

#define FSAL_NAME_INITIALIZER {"",0}
#define FSAL_PATH_INITIALIZER {"",0}

static fsal_name_t FSAL_DOT = { ".", 1 };
static fsal_name_t FSAL_DOT_DOT = { "..", 2 };

/* Filesystem handle */

typedef struct fsal_handle__
{

  /* The object type */
  fsal_nodetype_t obj_type;

  /* The hpss handle */
  ns_ObjHandle_t ns_handle;

} fsal_handle_t;

/** FSAL security context */

#if HPSS_MAJOR_VERSION == 5

typedef struct
{

  time_t last_update;
  hsec_UserCred_t hpss_usercred;

} fsal_cred_t;

#elif (HPSS_MAJOR_VERSION == 6) || (HPSS_MAJOR_VERSION == 7)

typedef struct
{

  time_t last_update;
  sec_cred_t hpss_usercred;

} fsal_cred_t;

#endif

typedef struct
{

  ns_ObjHandle_t fileset_root_handle;
  unsigned int default_cos;

} fsal_export_context_t;

#define FSAL_EXPORT_CONTEXT_SPECIFIC( pexport_context ) (uint64_t)(pexport_context->default_cos)

typedef struct
{

  fsal_cred_t credential;
  fsal_export_context_t *export_context;

} fsal_op_context_t;

#if (HPSS_MAJOR_VERSION == 5)
#define FSAL_OP_CONTEXT_TO_UID( pcontext ) ( pcontext->credential.hpss_usercred.SecPwent.Uid )
#define FSAL_OP_CONTEXT_TO_GID( pcontext ) ( pcontext->credential.hpss_usercred.SecPwent.Gid )
#elif (HPSS_MAJOR_VERSION == 6) || (HPSS_MAJOR_VERSION == 7)
#define FSAL_OP_CONTEXT_TO_UID( pcontext ) ( pcontext->credential.hpss_usercred.Uid )
#define FSAL_OP_CONTEXT_TO_GID( pcontext ) ( pcontext->credential.hpss_usercred.Gid )
#endif

/** directory stream descriptor */
typedef struct fsal_dir__
{
  fsal_handle_t dir_handle;     /* directory handle */
  fsal_op_context_t context;    /* credential for readdir operations */
  int reserved;                 /* not used */
} fsal_dir_t;

/** FSAL file descriptor */

#if (HPSS_MAJOR_VERSION == 5)

typedef struct fsal_file__
{
  int filedes;                  /* file descriptor. */
  gss_token_t fileauthz;        /* data access credential. */
} fsal_file_t;

#elif (HPSS_MAJOR_VERSION == 6)

typedef struct fsal_file__
{
  int filedes;                  /* file descriptor. */
  hpss_authz_token_t fileauthz; /* data access credential. */
} fsal_file_t;

#elif (HPSS_MAJOR_VERSION == 7)

typedef struct fsal_file__
{
  int filedes;                  /* file descriptor. */
} fsal_file_t;

#endif

#define FSAL_FILENO( p_fsal_file )  ( (p_fsal_file)->filedes )

/** HPSS specific init info */

#if (HPSS_MAJOR_VERSION == 5)

typedef struct fs_specific_initinfo__
{

  /* specifies the behavior for each init value : */
  struct behaviors
  {
    fsal_initflag_t
        /* client API configuration */
    PrincipalName, KeytabPath,
        /* Other specific configuration */
    CredentialLifetime, ReturnInconsistentDirent;

  } behaviors;

  /* client API configuration info : */
  api_config_t hpss_config;

  /* other configuration info */
  fsal_uint_t CredentialLifetime;
  fsal_uint_t ReturnInconsistentDirent;

} fs_specific_initinfo_t;

#elif (HPSS_MAJOR_VERSION == 6) || (HPSS_MAJOR_VERSION == 7)

typedef struct fs_specific_initinfo__
{

  /* specifies the behavior for each init value : */
  struct behaviors
  {
    fsal_initflag_t
        /* client API configuration */
    AuthnMech, NumRetries, BusyDelay, BusyRetries, MaxConnections, DebugPath,
        /* Other specific configuration */
    Principal, KeytabPath, CredentialLifetime, ReturnInconsistentDirent;

  } behaviors;

  /* client API configuration info : */
  api_config_t hpss_config;

  /* other configuration info */
  char Principal[FSAL_MAX_NAME_LEN];
  char KeytabPath[FSAL_MAX_PATH_LEN];

  fsal_uint_t CredentialLifetime;
  fsal_uint_t ReturnInconsistentDirent;

} fs_specific_initinfo_t;

#endif

/** directory cookie : OffsetOut parameter of hpss_ReadRawAttrsHandle. */
typedef u_signed64 fsal_cookie_t;

#define FSAL_READDIR_FROM_BEGINNING  (cast64(0))

typedef void *fsal_lockdesc_t;   /**< not implemented in hpss */

#if HPSS_LEVEL >= 730
#define HAVE_XATTR_CREATE 1
#endif

#endif                          /* _FSAL_TYPES_SPECIFIC_H */