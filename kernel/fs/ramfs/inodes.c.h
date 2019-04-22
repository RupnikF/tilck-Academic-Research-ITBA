/* SPDX-License-Identifier: BSD-2-Clause */

static ramfs_inode *ramfs_new_inode(ramfs_data *d)
{
   ramfs_inode *i = kzmalloc(sizeof(ramfs_inode));

   if (!i)
      return NULL;

   rwlock_wp_init(&i->rwlock);
   i->inode = d->next_inode_num++;
   return i;
}

static ramfs_inode *
ramfs_create_inode_dir(ramfs_data *d, mode_t mode, ramfs_inode *parent)
{
   ramfs_inode *i = ramfs_new_inode(d);

   if (!i)
      return NULL;

   i->type = RAMFS_DIRECTORY;
   i->mode = (mode & 0777) | S_IFDIR;

   if (ramfs_dir_add_entry(i, ".", i) < 0) {
      kfree2(i, sizeof(ramfs_inode));
      return NULL;
   }

   if (!parent)
      parent = i;

   if (ramfs_dir_add_entry(i, "..", parent) < 0) {

      ramfs_entry *e = i->entries_tree_root;
      ramfs_dir_remove_entry(i, e);

      kfree2(i, sizeof(ramfs_inode));
      return NULL;
   }

   read_system_clock_datetime(&i->ctime);
   i->wtime = i->ctime;
   return i;
}

static ramfs_inode *
ramfs_create_inode_file(ramfs_data *d, mode_t mode, ramfs_inode *parent)
{
   ramfs_inode *i = ramfs_new_inode(d);

   if (!i)
      return NULL;

   i->type = RAMFS_FILE;
   i->mode = (mode & 0777) | S_IFREG;

   read_system_clock_datetime(&i->ctime);
   i->wtime = i->ctime;
   return i;
}

static int ramfs_destroy_inode(ramfs_data *d, ramfs_inode *i)
{
   /*
    * We can destroy only inodes referring to NO blocks (= data) in case of
    * files and no entries in case of directories. Also, we have to be SURE that
    * no dir entry nor file handle points to it.
    */
   ASSERT(get_ref_count(i) == 0);
   ASSERT(i->nlink == 0);

   if (i->type == RAMFS_DIRECTORY) {
      ASSERT(i->entries_tree_root == NULL);
   } else if (i->type == RAMFS_FILE) {
      ASSERT(i->blocks_tree_root == NULL);
   }

   rwlock_wp_destroy(&i->rwlock);
   kfree2(i, sizeof(ramfs_inode));
   return 0;
}
