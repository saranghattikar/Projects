#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/**
 * structure for indirect data blocks
 */
struct indirect_data_block {
	block_sector_t blocks[INDIRECT_BLOCKS_PTRS];
};

/*
 * In-memory inode structure.
 */
struct inode {
	struct list_elem elem; /* Element in inode list. */
	block_sector_t sector; /* Sector number of disk location. */
	int open_cnt; /* Number of openers. */
	bool removed; /* True if deleted, false otherwise. */
	int deny_write_cnt; /* 0: writes ok, >0: deny writes. */
	off_t length; /* inode size in bytes. */
	off_t read_length;
	size_t direct; /* direct data block size */
	size_t indirect; /* indirect data block size */
	size_t double_indirect; /* double indirect data block size */
	bool is_dir; /* is this directory or file */
	block_sector_t parent_inode; /* reference to parent' inode */
	struct lock lock; /* individual inode specific lock */
	block_sector_t blocks[INODE_BLOCKS_PTRS]; /* data block pointers */
};

/*
 * On-disk inode structure.
 * Must be exactly BLOCK_SECTOR_SIZE bytes long.
 */
struct inode_disk {
	off_t length; /* File size in bytes. */
	unsigned magic; /* Magic number. */
	uint32_t unused[107]; /* Not used. */
	bool is_dir; /* is this directory or file */
	block_sector_t parent_inode; /* reference to parent' inode */
	uint32_t direct; /* direct data block pointer */
	uint32_t indirect; /* indirect data block pointer */
	uint32_t double_indirect; /* double indirect data block pointer */
	block_sector_t blocks[INODE_BLOCKS_PTRS]; /* data blocks - stores pointers to them */
};

/* List of open inodes, so that opening a single inode twice, returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void inode_init(void) {
	list_init(&open_inodes);
}

/* Function declarations */
bool create_inode(struct inode_disk *disk_inode);

/**
 * Returns the number of sectors to allocate for an inode SIZE bytes long.
 */
static inline size_t bytes_to_sectors(off_t size) {
	return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE);
}

/* Reopens and returns the INODE. */
struct inode * inode_reopen(struct inode *inode) {
	if (inode != NULL)
		inode->open_cnt++;
	return inode;
}

/**
 * Get inode number.
 */
block_sector_t get_inumber(const struct inode *inode) {
	return inode->sector;
}

/**
 * Check if given inode is directory or file
 */
bool is_inode_dir(const struct inode *inode) {
	return inode->is_dir;
}

/**
 * Check inode is opened by how many processes
 */
int get_inode_open_cnt(const struct inode *inode) {
	return inode->open_cnt;
}

/**
 * Get parent-inode of this inode
 */
block_sector_t get_inode_parent(const struct inode *inode) {
	return inode->parent_inode;
}

/**
 * Acquire inode's locks
 */
void acquire_inode_lock(const struct inode *inode) {
	lock_acquire(&((struct inode *) inode)->lock);
}

/**
 * release the inode's lock
 */
void release_inode_lock(const struct inode *inode) {
	lock_release(&((struct inode *) inode)->lock);
}

/* Disables writes to INODE.
 May be called at most once per inode opener. */
void inode_deny_write(struct inode *inode) {
	inode->deny_write_cnt++;
	ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
 Must be called once by each inode opener who has called
 inode_deny_write() on the inode, before closing the inode. */
void inode_allow_write(struct inode *inode) {
	ASSERT(inode->deny_write_cnt > 0);
	ASSERT(inode->deny_write_cnt <= inode->open_cnt);
	inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t inode_length(const struct inode *inode) {
	return inode->length;
}

/**
 * Returns total number of indirect sectors to allocate for an inode SIZE bytes long.
 */
static size_t bytes_to_indirect_data_sectors(off_t size) {
	// If size is less than total direct blocks size, then no need of indirect blocks
	if (size <= BLOCK_SECTOR_SIZE * DIRECT_BLOCK_PTRS) {
		return 0;
	}
	size -= BLOCK_SECTOR_SIZE * DIRECT_BLOCK_PTRS;
	return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE*INDIRECT_BLOCKS_PTRS);
}

/**
 * Returns total number of double indirect sectors to allocate for an inode SIZE bytes long.
 */
static size_t bytes_to_double_indirect_data_sector(off_t size) {
	// If size is less than total direct blocks + indirect data blocks size, then no need of double indirect blocks
	if (size <= BLOCK_SECTOR_SIZE * (DIRECT_BLOCK_PTRS +
	INODE_BLOCKS_PTRS * INDIRECT_BLOCKS_PTRS)) {
		return 0;
	}
	// At max we allow only DOUBLE_INDIRECT_BLOCK_PTRS i.e. 1 double indirect blocks
	return DOUBLE_INDIRECT_BLOCK_PTRS;
}

/**
 * Free-up all doubly indirectly allocated inode data blocks -
 * by resetting their corresponding bitmap bits
 */
void delete_double_indirect_block(block_sector_t *ptr, size_t indirect_ptrs,
		size_t data_ptrs) {
	struct indirect_data_block block;
	block_read(fs_device, *ptr, &block);
	int block_index = 0;
	for (; (block_index < indirect_ptrs); block_index++) {
		size_t data_per_block = INDIRECT_BLOCKS_PTRS;
		if (data_ptrs < INDIRECT_BLOCKS_PTRS) {
			data_per_block = data_ptrs;
		}
		// delete_indirect_blocks(&block.blocks[block_index], data_per_block);

		struct indirect_data_block indirect_block;
		block_read(fs_device, block.blocks[block_index], &indirect_block);
		int indirect_idx = 0;
		for (; indirect_idx < data_per_block;) {
			free_map_release(indirect_block.blocks[indirect_idx++], 1);
		}
		free_map_release(indirect_block.blocks[block_index], 1);
		data_ptrs -= data_per_block;
	}
	free_map_release(*ptr, 1);
}

/**
 * Release indirect and doubly indirect blocks
 */
void release_indirect_doubly_indirect_blocks(struct inode *inode,
		int block_number, size_t direct_blocks) {

	// Size of indirect data blocks
	size_t indirect_blocks = bytes_to_indirect_data_sectors(inode->length);
	for (;
			((block_number < (DIRECT_BLOCK_PTRS + INDIRECT_BLOCKS_PTRS))
					&& (indirect_blocks > 0));
			block_number++, indirect_blocks--) {

		size_t block_count = 0;
		if (direct_blocks >= INDIRECT_BLOCKS_PTRS) {
			block_count = direct_blocks;
		} else {
			block_count = INDIRECT_BLOCKS_PTRS;
		}
		direct_blocks -= block_count;
		// delete_indirect_blocks(&inode->blocks[block_number], block_count);

		struct indirect_data_block block;
		block_read(fs_device, inode->blocks[block_number], &block);
		int indirect_index = 0;
		for (; (indirect_index < block_count);) {
			free_map_release(block.blocks[indirect_index++], 1);
		}
		free_map_release(inode->blocks[block_number], 1);
	}

	// if there is any doubly indirect data block
	if (bytes_to_double_indirect_data_sector(inode->length) > 0) {
		delete_double_indirect_block(&inode->blocks[block_number],
				indirect_blocks, direct_blocks);
	}
}

/**
 * delete inode -- free-up all - direct, indirect and doubly-indirect data blocks
 */
void delete_inode(struct inode *inode) {
	// Take the size of direct data-blocks
	size_t direct_blocks = bytes_to_sectors(inode->length);

	// Release one by one - all blocks - direct, indirect and doubly indirect
	int block_number = 0;
	//  free up all the data blocks - direct data blocks
	for (; ((block_number < DIRECT_BLOCK_PTRS) && (direct_blocks > 0));
			direct_blocks--, block_number++) {
		free_map_release(inode->blocks[block_number], 1);
	}

	// release the remaining indirect and doubly indirect data blocks
	release_indirect_doubly_indirect_blocks(&inode, block_number,
			direct_blocks);
}

/**
 * Returns the block device sector that contains byte offset POS within INODE.
 * Returns -1 if INODE does not contain data for a byte at offset POS.
 */
static block_sector_t byte_to_sector(const struct inode *inode, off_t pos,
		off_t length) {
	if (pos >= length) {
		return -1;
	}
	uint32_t block_index;
	uint32_t indirect_data_block[INDIRECT_BLOCKS_PTRS];
	ASSERT(inode != NULL);
	// If pos is within direct data blocks range
	if (pos < BLOCK_SECTOR_SIZE * DIRECT_BLOCK_PTRS) {
		return inode->blocks[pos / BLOCK_SECTOR_SIZE];
	}
	// if position is within indirect data blocks range
	else if (pos < BLOCK_SECTOR_SIZE * (DIRECT_BLOCK_PTRS +
	INODE_BLOCKS_PTRS * INDIRECT_BLOCKS_PTRS)) {
		pos -= BLOCK_SECTOR_SIZE * DIRECT_BLOCK_PTRS;
		block_index = pos
				/ (BLOCK_SECTOR_SIZE * INDIRECT_BLOCKS_PTRS)+ DIRECT_BLOCK_PTRS;
		block_read(fs_device, inode->blocks[block_index], &indirect_data_block);
		pos %= BLOCK_SECTOR_SIZE * INDIRECT_BLOCKS_PTRS;
		return indirect_data_block[pos / BLOCK_SECTOR_SIZE];
	}
	// in double indirect block range
	else {
		block_read(fs_device,
				inode->blocks[(DIRECT_BLOCK_PTRS + INDIRECT_BLOCKS_PTRS)],
				&indirect_data_block);
		pos -= BLOCK_SECTOR_SIZE * (DIRECT_BLOCK_PTRS +
		INODE_BLOCKS_PTRS * INDIRECT_BLOCKS_PTRS);
		block_index = pos / (BLOCK_SECTOR_SIZE * INDIRECT_BLOCKS_PTRS);
		block_read(fs_device, indirect_data_block[block_index],
				&indirect_data_block);
		pos %= BLOCK_SECTOR_SIZE * INDIRECT_BLOCKS_PTRS;
		return indirect_data_block[pos / BLOCK_SECTOR_SIZE];
	}
}

/* Initializes an inode with LENGTH bytes of data and
 writes the new inode to sector SECTOR on the file system
 device.
 Returns true if successful.
 Returns false if memory or disk allocation fails. */
bool inode_create(block_sector_t sector, off_t length, bool is_dir) {
	struct inode_disk *disk_inode = NULL;
	bool success = false;

	ASSERT(length >= 0);

	/* If this assertion fails, the inode structure is not exactly
	 one sector in size, and you should fix that. */
	ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

	// allocate a new disk_inode type object
	disk_inode = calloc(1, sizeof *disk_inode);

	if (disk_inode != NULL) {
		// Magic number for currpution prevention
		disk_inode->magic = INODE_MAGIC;
		// is this a directory
		disk_inode->is_dir = is_dir;
		// Default parent of this inode is root sector
		disk_inode->parent_inode = ROOT_DIR_SECTOR;
		// file length on disk cannot exceed the 8MB hard limit - as our partition size is 8 MB
		disk_inode->length = (disk_inode->length > MAX_FILE_SIZE) ?
		MAX_FILE_SIZE :
																	length;
		// allocate a new inode on disk
		if (create_inode(disk_inode)) {
			// Write this inode to disk
			block_write(fs_device, sector, disk_inode);
			success = true;
		}
		free(disk_inode);
	}
	return success;
}

/**
 * Reads an inode from SECTOR and returns a `struct inode' that contains it.
 * Returns a null pointer if memory allocation fails.
 */
struct inode * inode_open(block_sector_t sector) {
	struct list_elem *e;
	struct inode *inode;

	/* Check whether this inode is already open. */
	for (e = list_begin(&open_inodes); e != list_end(&open_inodes); e =
			list_next(e)) {
		inode = list_entry(e, struct inode, elem);
		if (inode->sector == sector) {
			inode_reopen(inode);
			return inode;
		}
	}

	/* Allocate memory. */
	inode = malloc(sizeof *inode);
	if (inode == NULL)
		return NULL;

	/* Initialize. */
	list_push_front(&open_inodes, &inode->elem);
	inode->sector = sector;
	inode->open_cnt = 1;
	inode->deny_write_cnt = 0;
	inode->removed = false;

	lock_init(&inode->lock);

	struct inode_disk data;
	// Read the disk inode from sector
	block_read(fs_device, inode->sector, &data);
	// Copy the data blocks - to in-memory inode
	memcpy(&inode->blocks, &data.blocks,
	INODE_BLOCKS_PTRS * sizeof(block_sector_t));

	// set inode's parent inode
	inode->parent_inode = data.parent_inode;
	// is this a directory or a file
	inode->is_dir = data.is_dir;
	// length of current inode file
	inode->read_length = data.length;
	inode->length = data.length;

	// Current sizes of all direct, indirect data blocks
	inode->direct = data.direct;
	inode->indirect = data.indirect;
	inode->double_indirect = data.double_indirect;

	return inode;
}

/**
 * add parent inode info to this indoe
 */
bool inode_add_parent(block_sector_t parent_sector, block_sector_t child_sector) {
	struct inode* inode = inode_open(child_sector);
	if (!inode) {
		return false;
	}
	inode->parent_inode = parent_sector;
	inode_close(inode);
	return true;
}

/**
 *  Closes INODE and writes it to disk.
 *  If this was the last reference to INODE, frees its memory.
 *  If INODE was also a removed inode, frees its blocks.
 */
void inode_close(struct inode *inode) {
	/* Ignore null pointer. */
	if (inode == NULL)
		return;

	/* Release resources if this was the last opener. */
	if (--inode->open_cnt == 0) {
		/* Remove from inode list and release lock. */
		list_remove(&inode->elem);

		/* Deallocate blocks if removed. */
		if (!inode->removed) {
			// create a blank new object for on-disk inode
			struct inode_disk disk_inode = { };
			disk_inode.direct = inode->direct;
			disk_inode.indirect = inode->indirect;
			disk_inode.double_indirect = inode->double_indirect;
			// Magic number for corruption prevention
			disk_inode.magic = INODE_MAGIC;
			// is this a directory
			disk_inode.is_dir = inode->is_dir;
			// Default parent of this inode is root sector
			disk_inode.parent_inode = inode->parent_inode;
			// file length on disk cannot exceed the 8MB hard limit - as our partition size is 8 MB
			disk_inode.length = inode->length;

			// Copy any changes in data-blocks to on-disk inode
			memcpy(&disk_inode.blocks, &inode->blocks,
			INODE_BLOCKS_PTRS * sizeof(block_sector_t));

			// Write back to on-disk indoe
			block_write(fs_device, inode->sector, &disk_inode);
		} else {
			// release the inode - clear the bitmap bit
			free_map_release(inode->sector, 1);
			// delete the given inode
			delete_inode(inode);
		}

		free(inode);
	}
}

/* Marks INODE to be deleted when it is closed by the last caller who
 has it open. */
void inode_remove(struct inode *inode) {
	ASSERT(inode != NULL);
	inode->removed = true;
}

/**
 * alloc_first_level_double_indirect_data_block
 * Allocate second level double indirect data blocks
 */
size_t alloc_second_level_double_indirect_data_block(struct inode *o,
		size_t new_required_data_blocks_to_allocate,
		struct indirect_data_block* upper_lvel_block) {

	// Check if its already have double indirect blacks
	struct indirect_data_block inner_block;
	if (o->double_indirect != 0) {
		block_read(fs_device, upper_lvel_block->blocks[o->indirect],
						&inner_block);
	} else {
		free_map_allocate(1, &upper_lvel_block->blocks[o->indirect]);
	}

	// Allocate at max 128
	static char block_of_zeros[BLOCK_SECTOR_SIZE];
	while (o->double_indirect < INDIRECT_BLOCKS_PTRS) {
		free_map_allocate(1, &inner_block.blocks[o->double_indirect]);
		block_write(fs_device, inner_block.blocks[o->double_indirect],
				block_of_zeros);

		new_required_data_blocks_to_allocate = new_required_data_blocks_to_allocate - 1;
		o->double_indirect = o->double_indirect + 1;
		if (new_required_data_blocks_to_allocate == 0) {
			break;
		}
	}
	// Append the last block
	block_write(fs_device, upper_lvel_block->blocks[o->indirect], &inner_block);
	if (o->double_indirect == INDIRECT_BLOCKS_PTRS) {
		o->indirect = o->indirect  +1 ;
		o->double_indirect = 0;
	}
	return new_required_data_blocks_to_allocate;
}

/**
 * Allocates first level of double indirect data block
 */
size_t alloc_first_level_double_indirect_data_block(struct inode *e,
		size_t new_required_data_blocks_to_allocate) {

	// Allocate first level of double indirect block
	struct indirect_data_block b;
	if (!(e->double_indirect == 0 && e->indirect == 0)) {
		block_read(fs_device, e->blocks[e->direct], &b);
	} else {
		free_map_allocate(1, &e->blocks[e->direct]);
	}


	// Allocate till all available double indirect blocks
	while (e->indirect < INDIRECT_BLOCKS_PTRS) {
		new_required_data_blocks_to_allocate = alloc_second_level_double_indirect_data_block(e,
				new_required_data_blocks_to_allocate, &b);
		if (new_required_data_blocks_to_allocate == 0) {
			break;
		}
	}

	// write the last block
	block_write(fs_device, e->blocks[e->direct], &b);
	return new_required_data_blocks_to_allocate;
}
/**
 * alloc_first_level_indirect_blocks
 * Allocate new data blocks at first indirect level
 */
size_t alloc_first_level_indirect_blocks(struct inode *i, size_t new_required_data_blocks_to_allocate) {
	struct indirect_data_block b;
	if (i->indirect != 0) {
		block_read(fs_device, i->blocks[i->direct], &b);
	} else {
		free_map_allocate(1, &i->blocks[i->direct]);
	}

	static char block_of_zeros[BLOCK_SECTOR_SIZE];
	while (i->indirect < INDIRECT_BLOCKS_PTRS) {
		free_map_allocate(1, &b.blocks[i->indirect]);
		block_write(fs_device, b.blocks[i->indirect], block_of_zeros);
		i->indirect = i->indirect + 1;
		new_required_data_blocks_to_allocate = new_required_data_blocks_to_allocate - 1;
		if (new_required_data_blocks_to_allocate == 0) {
			break;
		}
	}

	block_write(fs_device, i->blocks[i->direct], &b);
	if (i->indirect == INDIRECT_BLOCKS_PTRS) {
		i->direct =i->direct + 1;
		i->indirect = 0;
	}
	return new_required_data_blocks_to_allocate;
}

/**
 * Expand the inode
 */
off_t append_to_inode(struct inode *n, off_t length) {
	/*
	 // For indirect and doubly indirect blocks allocations
	 int new_blocks_length = allocate_for_indirect_and_doubly_indirect_blocks(
	 &inode, length, new_required_data_blocks_to_allocate);
	 return (length - new_blocks_length);
	 */
	int new_required_data_blocks_to_allocate = bytes_to_sectors(length)
			- bytes_to_sectors(n->length);
	// If no increase in inode size then return the length itself
	if (new_required_data_blocks_to_allocate == 0) {
		return length;
	}
	// When expanding the n - data between the new length index
	// and previous EOF index should be zeroed
	static char block_of_zeros[BLOCK_SECTOR_SIZE];
	while (n->direct < DIRECT_BLOCK_PTRS) {
		// First free the block and then write it back to disk
		free_map_allocate(1, &n->blocks[n->direct]);
		// write block to partition
		block_write(fs_device, n->blocks[n->direct], block_of_zeros);
		// check if all required blocks for appending to inode have been allocated
		new_required_data_blocks_to_allocate =
				new_required_data_blocks_to_allocate - 1;
		n->direct = n->direct + 1;
		if (new_required_data_blocks_to_allocate == 0) {
			return length;
		}
	}

	// For Indirect connected blocks
	while (n->direct < (DIRECT_BLOCK_PTRS + INDIRECT_BLOCKS_PTRS)) {
		new_required_data_blocks_to_allocate = alloc_first_level_indirect_blocks(n,
				new_required_data_blocks_to_allocate);
		if (new_required_data_blocks_to_allocate == 0) {
			return length;
		}
	}

	// For Doubly indirect blocks
	if (n->direct == (DIRECT_BLOCK_PTRS + INDIRECT_BLOCKS_PTRS)) {
		new_required_data_blocks_to_allocate =
				alloc_first_level_double_indirect_data_block(n,
						new_required_data_blocks_to_allocate);
	}

	// Whats the new size of newly allocated block
	off_t total_allocated_length_size = (new_required_data_blocks_to_allocate
			* BLOCK_SECTOR_SIZE);
	off_t result = (length - total_allocated_length_size);
	// Return total actually allocated blocks count
	return result;
}

/**
 * create new inode of size zero, and then expand it for given disk_inode's size
 */
bool create_inode(struct inode_disk *di) {
	// Create an empty in-memory inode object
	struct inode inode = { };
	inode.direct = 0;
	inode.indirect = 0;
	inode.double_indirect = 0;
	inode.length = 0;

	// expand the inode for given length
	append_to_inode(&inode, di->length);
	di->direct = inode.direct;
	di->indirect = inode.indirect;
	di->double_indirect = inode.double_indirect;
	memcpy(&di->blocks, &inode.blocks,
	INODE_BLOCKS_PTRS * sizeof(block_sector_t));
	return true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
 Returns the number of bytes actually read, which may be less
 than SIZE if an error occurs or end of file is reached. */
off_t inode_read_at(struct inode *inode, void *buffer_, off_t size,
		off_t offset) {
	uint8_t *buffer = buffer_;
	off_t bytes_read = 0;

	off_t inode_length = inode->read_length;

	if (offset < inode_length) {
		while (size > 0) {
			/* Disk sector to read, starting byte offset within sector. */
			block_sector_t sector_idx = byte_to_sector(inode, offset,
					inode_length);
			int sector_ofs = offset % BLOCK_SECTOR_SIZE;

			/* Bytes left in inode, bytes left in sector, lesser of the two. */
			off_t inode_left = inode_length - offset;
			int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
			int min_left = inode_left < sector_left ? inode_left : sector_left;

			/* Number of bytes to actually copy out of this sector. */
			int chunk_size = size < min_left ? size : min_left;
			if (chunk_size <= 0)
				break;

			// read it in blocks cache
			struct cache_element_entry *cache_element = get_cache(sector_idx,
			true);
			cache_element->is_cache_accessed = true;
			cache_element->counter--;
			memcpy(buffer + bytes_read,
					(uint8_t *) &cache_element->cache_block + sector_ofs,
					chunk_size);

			/* Advance. */
			size -= chunk_size;
			offset += chunk_size;
			bytes_read += chunk_size;
		}
	}

	return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
 Returns the number of bytes actually written, which may be
 less than SIZE if end of file is reached or an error occurs.
 (Normally a write at end of file would extend the inode, but
 growth is not yet implemented.) */
off_t inode_write_at(struct inode *inode, const void *buffer_, off_t size,
		off_t offset) {
	if (inode->deny_write_cnt) {
		return 0;
	}
	const uint8_t *buffer = buffer_;
	off_t bytes_written = 0;

	if ((offset + size) > inode->length) {
		if (!inode->is_dir) {
			acquire_inode_lock(inode);
			inode->length = append_to_inode(inode, offset + size);
			release_inode_lock(inode);
		} else {
			inode->length = append_to_inode(inode, offset + size);
		}
	}

	while (size > 0) {
		/* Sector to write, starting byte offset within sector. */
		block_sector_t sector_idx = byte_to_sector(inode, offset,
				inode->length);
		int sector_ofs = offset % BLOCK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode->length - offset;
		int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually write into this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		struct cache_element_entry *cache_element = get_cache(sector_idx,
		false);
		cache_element->is_cache_accessed = true;
		cache_element->is_cache_dirty = true;
		cache_element->counter--;
		memcpy((uint8_t *) &cache_element->cache_block + sector_ofs,
				buffer + bytes_written, chunk_size);

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_written += chunk_size;
	}

	inode->read_length = inode->length;
	return bytes_written;
}
