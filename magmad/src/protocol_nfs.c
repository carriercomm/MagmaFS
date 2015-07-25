/*
   MAGMA -- protocol_nfs.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Implement protocol for client mounts. File name is a bit misleading.
   protocol_mount.c should be more appropriate. Probably will change
   in future release to encourage writing pure kernel filesystems.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 
*/
/*
 * This file was generated by glibc's rpcgen tool and then customized
 */

#ifdef MAGMA_ENABLE_NFS_INTERFACE

#ifndef MAX_READ_TRY
#define MAX_READ_TRY 16
#endif
#ifndef PATH_LENGTH
#define PATH_LENGTH 255
#endif

#include "magma.h"

/* NFS v.2 does not allow memory starvation as a cause of error! */
#define NFSERR_NOMEM NFSERR_IO

bool_t
nfsproc_null_2_svc(void *argp, void *result, struct svc_req *rqstp)
{
	bool_t retval = TRUE;

	(void) argp;
	(void) result;
	(void) rqstp;  /* should be used for authentication */

	/*
	 * insert server code here
	 */

	return retval;
}

#define compose_fattr(flare,fattr) {\
	     if (magma_isreg(flare))  fattr.type = NFREG;\
	else if (magma_isdir(flare))  fattr.type = NFDIR;\
	else if (magma_isblk(flare))  fattr.type = NFBLK;\
	else if (magma_ischr(flare))  fattr.type = NFCHR;\
	else if (magma_islnk(flare))  fattr.type = NFLNK;\
	else if (magma_issock(flare)) fattr.type = NFSOCK;\
	else if (magma_isfifo(flare)) fattr.type = NFFIFO;\
	fattr.mode = flare->st.st_mode;\
	fattr.nlink = flare->st.st_nlink;\
	fattr.uid = flare->st.st_uid;\
	fattr.gid = flare->st.st_gid;\
	fattr.size = flare->st.st_size;\
	fattr.blocksize = flare->st.st_blksize;\
	fattr.rdev = flare->st.st_rdev;\
	fattr.blocks = flare->st.st_blocks;\
	fattr.fsid = 100;\
	fattr.fileid = flare->st.st_ino;\
	memcpy(&(fattr.atime), &(flare->st.st_atime), sizeof(nfstime));\
	memcpy(&(fattr.ctime), &(flare->st.st_ctime), sizeof(nfstime));\
	memcpy(&(fattr.mtime), &(flare->st.st_mtime), sizeof(nfstime));\
}

bool_t
nfsproc_getattr_2_svc(nfs_fh *argp, attrstat *result, struct svc_req *rqstp)
{
	struct stat statbuf;
	int res = 0, server_errno = 0;
	char *path = NULL;

	(void) rqstp;  /* should be used for authentication */

	memset(&statbuf, 0, sizeof(struct stat));

	/* get flare from cache or from disk */
	magma_flare_t *flare = magma_search_by_hash(argp->data);
	if (flare == NULL) {
		result->status = NFSERR_NOENT;
		char *armoured = armour_hash(argp->data);
		if (armoured != NULL) {
			dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOMEM inside GETATTR for %s", armoured);
			g_free_null(armoured);
		} else {
			dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOMEM inside GETATTR for unknown hash");
		}
		return FALSE;
	}

	dbg(LOG_INFO, DEBUG_PNFS, " ** GETATTR on %s -> %s", flare->path, flare->xlated);

	magma_node_t *owner = magma_route_path(flare->path);

	if (compare_nodes(owner, &myself)) {

		if (!flare->is_upcasted) {
			result->status = NFSERR_NOENT;
			dbg(LOG_ERR, DEBUG_ERR, " ** GETATTR flare not upcasted");
			return FALSE;
		}

		if ((res = stat(flare->contents, &statbuf)) == -1) {
			result->status = errno;
			dbg(LOG_ERR, DEBUG_ERR, " ** GETATTR error on stat(): %s", strerror(errno));
			return FALSE;
		} else {
			result->status = NFS_OK;
			flare->st.st_size = statbuf.st_size;
			flare->st.st_blocks = statbuf.st_blocks;
			flare->st.st_blksize = statbuf.st_blksize;
		}

		if (magma_isdir(flare)) {
			memcpy(&statbuf, &(flare->st), sizeof(struct stat));
			statbuf.st_mode = flare->st.st_mode;
		} else {
			memcpy(&statbuf, &(flare->st), sizeof(struct stat));
		}

	} else {
		
		int fs;
		forward_socket(owner->ip_addr, fs);
		pktqs_getattr(fs,path);
		pktar_getattr(fs,res,statbuf,server_errno);
		shutdown(fs, SHUT_RDWR);

	}

	/* setting result */
	compose_fattr(flare,result->attrstat_u.attributes);

	dbg(LOG_INFO, DEBUG_PNFS, " ** GETATTR performed ok!");
	return TRUE;
}

bool_t
nfsproc_setattr_2_svc(sattrargs *argp, attrstat *result, struct svc_req *rqstp)
{
	bool_t retval = TRUE;
	struct stat statbuf;
	int res = 0, server_errno = 0;
	char *path = NULL;

	(void) rqstp;  /* should be used for authentication */

	memset(&statbuf, 0, sizeof(struct stat));

	/* get flare from cache or from disk */
	magma_flare_t *flare = magma_search_by_hash(argp->file.data);
	if (flare == NULL) {
		result->status = NFSERR_NOENT;
		dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOMEM inside SETATTR");
		goto SETATTR_ABORT;
	}

	dbg(LOG_INFO, DEBUG_PNFS, " ** SETATTR on %s -> %s", flare->path, flare->xlated);

	magma_node_t *owner = magma_route_path(flare->path);

	if (compare_nodes(owner, &myself)) {

		if (!flare->is_upcasted) {
			res = -1;
			result->status = NFSERR_NOENT;
			dbg(LOG_ERR, DEBUG_ERR, " ** GETATTR flare not upcasted");
			goto SETATTR_ABORT;
		}

		flare->st.st_mode &= ~ALLPERMS;	            /* clear mode bits */
		flare->st.st_mode |= argp->attributes.mode; /* set new mode bits */

		memcpy(&(flare->st.st_uid),   &(argp->attributes.uid), sizeof(u_int));
		memcpy(&(flare->st.st_gid),   &(argp->attributes.gid), sizeof(u_int));
		memcpy(&(flare->st.st_size),  &(argp->attributes.size), sizeof(u_int));
		memcpy(&(flare->st.st_atime), &(argp->attributes.atime), sizeof(u_int));
		memcpy(&(flare->st.st_mtime), &(argp->attributes.mtime), sizeof(u_int));

		result->status = NFS_OK;
		compose_fattr(flare,result->attrstat_u.attributes);

	} else {
		
		int mode, uid, gid, offset;
		int fs;
		forward_socket(owner->ip_addr, fs);
		pktqs_chmod(fs,mode,path);
		pktar_chmod(fs,res,server_errno);
		if (result->status != NFS_OK) goto SETATTR_PROXY;
		pktqs_chown(fs,uid,gid,path);
		pktar_chown(fs,res,server_errno);
		if (result->status != NFS_OK) goto SETATTR_PROXY;
		pktqs_truncate(fs,offset,path);
		pktar_truncate(fs,res,server_errno);
		if (result->status != NFS_OK) goto SETATTR_PROXY;
		pktqs_getattr(fs,path);
		pktar_getattr(fs,res,statbuf,server_errno);

SETATTR_PROXY:
		shutdown(fs, SHUT_RDWR);

	}

SETATTR_ABORT:

	if ( result->status != NFS_OK ) {
		dbg(LOG_ERR, DEBUG_ERR, " ** GETATTR %s: %s", flare->path, strerror(result->status));
	} else {
		dbg(LOG_INFO, DEBUG_PNFS, " ** GETATTR performed ok!");
	}

	return retval;
}

bool_t
nfsproc_root_2_svc(void *argp, void *result, struct svc_req *rqstp)
{
	bool_t retval = TRUE;

	(void) argp;
	(void) result;
	(void) rqstp;  /* should be used for authentication */

	/* obsoleted in release 2 */
	return retval;
}

bool_t
nfsproc_lookup_2_svc(diropargs *argp, diropres *result, struct svc_req *rqstp)
{
	(void) rqstp;  /* should be used for authentication */

	magma_flare_t *dir_flare = magma_search_by_hash(argp->dir.data);
	if (dir_flare == NULL) {
		result->status = NFSERR_IO;
		return FALSE;
	} else if (!dir_flare->is_upcasted) {
		if (!magma_check_flare(dir_flare)) {
			char *armoured = armour_hash(argp->dir.data);
			if (armoured != NULL) {
				dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOENT inside LOOKUP for hash %s", armoured);
				g_free_null(armoured);
			} else {
				dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOENT inside LOOKUP");
			}
			result->status = NFSERR_NOENT;
		} else {
			char *armoured = armour_hash(argp->dir.data);
			if (armoured != NULL) {
				dbg(LOG_ERR, DEBUG_ERR, "NFSERR_IO inside LOOKUP on hash %s", armoured);
			} else {
				dbg(LOG_ERR, DEBUG_ERR, "NFSERR_IO inside LOOKUP");
			}
			result->status = NFSERR_IO;
		}
		return FALSE;
	}

	char *fullpath;
	fullpath = g_strconcat(dir_flare->path, "/", argp->name, NULL);
	if (fullpath == NULL) {
		result->status = NFSERR_NOENT;
		dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOENT inside LOOKUP");
		return FALSE;
	}

	magma_flare_t *file_flare = magma_search_or_create(fullpath);
	if ((file_flare == NULL) || (!file_flare->is_upcasted)) {
		result->status = NFSERR_NOENT;
		dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOENT inside LOOKUP for %s", fullpath);
		g_free_null(fullpath);
		return FALSE;
	}

	/* composing file attributes */
	compose_fattr(file_flare, result->diropres_u.diropres.attributes);	
		
	g_free_null(fullpath);
	return TRUE;
}

bool_t
nfsproc_readlink_2_svc(nfs_fh *argp, readlinkres *result, struct svc_req *rqstp)
{
	bool_t retval = TRUE;
	int res = 0, server_errno;

	(void) argp;
	(void) result;
	(void) rqstp;  /* should be used for authentication */

	/* get flare from cache or from disk */
	magma_flare_t *flare = magma_search_by_hash(argp->data);
	if (flare == NULL) {
		result->status = NFSERR_NOMEM;
		dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOMEM inside READLINK");
		goto READLINK_ABORT;
	}

	dbg(LOG_INFO, DEBUG_PNFS, " ** READLINK on %s -> %s", flare->path, flare->xlated);

	magma_node_t *owner = magma_route_path(flare->path);
	if ( compare_nodes(&myself, owner) ) {

		if (!flare->is_upcasted) {
			result->status = NFSERR_NOENT;
			goto READLINK_ABORT;
		}

		if (!magma_islnk(flare)) {
			result->status = NFSERR_NOENT;
			goto READLINK_ABORT;
		}

		assert(flare != NULL);
		assert(flare->item.symlink != NULL);

		if (flare->item.symlink->target != NULL) {
			result->readlinkres_u.data = g_strdup(flare->item.symlink->target);
			result->status = NFS_OK;
		} else {
			struct stat st;
			if (stat(flare->contents, &st) != -1) {
				flare->item.symlink->target = malloc(st.st_size+1);
				if (flare->item.symlink->target != NULL) {
					int fd = open_flare_contents(flare);
					if (fd != -1) {
						read(fd, flare->item.symlink->target, st.st_size);
						close(fd);
						result->readlinkres_u.data = g_strdup(flare->item.symlink->target);
						result->status = NFS_OK;
					} else {
						result->status = NFSERR_ACCES;
						goto READLINK_ABORT;
					}
				} else {
					result->status = NFSERR_NOMEM;
					goto READLINK_ABORT;
				}
			} else {
				result->status = errno;
				goto READLINK_ABORT;
			}
		}

	} else {

		int fs, size = 1024;
		char *nbuf = malloc(size);
		forward_socket(owner->ip_addr, fs);
		pktqs_readlink(fs, flare->path, size);
		pktar_readlink(fs, res, nbuf, server_errno);
		shutdown(fs, SHUT_RDWR);
		g_free_null(nbuf);
		result->status = res;
		result->readlinkres_u.data = g_strdup(nbuf);

	}

READLINK_ABORT:

	if (result->status == NFS_OK) {
		dbg(LOG_ERR, DEBUG_ERR, " ** READLINK %s: %s", flare->path, strerror(result->status));
	} else {
		dbg(LOG_INFO, DEBUG_PNFS, " ** READLINK performed: %s", result->readlinkres_u.data);
	}

	return retval;
}

struct entry *new_entry()
{
	struct entry *e = malloc(sizeof(struct entry));

	if (e != NULL) {
		e->fileid = 0;
		e->name = NULL;
		strcpy(e->cookie, "");
		e->nextentry = NULL;
		dbg(LOG_INFO, DEBUG_PNFS, "New entry allocated");
	}

	return e;
}

bool_t
nfsproc_readdir_2_svc(readdirargs *argp, readdirres *result, struct svc_req *rqstp)
{
	off_t offset = 0;

	(void) rqstp;  /* should be used for authentication */

	result->status = NFS_OK;
	result->readdirres_u.reply.eof = FALSE;
	result->readdirres_u.reply.entries = new_entry();
	struct entry *nextentry = result->readdirres_u.reply.entries;
	struct entry *lastentry = result->readdirres_u.reply.entries;

	if (nextentry == NULL) {
		result->status = NFSERR_NOMEM;
		dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOMEM inside READDIR");
		return FALSE;
	}

	/* get flare from cache or from disk */
	magma_flare_t *flare = magma_search_by_hash(argp->dir.data);
	if (flare == NULL) {
		result->status = NFSERR_NOMEM;
		dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOMEM inside READDIR");
		return FALSE;
	}

	dbg(LOG_INFO, DEBUG_PNFS, " ** READDIR on %s -> %s", flare->path, flare->xlated);

	magma_node_t *owner = magma_route_path(flare->path);

	if (!compare_nodes(owner, &myself)) {

//
//		TUTTA DA AGGIUSTARE!!!
//
//		char *entry;
//		int fs;
//		forward_socket(owner->ip_addr, fs);
//
//		/* start readdir */
//		pktqs_readdir(fs,path);
//
//		/* confirm readdir */
//		pktar_readdir(fs,res,server_errno); 
//		pktas_readdir(s,(void *)res,server_errno);
//
//		if ( res ) {
//			do {
//				recv_readdir_offset(s,offset);
//				send_readdir_offset(fs,offset);
//				recv_readdir_entry(fs,entry,offset);
//				send_readdir_entry(s,entry,offset);
//			} while (offset != READDIR_STOPPER);
//		}
//
//		shutdown(fs, SHUT_RDWR);

	} else {

		/* check if flare is upcasted */
		if (!flare->is_upcasted)
			magma_load_flare(flare);

		if (!flare->is_upcasted) {
			result->status = NFSERR_NOENT;
			return FALSE;
		}

		dbg(LOG_INFO, DEBUG_PNFS, " ** READDIR on %s", flare->path);
	
    	magma_DIR_t *dp = magma_opendir(flare->path);
	    if ( dp == NULL ) {
			result->status = NFSERR_NOENT;
	        return FALSE;
		}
	
		/* start reading directory at offset passed by query */
		memcpy(&offset, &(argp->cookie), sizeof(offset));
		offset = ntohl(offset);
	    magma_seekdir(dp, offset);

    	char *de = NULL;
		do {
		
			dbg(LOG_INFO, DEBUG_FLARE, " ** READDIR: reading directory...");
	    	if ((de = magma_readdir(dp)) == NULL) {
				/* EOF reached */
				result->readdirres_u.reply.eof = TRUE;
				result->status = NFS_OK;
				dbg(LOG_INFO, DEBUG_FLARE, " ** READDIR: last entry read");

				g_free_null(nextentry);
				lastentry->nextentry = NULL;

				return TRUE;
			}

			/* saving this entry */
			offset = htonl(offset);
			memcpy(&(nextentry->cookie), &offset, sizeof(nextentry->cookie));
			offset = magma_telldir(dp);

			nextentry->name = g_strdup(de);

			/* getting the file id from st.st_ino field of associated flare */
			magma_flare_t *dir_flare = magma_search_by_hash(argp->dir.data);
			if (dir_flare != NULL) {
				char *flare_name;
				flare_name = g_strconcat(dir_flare->path, "/", de, NULL);
				magma_flare_t *file_flare = magma_search_or_create(flare_name);	
				if (file_flare != NULL && file_flare->is_upcasted) {
					nextentry->fileid = magma_get_flare_inode(file_flare);
				} else {
					nextentry->fileid = 0;
				}
				g_free_null(flare_name);
			} else {
				nextentry->fileid = 0;
			}
	
			/* allocate next entry */
			nextentry->nextentry = new_entry();
			if (nextentry->nextentry == NULL) {
				result->status = NFSERR_NOMEM;
				dbg(LOG_INFO, DEBUG_FLARE, " ** READDIR: error allocating memory for nextentry");
				return FALSE;
			}
			/* linking the list */
			lastentry = nextentry;
			nextentry = nextentry->nextentry;

			dbg(LOG_INFO, DEBUG_PNFS, " ** READDIR: adding entry %s to reply @%u",
				de, (unsigned int) offset);
	
		} while (de != NULL);

    	magma_closedir(dp);
	}

	dbg(LOG_INFO, DEBUG_PNFS, " ** READDIR performed");

	result->status = NFS_OK;
	return TRUE;
}

/**
 * manages a mknod() operation
 *
 * @param @s peer socket
 */
bool_t
nfsproc_create_2_svc(createargs *argp, diropres *result, struct svc_req *rqstp)
{
	int server_errno = 0, res = 0;

	(void) rqstp;  /* should be used for authentication */

	/* get flares from cache or from disk */
	magma_flare_t *dir_flare = magma_search_by_hash(argp->where.dir.data);
	if (dir_flare == NULL || !dir_flare->is_upcasted) {
		result->status = NFSERR_NOMEM;
		dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOMEM inside CREATE");
		goto CREATE_ABORT;
	}

	char *filepath;
	filepath = g_strconcat(dir_flare->path, "/", argp->where.name, NULL);
	if (filepath == NULL) {
		result->status = NFSERR_NOMEM;
		dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOMEM inside CREATE");
		goto CREATE_ABORT;
	}

	magma_flare_t *flare = magma_search_or_create(filepath);
	if (flare == NULL) {
		result->status = NFSERR_NOMEM;
		dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOMEM inside CREATE");
		goto CREATE_ABORT;
	}

	dbg(LOG_INFO, DEBUG_PNFS, " ** CREATE on %s -> %s", flare->path, flare->xlated);

	magma_node_t *owner = magma_route_path(flare->path);
	if (!compare_nodes(owner, &myself)) {

		int fs;
		dev_t rdev = 0;
		forward_socket(owner->ip_addr, fs);
		pktqs_mknod(fs,argp->attributes.mode,rdev,flare->path);
		pktar_mknod(fs,res,server_errno);
		shutdown(fs, SHUT_RDWR);

	} else {

		if (flare->is_upcasted) {
			result->status = NFSERR_EXIST;
			goto CREATE_ABORT;
		} else {
			
			/* upcast this flare */
			     if (S_ISREG(argp->attributes.mode))  { magma_cast_to_file(flare); }
			else if (S_ISCHR(argp->attributes.mode))  { magma_cast_to_chardev(flare); }
			else if (S_ISBLK(argp->attributes.mode))  { magma_cast_to_blockdev(flare); }
			else if (S_ISFIFO(argp->attributes.mode)) { magma_cast_to_fifo(flare); }
			else if (S_ISLNK(argp->attributes.mode))  { magma_cast_to_symlink(flare); }

			/* setting permissions */
			flare->st.st_mode |= argp->attributes.mode|S_IRUSR|S_IWUSR;

			/* setting major and minor */
			/* flare->st.st_rdev = rdev; // Not on NFS ! */

			/* save flare to disk */
			result->status = magma_save_flare(flare);

			if (result->status != NFS_OK) {
				dbg(LOG_ERR, DEBUG_ERR, " ** CREATE error saving flare %s: %s", flare->path, strerror(result->status));
				goto CREATE_ABORT;
			} else {
				/* add to parent directory */
				result->status = magma_add_flare_to_parent(flare);
				if ( result->status != NFS_OK ) {
					dbg(LOG_ERR, DEBUG_ERR, " ** CREATE Can't add %s to parent: %s", flare->path, strerror(errno));
					goto CREATE_ABORT;
				}
			}

			memcpy(&(result->diropres_u.diropres.file.data), flare->binhash, SHA_DIGEST_LENGTH);
			compose_fattr(flare, result->diropres_u.diropres.attributes);
		}
	}

CREATE_ABORT:

	if (filepath != NULL) g_free_null(filepath);

	if ( result->status != NFS_OK ) {
		dbg(LOG_ERR, DEBUG_ERR, " ** CREATE %s: %s", flare->path, strerror(result->status));
	} else {
		dbg(LOG_INFO, DEBUG_PNFS, " ** CREATE performed ok!");
	}

	return (result->status == NFS_OK) ? TRUE : FALSE;
}

bool_t
nfsproc_mkdir_2_svc(createargs *argp, diropres *result, struct svc_req *rqstp)
{
	int server_errno = 0, res = 0;

	(void) rqstp;  /* should be used for authentication */

	/* get flares from cache or from disk */
	magma_flare_t *dir_flare = magma_search_by_hash(argp->where.dir.data);
	if (dir_flare == NULL || !dir_flare->is_upcasted) {
		result->status = NFSERR_NOMEM;
		dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOMEM inside MKDIR");
		return FALSE;
	}

	char *filepath;
	filepath = g_strconcat(dir_flare->path, "/", argp->where.name, NULL);
	if (filepath == NULL) {
		result->status = NFSERR_NOMEM;
		dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOMEM inside MKDIR");
		return FALSE;
	}

	magma_flare_t *flare = magma_search_or_create(filepath);
	if (flare == NULL) {
		result->status = NFSERR_NOMEM;
		g_free_null(filepath);
		dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOMEM inside MKDIR");
		return FALSE;
	}

	g_free_null(filepath);

	dbg(LOG_INFO, DEBUG_PNFS, " ** MKDIR on %s -> %s", flare->path, flare->xlated);

	magma_node_t *owner = magma_route_path(flare->path);
	if (!compare_nodes(owner, &myself)) {

		int fs;
		forward_socket(owner->ip_addr, fs);
		pktqs_mkdir(fs,argp->attributes.mode,flare->path);
		pktar_mkdir(fs,res,server_errno);
		shutdown(fs, SHUT_RDWR);

	} else {
		
		if (flare->is_upcasted) {
			result->status = NFSERR_EXIST;
			dbg(LOG_INFO, DEBUG_DIR, " ** MKDIR flare %s already exists", flare->path);
			return FALSE;
		} else {
			/* turn flare into a directory */
			magma_cast_to_dir(flare);

			/* save requested permission mode */
			flare->st.st_mode |= argp->attributes.mode|S_IRUSR|S_IWUSR;

			/* save flare to disk */
			res = magma_save_flare(flare);

			if (result->status != NFS_OK) {
				server_errno = errno;
				dbg(LOG_ERR, DEBUG_ERR, " ** MKDIR error saving dir %s: %s", flare->path, strerror(errno));
			} else {
				res = magma_add_flare_to_parent(flare);
				if ( result->status != NFS_OK ) {
					server_errno = errno;
					dbg(LOG_ERR, DEBUG_ERR, " ** MKDIR Can't add %s to parent: %s", flare->path, strerror(errno));
				} else {
					server_errno = 0;
					dbg(LOG_INFO, DEBUG_PNFS, " ** MKDIR performed");
				}
			}
		}

	}

	return (result->status == NFS_OK) ? TRUE : FALSE;
}

bool_t
nfsproc_remove_2_svc(diropargs *argp, nfsstat *result, struct svc_req *rqstp)
{
	bool_t retval = TRUE;
	char *path = NULL;
	int server_errno = 0, res = 0;

	(void) rqstp;  /* should be used for authentication */

	/* get flare from cache or from disk */
	magma_flare_t *flare = magma_search_by_hash(argp->dir.data);
	if (flare == NULL) {
		*result = NFSERR_NOMEM;
		dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOMEM inside REMOVE");
		goto UNLINK_ABORT;
	}

	dbg(LOG_INFO, DEBUG_PNFS, " ** UNLINK on %s -> %s", flare->path, flare->xlated);
	
	magma_node_t *owner = magma_route_path(flare->path);
	if (!compare_nodes(owner, &myself)) {

		int fs;
		forward_socket(owner->ip_addr, fs);
		pktqs_unlink(fs,path);
		pktar_unlink(fs,res,server_errno);
		shutdown(fs, SHUT_RDWR);

	} else {

		if (!flare->is_upcasted) {
			res = -1;
			server_errno = magma_check_flare(flare) ? EPERM : ENOENT;
			goto UNLINK_ABORT;
		}

		if (magma_isdir(flare)) {
			res = -1;
			server_errno = EPERM;
			goto UNLINK_ABORT;
		}

		res = magma_erase_flare(flare);
		server_errno = (res == -1) ? errno : 0;
	}

UNLINK_ABORT:


	if ( *result != NFS_OK ) {
		dbg(LOG_ERR, DEBUG_ERR, " ** UNLINK %s: %s", flare->path, strerror(*result));
	} else {
		dbg(LOG_INFO, DEBUG_PNFS, " ** UNLINK performed");
	}

	return retval;
}

bool_t
nfsproc_rmdir_2_svc(diropargs *argp, nfsstat *result, struct svc_req *rqstp)
{
	bool_t retval = TRUE;
	int server_errno = 0, res = 0;
	char *path = NULL;

	(void) rqstp;  /* should be used for authentication */

	/* get flare from cache or from disk */
	magma_flare_t *flare = magma_search_by_hash(argp->dir.data);
	if (flare == NULL) {
		*result = NFSERR_NOMEM;
		dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOMEM inside RMDIR");
		goto RMDIR_ABORT;
	}

	dbg(LOG_INFO, DEBUG_PNFS, " ** RMDIR on %s -> %s", flare->path, flare->xlated);
	
	magma_node_t *owner = magma_route_path(flare->path);

	if (!compare_nodes(owner, &myself)) {

		int fs;
		forward_socket(owner->ip_addr, fs);
		pktqs_rmdir(fs,path);
		pktar_rmdir(fs,res,server_errno);
		shutdown(fs, SHUT_RDWR);

	} else {

		if (!flare->is_upcasted) {
			res = -1;
			if (magma_check_flare(flare)) {
				server_errno = EPERM;
				goto RMDIR_ABORT;
			}
		}

		if (!magma_isdir(flare)) {
			res = -1;
			server_errno = ENOTDIR;
			goto RMDIR_ABORT;
		}

		if (!magma_dir_is_empty(flare)) {
			res = -1;
			server_errno = ENOTEMPTY;
			goto RMDIR_ABORT;
		}

		/* erasare dal disco :) */
		res = magma_erase_flare(flare);
		server_errno = ( res == -1 ) ? errno : 0;

	}

RMDIR_ABORT:


	if ( *result != NFS_OK ) {
		dbg(LOG_ERR, DEBUG_ERR, " ** RMDIR %s: %s", flare->path, strerror(*result));
	} else {
		dbg(LOG_INFO, DEBUG_PNFS, " ** RMDIR performed");
	}

	return retval;
}

bool_t
nfsproc_symlink_2_svc(symlinkargs *argp, nfsstat *result, struct svc_req *rqstp)
{
	bool_t retval = TRUE;
	char *from = NULL, *to = NULL;

	(void) argp;
	(void) result;
	(void) rqstp;  /* should be used for authentication */

	/* get flare from cache or from disk */
	magma_flare_t *flare = magma_search_or_create(argp->to);
	if (flare == NULL) {
		*result = NFSERR_NOMEM;
		dbg(LOG_ERR, DEBUG_ERR, "NFSERR_NOMEM inside SYMLINK");
		goto SYMLINK_ABORT;
	}

	dbg(LOG_INFO, DEBUG_PNFS, " ** SYMLINK on %s -> %s", from, flare->xlated);

	magma_node_t *owner = magma_route_path(to);
	if (compare_nodes(owner, &myself)) {
		if (flare->is_upcasted) {
			*result = NFSERR_EXIST;
			dbg(LOG_ERR, DEBUG_ERR, " ** SYMLINK flare %s already exists", flare->path);
			goto SYMLINK_ABORT;
		}

		/* upcast flare to symbolic link */
		magma_cast_to_symlink(flare);
		if (!flare->is_upcasted) {
			*result = NFSERR_NOMEM;
			dbg(LOG_ERR, DEBUG_ERR, " ** SYMLINK error upcasting flare: %s", strerror(*result));
			goto SYMLINK_ABORT;
		}

		/* composing end path */
		magma_flare_t *from_dir = magma_search_by_hash(argp->from.dir.data);
		if (from_dir == NULL) {
			*result = NFSERR_NOMEM;
			dbg(LOG_ERR, DEBUG_ERR, " ** SYMLINK error locating from directory in cache");
			goto SYMLINK_ABORT;
		}

		char *frompath = NULL;
		frompath = g_strconcat(from_dir->path, "/", argp->from.name, NULL);
		if (frompath == NULL) {
			*result = NFSERR_NOMEM;
			dbg(LOG_ERR, DEBUG_ERR, " ** SYMLINK error preparing from path");
			goto SYMLINK_ABORT;
		}

		/* copy pointed flare->path in target member */
		flare->item.symlink->target = g_strdup(frompath);

		if (!magma_save_flare(flare)) {
			*result = NFSERR_IO;
			dbg(LOG_ERR, DEBUG_ERR, " ** SYMLINK error saving to disk: %s", strerror(*result));
			goto SYMLINK_ABORT;
		}

		/* add to parent directory */
		*result = magma_add_flare_to_parent(flare);
		if ( *result != NFS_OK ) {
			*result = NFSERR_IO;
			dbg(LOG_ERR, DEBUG_ERR, " ** SYMLINK Can't add %s to parent: %s", flare->path, strerror(*result));
		}

	} else {

		int fs, res;
		forward_socket(owner->ip_addr, fs);
		pktqs_symlink(fs,from,to);
		pktar_symlink(fs,res,*result);
		shutdown(fs, SHUT_RDWR);

	}

SYMLINK_ABORT:	

	if ( *result != NFS_OK ) {
		dbg(LOG_ERR, DEBUG_ERR, " ** SYMLINK %s: %s", to, strerror(*result));
	} else {
		dbg(LOG_INFO, DEBUG_PNFS, " ** SYMLINK performed ok!");
	}

	g_free_null(from);
	g_free_null(to);
	return retval;
}

/**
 * rename a flare. renaming is a perfect example of magma
 * internal complexity (which is solved kindly by POSIX standard).
 *
 * rename()ing a flare can involve from 1 to 4(!) nodes. the
 * most complex case is when flare "/path1/original", hosted on
 * node "n1", has to be moved to "/path2/newname", hosted on
 * node "n2", while flare "/path1/" is on node n3 and "/path2/" is
 * on node n4.
 *
 * that is pure pain in the ass! magma should.
 * 1. add entry in /path2/ on node n4
 * 2. create flare "/path2/newname" on n2 and sync contents.
 * 3. remove entry from /path1/ on node n3
 * 4. delete flare from itself on node n1
 *
 * everything should be managed with a chain of fallback
 * tests which should recover previous state if a step fail!
 *
 * BUT! POSIX standard states that when a rename() is performed
 * on two flare->paths on different filesystems, should return -1 and
 * set errno = EXDEV (Invalid cross-device link).
 *
 * take mv(1) behaviour as an example, while renaming /tmp/file
 * to /home/user/file (assuming that /tmp and /home are on
 * different filesystems):
 *
 * [..cut..]
 * rename("/home/tx0/xyz", "/tmp/xyz")     = -1 EXDEV (Invalid cross-device link)
 * unlink("/tmp/xyz")                      = -1 ENOENT (No such file or directory)
 * open("/home/tx0/xyz", O_RDONLY|O_LARGEFILE) = 3
 * fstat64(3, {st_mode=S_IFREG|0644, st_size=0, ...}) = 0
 * open("/tmp/xyz", O_WRONLY|O_CREAT|O_LARGEFILE, 0100644) = 4
 * [..cut..]
 *
 * as you can see, rename failed with EXDEV errno. so mv assumes
 * that a normal copy (read/write pair) plus unlink should be
 * performed! and do it. but magma already has operations for
 * opening, reading and writing a flare (which also add and
 * remove flares from parent flares)
 *
 * so, that's the plan: if source and destination is both locally
 * we implement rename for speed reasons (it's simply a matter of
 * create new destination flare, save it, copy contents, case by
 * case -- is a directory? a symlink? what? -- and than erasing
 * the original one). for all other cases, -1 EXDEV will be
 * returned!
 *
 * @param @s network socket
 */
bool_t
nfsproc_rename_2_svc(renameargs *argp, nfsstat *result, struct svc_req *rqstp)
{
	bool_t retval = TRUE;
	char *from = NULL, *to = NULL;
	int server_errno = 0, res = 0;

	(void) argp;
	(void) result;
	(void) rqstp;  /* should be used for authentication */

	*result = NFSERR_NXIO;
	goto RENAME_ABORT;

	/* get flares from cache or from disk */
	magma_flare_t *from_flare = magma_search_by_hash(from);
	if (from_flare == NULL) {
		res = -1;
		server_errno = ENOMEM;
		goto RENAME_ABORT;
	}

	dbg(LOG_INFO, DEBUG_PNFS, " ** RENAME on %s -> %s", from, to);

	magma_node_t *from_owner = magma_route_path(from);
	if (!compare_nodes(from_owner, &myself)) {
		
		int fs;
		forward_socket(from_owner->ip_addr, fs);
		pktqs_rename(fs,from,to);
		pktar_rename(fs,res,server_errno);
		shutdown(fs, SHUT_RDWR);

	} else {

		/*
		 * 1. aggiungere il flare al parent
		 * 2. routare il flare di destinazione
		 * 3. se e' locale:
		 * 3.1. scrivere una funzione che duplica i flare
		 * 3.1.1. questa funzione non replica i campi parent, left, right, mutex, load
		 * 3.2. duplicare il from_flare
		 * 3.3. salvare il flare sotto il nuovo nome
		 * 4. se e' remoto:
		 * 4.1. ottenere il nodo remoto
		 * 4.2. aprire un socket
		 * 4.3. copiare il contenuto del flare con una send_key
		 * 5. rimuovere il vecchio flare
		 */

		magma_flare_t *to_flare = magma_search_by_hash(to);
		if (to_flare == NULL) {
			res = -1;
			server_errno = ENOMEM;
			goto RENAME_ABORT;
		}

		/* destination flare exists */
		res = magma_add_flare_to_parent(to_flare);
		if (res == -1) {
			/* problems with permissions */
			server_errno = errno;
			goto RENAME_ABORT;
		}

		magma_node_t *to_owner = magma_route_path(to);
		if (compare_nodes(to_owner, &myself)) {
			/* destination it's local */

			/* check if destination flare is upcasted */
			if (!to_flare->is_upcasted) {
				flare_upcast(to_flare);
			}

			if (!to_flare->is_upcasted) {
				res = -1;
				server_errno = errno;
				goto RENAME_ABORT;
			}

			/* save flare to disk */
			if (!magma_save_flare(to_flare)) {
				res = -1;
				server_errno = errno;
				goto RENAME_ABORT;
			}

			
		} else {
			/* destination it's remote */
		}

		/* erasing original flare */
		magma_erase_flare(from_flare);

	}

RENAME_ABORT:

	if ( *result != NFS_OK ) {
		dbg(LOG_ERR, DEBUG_ERR, " ** RENAME %s -> %s: %s", from, to, strerror(*result));
	} else {
		dbg(LOG_INFO, DEBUG_PNFS, " ** RENAME performed");
	}

	if (from != NULL) g_free_null(from);
	if (to != NULL) g_free_null(to);
	return retval;
}

bool_t
nfsproc_link_2_svc(linkargs *argp, nfsstat *result, struct svc_req *rqstp)
{
	bool_t retval = TRUE;
	/*
	char *from, *to, *xlated_from, *xlated_to;
	int server_errno = 0, res = 0;
	*/

	(void) argp;
	(void) result;
	(void) rqstp;  /* should be used for authentication */

	/* TODO to be implemented */
	return retval;
}

/* SIAMO ARRIVATI QUI CON LA CONVERSIONE MA UN SACCO DI ROBA VA TESTATA!!! */

bool_t
nfsproc_read_2_svc(readargs *argp, readres *result, struct svc_req *rqstp)
{
	bool_t retval = TRUE;
	size_t size;
	off_t offset;
	char *nbuf = NULL, *path = NULL;
	int server_errno = 0, res = 0;

	(void) argp;
	(void) result;
	(void) rqstp;  /* should be used for authentication */

	/* get flare from cache or from disk */
	magma_flare_t *flare = magma_search_by_hash(path);
	if (flare == NULL) {
		res = -1;
		server_errno = ENOMEM;
		dbg(LOG_ERR, DEBUG_ERR, " ** READ %s: %s", flare->path, strerror(result->status));
		goto READ_ABORT;
	}

	dbg(LOG_INFO, DEBUG_PNFS, " ** READ on %s -> %s", flare->path, flare->xlated);

	magma_node_t *owner = magma_route_path(flare->path);

	if (!compare_nodes(owner, &myself)) {

		int fs;
		forward_socket(owner->ip_addr, fs);
		pktqs_read(fs,size,offset,path);
		pktar_read(fs,res,nbuf,server_errno);
		shutdown(fs, SHUT_RDWR);

	} else {

		int fd = open_flare_contents(flare);
		if (fd == -1) {
			res = -1;
			server_errno = errno;
			dbg(LOG_ERR, DEBUG_ERR, " ** READ can't open() %s: %s", flare->path, strerror(result->status));
		} else {

			if ( ( nbuf = malloc(size) ) == NULL ) {
				res = -1;
				server_errno = errno;
				dbg(LOG_ERR, DEBUG_ERR, "Can't allocate %d bytes!", size);
			} else {
				memset(nbuf, 0, size);
	
				if ( (res = pread(fd, nbuf, size, offset)) == -1 ) {
					server_errno = errno;
					dbg(LOG_ERR, DEBUG_ERR, " ** READ: can't pread() %s: %s", flare->path, strerror(result->status));
				} else {
					dbg(LOG_INFO, DEBUG_PNFS, " ** READ: pread() returned %d/%d bytes from %s", res, size, flare->path);
				}
			}
			close(fd);
		}
	}

READ_ABORT:

g_free_null(nbuf);
    return retval;
}

bool_t
nfsproc_writecache_2_svc(void *argp, void *result, struct svc_req *rqstp)
{
	bool_t retval = TRUE;

	(void) argp;
	(void) result;
	(void) rqstp;  /* should be used for authentication */
	/* unused in release 2 */
	return retval;
}

bool_t
nfsproc_write_2_svc(writeargs *argp, attrstat *result, struct svc_req *rqstp)
{
	bool_t retval = TRUE;
	size_t size;
	off_t offset;
	char *data = NULL, *path = NULL;
	int server_errno = 0, res = 0;

	(void) argp;
	(void) result;
	(void) rqstp;  /* should be used for authentication */

	/* get flare from cache or from disk */
	magma_flare_t *flare = magma_search_by_hash(path);
	if (flare == NULL) {
		res = -1;
		server_errno = ENOMEM;
		goto WRITE_ABORT;
	}

	dbg(LOG_INFO, DEBUG_PNFS, " ** WRITE: writing %d bytes at offset %d on %s -> %s", size, (int) offset, flare->path, flare->xlated);

	magma_node_t *owner = magma_route_path(flare->path);
	if (compare_nodes(owner, &myself)) {

		if (flare->is_upcasted) {

			if (magma_isdir(flare)) {
				res = -1;
				server_errno = EISDIR;	
			} else {

				int contentsfd = open_flare_contents(flare);
				if (contentsfd != -1) {
					res = pwrite(contentsfd, data, size, offset);
					server_errno = ( result->status != NFS_OK ) ? errno : 0;
					close(contentsfd);
	
					if ( result->status != NFS_OK ) {
						dbg(LOG_ERR, DEBUG_ERR, " ** WRITE can't pwrite() %s: %s", flare->path, strerror(result->status));
					} else {
						dbg(LOG_INFO, DEBUG_PNFS, " ** WRITE performed (%d bytes)", res);
					}
				} else {
					res = -1;
					server_errno = errno;
				}
			}
		}

	} else {

		int fs;
		forward_socket(owner->ip_addr, fs);
		pktqs_write(fs,size,offset,path,data);
		pktar_write(fs,res,server_errno);
		shutdown(fs, SHUT_RDWR);

	}

	dbg(LOG_INFO, DEBUG_PNFS, "Returning %d bytes writted", res);

WRITE_ABORT:

g_free_null(data);

    return retval;
}

bool_t
nfsproc_statfs_2_svc(nfs_fh *argp, statfsres *result, struct svc_req *rqstp)
{
	struct statfs statbuf;
	int res = 0, server_errno = 0;
	char *path = NULL;

	(void) argp;
	(void) result;
	(void) rqstp;  /* should be used for authentication */

	/* get flare from cache or from disk */
	magma_flare_t *flare = magma_search_by_hash(argp->data);
	if (flare == NULL) {
		result->status = NFSERR_NOENT;
		goto STATFS_ABORT;
	}

	dbg(LOG_INFO, DEBUG_PNFS, " ** STATFS on %s -> %s", flare->path, flare->xlated);

	magma_node_t *owner = magma_route_path(flare->path);

	if (!compare_nodes(owner, &myself)) {

		int fs;
		forward_socket(owner->ip_addr, fs);
		pktqs_statfs(fs,path);
		pktar_statfs(fs,res,&statbuf,server_errno);
		shutdown(fs, SHUT_RDWR);

	} else {

		memset(&statbuf, 0, sizeof(struct statfs));
		result->status = statfs(flare->xlated, &statbuf);
	}

	result->statfsres_u.reply.tsize = statbuf.f_bsize;
	result->statfsres_u.reply.bsize = flare->st.st_blksize;
	result->statfsres_u.reply.blocks = statbuf.f_blocks;
	result->statfsres_u.reply.bfree = statbuf.f_bfree;
	result->statfsres_u.reply.bavail = statbuf.f_bavail;

STATFS_ABORT:

	if ( result->status != NFS_OK ) {
		dbg(LOG_ERR, DEBUG_ERR, " ** STATFS %s: %s", flare->path, strerror(result->status));
		return FALSE;
	} else {
		dbg(LOG_INFO, DEBUG_PNFS, " ** STATFS performed ok!");
		return TRUE;
	}
}

int
nfs_program_2_freeresult (SVCXPRT *transp, xdrproc_t xdr_result, caddr_t result)
{
	xdr_free (xdr_result, result);

	(void) transp;
	(void) xdr_result;
	(void) result;

	/*
	 * Insert additional freeing code here, if needed
	 */

	return 1;
}

#endif /* MAGMA_ENABLE_NFS_INTERFACE */

// vim:ts=4:nocindent:autoindent