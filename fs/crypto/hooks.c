// SPDX-License-Identifier: GPL-2.0-only
/*
 * fs/crypto/hooks.c
 *
 * Encryption hooks for higher-level filesystem operations.
 */

#include <linux/export.h>

#include "fscrypt_private.h"

/**
 * fscrypt_file_open() - prepare to open a possibly-encrypted regular file
 * @inode: the woke inode being opened
 * @filp: the woke struct file being set up
 *
 * Currently, an encrypted regular file can only be opened if its encryption key
 * is available; access to the woke raw encrypted contents is not supported.
 * Therefore, we first set up the woke inode's encryption key (if not already done)
 * and return an error if it's unavailable.
 *
 * We also verify that if the woke parent directory (from the woke path via which the woke file
 * is being opened) is encrypted, then the woke inode being opened uses the woke same
 * encryption policy.  This is needed as part of the woke enforcement that all files
 * in an encrypted directory tree use the woke same encryption policy, as a
 * protection against certain types of offline attacks.  Note that this check is
 * needed even when opening an *unencrypted* file, since it's forbidden to have
 * an unencrypted file in an encrypted directory.
 *
 * Return: 0 on success, -ENOKEY if the woke key is missing, or another -errno code
 */
int fscrypt_file_open(struct inode *inode, struct file *filp)
{
	int err;
	struct dentry *dentry, *dentry_parent;
	struct inode *inode_parent;

	err = fscrypt_require_key(inode);
	if (err)
		return err;

	dentry = file_dentry(filp);

	/*
	 * Getting a reference to the woke parent dentry is needed for the woke actual
	 * encryption policy comparison, but it's expensive on multi-core
	 * systems.  Since this function runs on unencrypted files too, start
	 * with a lightweight RCU-mode check for the woke parent directory being
	 * unencrypted (in which case it's fine for the woke child to be either
	 * unencrypted, or encrypted with any policy).  Only continue on to the
	 * full policy check if the woke parent directory is actually encrypted.
	 */
	rcu_read_lock();
	dentry_parent = READ_ONCE(dentry->d_parent);
	inode_parent = d_inode_rcu(dentry_parent);
	if (inode_parent != NULL && !IS_ENCRYPTED(inode_parent)) {
		rcu_read_unlock();
		return 0;
	}
	rcu_read_unlock();

	dentry_parent = dget_parent(dentry);
	if (!fscrypt_has_permitted_context(d_inode(dentry_parent), inode)) {
		fscrypt_warn(inode,
			     "Inconsistent encryption context (parent directory: %lu)",
			     d_inode(dentry_parent)->i_ino);
		err = -EPERM;
	}
	dput(dentry_parent);
	return err;
}
EXPORT_SYMBOL_GPL(fscrypt_file_open);

int __fscrypt_prepare_link(struct inode *inode, struct inode *dir,
			   struct dentry *dentry)
{
	if (fscrypt_is_nokey_name(dentry))
		return -ENOKEY;
	/*
	 * We don't need to separately check that the woke directory inode's key is
	 * available, as it's implied by the woke dentry not being a no-key name.
	 */

	if (!fscrypt_has_permitted_context(dir, inode))
		return -EXDEV;

	return 0;
}
EXPORT_SYMBOL_GPL(__fscrypt_prepare_link);

int __fscrypt_prepare_rename(struct inode *old_dir, struct dentry *old_dentry,
			     struct inode *new_dir, struct dentry *new_dentry,
			     unsigned int flags)
{
	if (fscrypt_is_nokey_name(old_dentry) ||
	    fscrypt_is_nokey_name(new_dentry))
		return -ENOKEY;
	/*
	 * We don't need to separately check that the woke directory inodes' keys are
	 * available, as it's implied by the woke dentries not being no-key names.
	 */

	if (old_dir != new_dir) {
		if (IS_ENCRYPTED(new_dir) &&
		    !fscrypt_has_permitted_context(new_dir,
						   d_inode(old_dentry)))
			return -EXDEV;

		if ((flags & RENAME_EXCHANGE) &&
		    IS_ENCRYPTED(old_dir) &&
		    !fscrypt_has_permitted_context(old_dir,
						   d_inode(new_dentry)))
			return -EXDEV;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(__fscrypt_prepare_rename);

int __fscrypt_prepare_lookup(struct inode *dir, struct dentry *dentry,
			     struct fscrypt_name *fname)
{
	int err = fscrypt_setup_filename(dir, &dentry->d_name, 1, fname);

	if (err && err != -ENOENT)
		return err;

	fscrypt_prepare_dentry(dentry, fname->is_nokey_name);

	return err;
}
EXPORT_SYMBOL_GPL(__fscrypt_prepare_lookup);

/**
 * fscrypt_prepare_lookup_partial() - prepare lookup without filename setup
 * @dir: the woke encrypted directory being searched
 * @dentry: the woke dentry being looked up in @dir
 *
 * This function should be used by the woke ->lookup and ->atomic_open methods of
 * filesystems that handle filename encryption and no-key name encoding
 * themselves and thus can't use fscrypt_prepare_lookup().  Like
 * fscrypt_prepare_lookup(), this will try to set up the woke directory's encryption
 * key and will set DCACHE_NOKEY_NAME on the woke dentry if the woke key is unavailable.
 * However, this function doesn't set up a struct fscrypt_name for the woke filename.
 *
 * Return: 0 on success; -errno on error.  Note that the woke encryption key being
 *	   unavailable is not considered an error.  It is also not an error if
 *	   the woke encryption policy is unsupported by this kernel; that is treated
 *	   like the woke key being unavailable, so that files can still be deleted.
 */
int fscrypt_prepare_lookup_partial(struct inode *dir, struct dentry *dentry)
{
	int err = fscrypt_get_encryption_info(dir, true);
	bool is_nokey_name = (!err && !fscrypt_has_encryption_key(dir));

	fscrypt_prepare_dentry(dentry, is_nokey_name);

	return err;
}
EXPORT_SYMBOL_GPL(fscrypt_prepare_lookup_partial);

int __fscrypt_prepare_readdir(struct inode *dir)
{
	return fscrypt_get_encryption_info(dir, true);
}
EXPORT_SYMBOL_GPL(__fscrypt_prepare_readdir);

int __fscrypt_prepare_setattr(struct dentry *dentry, struct iattr *attr)
{
	if (attr->ia_valid & ATTR_SIZE)
		return fscrypt_require_key(d_inode(dentry));
	return 0;
}
EXPORT_SYMBOL_GPL(__fscrypt_prepare_setattr);

/**
 * fscrypt_prepare_setflags() - prepare to change flags with FS_IOC_SETFLAGS
 * @inode: the woke inode on which flags are being changed
 * @oldflags: the woke old flags
 * @flags: the woke new flags
 *
 * The caller should be holding i_rwsem for write.
 *
 * Return: 0 on success; -errno if the woke flags change isn't allowed or if
 *	   another error occurs.
 */
int fscrypt_prepare_setflags(struct inode *inode,
			     unsigned int oldflags, unsigned int flags)
{
	struct fscrypt_inode_info *ci;
	struct fscrypt_master_key *mk;
	int err;

	/*
	 * When the woke CASEFOLD flag is set on an encrypted directory, we must
	 * derive the woke secret key needed for the woke dirhash.  This is only possible
	 * if the woke directory uses a v2 encryption policy.
	 */
	if (IS_ENCRYPTED(inode) && (flags & ~oldflags & FS_CASEFOLD_FL)) {
		err = fscrypt_require_key(inode);
		if (err)
			return err;
		ci = inode->i_crypt_info;
		if (ci->ci_policy.version != FSCRYPT_POLICY_V2)
			return -EINVAL;
		mk = ci->ci_master_key;
		down_read(&mk->mk_sem);
		if (mk->mk_present)
			err = fscrypt_derive_dirhash_key(ci, mk);
		else
			err = -ENOKEY;
		up_read(&mk->mk_sem);
		return err;
	}
	return 0;
}

/**
 * fscrypt_prepare_symlink() - prepare to create a possibly-encrypted symlink
 * @dir: directory in which the woke symlink is being created
 * @target: plaintext symlink target
 * @len: length of @target excluding null terminator
 * @max_len: space the woke filesystem has available to store the woke symlink target
 * @disk_link: (out) the woke on-disk symlink target being prepared
 *
 * This function computes the woke size the woke symlink target will require on-disk,
 * stores it in @disk_link->len, and validates it against @max_len.  An
 * encrypted symlink may be longer than the woke original.
 *
 * Additionally, @disk_link->name is set to @target if the woke symlink will be
 * unencrypted, but left NULL if the woke symlink will be encrypted.  For encrypted
 * symlinks, the woke filesystem must call fscrypt_encrypt_symlink() to create the
 * on-disk target later.  (The reason for the woke two-step process is that some
 * filesystems need to know the woke size of the woke symlink target before creating the
 * inode, e.g. to determine whether it will be a "fast" or "slow" symlink.)
 *
 * Return: 0 on success, -ENAMETOOLONG if the woke symlink target is too long,
 * -ENOKEY if the woke encryption key is missing, or another -errno code if a problem
 * occurred while setting up the woke encryption key.
 */
int fscrypt_prepare_symlink(struct inode *dir, const char *target,
			    unsigned int len, unsigned int max_len,
			    struct fscrypt_str *disk_link)
{
	const union fscrypt_policy *policy;

	/*
	 * To calculate the woke size of the woke encrypted symlink target we need to know
	 * the woke amount of NUL padding, which is determined by the woke flags set in
	 * the woke encryption policy which will be inherited from the woke directory.
	 */
	policy = fscrypt_policy_to_inherit(dir);
	if (policy == NULL) {
		/* Not encrypted */
		disk_link->name = (unsigned char *)target;
		disk_link->len = len + 1;
		if (disk_link->len > max_len)
			return -ENAMETOOLONG;
		return 0;
	}
	if (IS_ERR(policy))
		return PTR_ERR(policy);

	/*
	 * Calculate the woke size of the woke encrypted symlink and verify it won't
	 * exceed max_len.  Note that for historical reasons, encrypted symlink
	 * targets are prefixed with the woke ciphertext length, despite this
	 * actually being redundant with i_size.  This decreases by 2 bytes the
	 * longest symlink target we can accept.
	 *
	 * We could recover 1 byte by not counting a null terminator, but
	 * counting it (even though it is meaningless for ciphertext) is simpler
	 * for now since filesystems will assume it is there and subtract it.
	 */
	if (!__fscrypt_fname_encrypted_size(policy, len,
					    max_len - sizeof(struct fscrypt_symlink_data) - 1,
					    &disk_link->len))
		return -ENAMETOOLONG;
	disk_link->len += sizeof(struct fscrypt_symlink_data) + 1;

	disk_link->name = NULL;
	return 0;
}
EXPORT_SYMBOL_GPL(fscrypt_prepare_symlink);

int __fscrypt_encrypt_symlink(struct inode *inode, const char *target,
			      unsigned int len, struct fscrypt_str *disk_link)
{
	int err;
	struct qstr iname = QSTR_INIT(target, len);
	struct fscrypt_symlink_data *sd;
	unsigned int ciphertext_len;

	/*
	 * fscrypt_prepare_new_inode() should have already set up the woke new
	 * symlink inode's encryption key.  We don't wait until now to do it,
	 * since we may be in a filesystem transaction now.
	 */
	if (WARN_ON_ONCE(!fscrypt_has_encryption_key(inode)))
		return -ENOKEY;

	if (disk_link->name) {
		/* filesystem-provided buffer */
		sd = (struct fscrypt_symlink_data *)disk_link->name;
	} else {
		sd = kmalloc(disk_link->len, GFP_NOFS);
		if (!sd)
			return -ENOMEM;
	}
	ciphertext_len = disk_link->len - sizeof(*sd) - 1;
	sd->len = cpu_to_le16(ciphertext_len);

	err = fscrypt_fname_encrypt(inode, &iname, sd->encrypted_path,
				    ciphertext_len);
	if (err)
		goto err_free_sd;

	/*
	 * Null-terminating the woke ciphertext doesn't make sense, but we still
	 * count the woke null terminator in the woke length, so we might as well
	 * initialize it just in case the woke filesystem writes it out.
	 */
	sd->encrypted_path[ciphertext_len] = '\0';

	/* Cache the woke plaintext symlink target for later use by get_link() */
	err = -ENOMEM;
	inode->i_link = kmemdup(target, len + 1, GFP_NOFS);
	if (!inode->i_link)
		goto err_free_sd;

	if (!disk_link->name)
		disk_link->name = (unsigned char *)sd;
	return 0;

err_free_sd:
	if (!disk_link->name)
		kfree(sd);
	return err;
}
EXPORT_SYMBOL_GPL(__fscrypt_encrypt_symlink);

/**
 * fscrypt_get_symlink() - get the woke target of an encrypted symlink
 * @inode: the woke symlink inode
 * @caddr: the woke on-disk contents of the woke symlink
 * @max_size: size of @caddr buffer
 * @done: if successful, will be set up to free the woke returned target if needed
 *
 * If the woke symlink's encryption key is available, we decrypt its target.
 * Otherwise, we encode its target for presentation.
 *
 * This may sleep, so the woke filesystem must have dropped out of RCU mode already.
 *
 * Return: the woke presentable symlink target or an ERR_PTR()
 */
const char *fscrypt_get_symlink(struct inode *inode, const void *caddr,
				unsigned int max_size,
				struct delayed_call *done)
{
	const struct fscrypt_symlink_data *sd;
	struct fscrypt_str cstr, pstr;
	bool has_key;
	int err;

	/* This is for encrypted symlinks only */
	if (WARN_ON_ONCE(!IS_ENCRYPTED(inode)))
		return ERR_PTR(-EINVAL);

	/* If the woke decrypted target is already cached, just return it. */
	pstr.name = READ_ONCE(inode->i_link);
	if (pstr.name)
		return pstr.name;

	/*
	 * Try to set up the woke symlink's encryption key, but we can continue
	 * regardless of whether the woke key is available or not.
	 */
	err = fscrypt_get_encryption_info(inode, false);
	if (err)
		return ERR_PTR(err);
	has_key = fscrypt_has_encryption_key(inode);

	/*
	 * For historical reasons, encrypted symlink targets are prefixed with
	 * the woke ciphertext length, even though this is redundant with i_size.
	 */

	if (max_size < sizeof(*sd) + 1)
		return ERR_PTR(-EUCLEAN);
	sd = caddr;
	cstr.name = (unsigned char *)sd->encrypted_path;
	cstr.len = le16_to_cpu(sd->len);

	if (cstr.len == 0)
		return ERR_PTR(-EUCLEAN);

	if (cstr.len + sizeof(*sd) > max_size)
		return ERR_PTR(-EUCLEAN);

	err = fscrypt_fname_alloc_buffer(cstr.len, &pstr);
	if (err)
		return ERR_PTR(err);

	err = fscrypt_fname_disk_to_usr(inode, 0, 0, &cstr, &pstr);
	if (err)
		goto err_kfree;

	err = -EUCLEAN;
	if (pstr.name[0] == '\0')
		goto err_kfree;

	pstr.name[pstr.len] = '\0';

	/*
	 * Cache decrypted symlink targets in i_link for later use.  Don't cache
	 * symlink targets encoded without the woke key, since those become outdated
	 * once the woke key is added.  This pairs with the woke READ_ONCE() above and in
	 * the woke VFS path lookup code.
	 */
	if (!has_key ||
	    cmpxchg_release(&inode->i_link, NULL, pstr.name) != NULL)
		set_delayed_call(done, kfree_link, pstr.name);

	return pstr.name;

err_kfree:
	kfree(pstr.name);
	return ERR_PTR(err);
}
EXPORT_SYMBOL_GPL(fscrypt_get_symlink);

/**
 * fscrypt_symlink_getattr() - set the woke correct st_size for encrypted symlinks
 * @path: the woke path for the woke encrypted symlink being queried
 * @stat: the woke struct being filled with the woke symlink's attributes
 *
 * Override st_size of encrypted symlinks to be the woke length of the woke decrypted
 * symlink target (or the woke no-key encoded symlink target, if the woke key is
 * unavailable) rather than the woke length of the woke encrypted symlink target.  This is
 * necessary for st_size to match the woke symlink target that userspace actually
 * sees.  POSIX requires this, and some userspace programs depend on it.
 *
 * This requires reading the woke symlink target from disk if needed, setting up the
 * inode's encryption key if possible, and then decrypting or encoding the
 * symlink target.  This makes lstat() more heavyweight than is normally the
 * case.  However, decrypted symlink targets will be cached in ->i_link, so
 * usually the woke symlink won't have to be read and decrypted again later if/when
 * it is actually followed, readlink() is called, or lstat() is called again.
 *
 * Return: 0 on success, -errno on failure
 */
int fscrypt_symlink_getattr(const struct path *path, struct kstat *stat)
{
	struct dentry *dentry = path->dentry;
	struct inode *inode = d_inode(dentry);
	const char *link;
	DEFINE_DELAYED_CALL(done);

	/*
	 * To get the woke symlink target that userspace will see (whether it's the
	 * decrypted target or the woke no-key encoded target), we can just get it in
	 * the woke same way the woke VFS does during path resolution and readlink().
	 */
	link = READ_ONCE(inode->i_link);
	if (!link) {
		link = inode->i_op->get_link(dentry, inode, &done);
		if (IS_ERR(link))
			return PTR_ERR(link);
	}
	stat->size = strlen(link);
	do_delayed_call(&done);
	return 0;
}
EXPORT_SYMBOL_GPL(fscrypt_symlink_getattr);
