#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"
#include "threads/malloc.h"

#include "filesys/cache.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format(void);

/*
 * Initializes the file system module.
 * If format flag is true, reformats the file system.
 */
void filesys_init(bool format) {
	fs_device = block_get_role(BLOCK_FILESYS);
	if (fs_device == NULL)
		PANIC("No file system device found, can't initialize file system.");

	inode_init();
	// Initialize file system cache
	cache_init();
	// bitmap of free blocks - initialize
	free_map_init();

	if (format)
		do_format();

	free_map_open();
}

/**
 * Returns the parent or last container directory from the given path
 * e.g. if input path is '/test.c' - then returns directory '/'
 * e.g. if input path is 'test.c' - then returns process's current directory if not NULL
 * 		else root dir '/'
 * e.g. if input path is '/test/test2/test3.c' - then returns directory '/test/test2/'
 * e.g. if input path is 'test/test2/test3.c' -
 * 		then returns directory under '<current_directory>' --> 'test/test2/'
 */
struct dir* get_parent_container_dir_for_file(const char* file_path) {
	char *next_pointer, *next_token = NULL, *curr_token = NULL;
	char file_path_cpy[strlen(file_path) + 1];
	memcpy(file_path_cpy, file_path, strlen(file_path) + 1);

	// Parent directory where the given file resides
	struct dir* dir;

	// inode for directory
	struct inode *inode;

	// ASCII character for '/' is 47
	// Check if path is absolute path - i.e. start with '/',
	// Or process's current directory is null, then assume its root i.e. '/'
	if ((thread_current()->cur_dir == NULL)
			|| file_path_cpy[0] == FILE_PATH_SEPARATOR) {
		dir = dir_open(0, NULL , NULL);
	} else {
		// This will be used when use has given relative path i.e. this path is
		// with respect to process's current directory
		dir = dir_open(1, NULL, thread_current()->cur_dir);
	}

	// get the first token - from given path
	curr_token = strtok_r(file_path_cpy, "/", &next_pointer);

	// Get next token - i.e. if there is any
	if (curr_token) {
		next_token = strtok_r(NULL, "/", &next_pointer);
	}

	// Get each token from the file_path string and process it to get the directories
	// in hierarchical fashion
	for (; next_token != NULL;
			curr_token = next_token, next_token = strtok_r(NULL, "/",
					&next_pointer)) {
		// if token points to current directory then continue to next token
		if (strcmp(curr_token, ".") == 0) {
			continue;
		}

		// reset inode for the new subdirectory
		inode = NULL;

		// Check if its not call for - going to parent directory
		if (strcmp(curr_token, "..") != 0) {
			if (dir_lookup(dir, &inode, curr_token) == NULL) {
				// If there is no such subdirectory, meaning wrong file_path given
				// return NULL
				return NULL;
			}
		} else {
			// Check if there exists any parent directory
			if (get_parent_directory(dir, &inode) == NULL) {
				// if not, then user is trying to go beyond root '/'
				// return NULL
				return NULL;
			}
		}

		// From above obtained inode for sub-directory - check if its a file or directory
		if (!is_inode_dir(inode)) {
			// if file, close the inode and return NULL - because wrong file_path specified
			inode_close(inode);
			return NULL;
		} else {
			// if directory, close the previous parent's directory
			// and open current level directory using inode
			dir_close(dir);
			dir = dir_open(2, inode, NULL);
		}
	}
	return dir;
}

/**
 * Extract file name from given full file path
 */
char* extract_filename_from_file_path(const char* file_path) {
	char *curr_token, *save_ptr, *last_token = "";
	char file_path_cpy[strlen(file_path) + 1];
	// make a local copy of actual file_path - for local processing
	memcpy(file_path_cpy, file_path, strlen(file_path) + 1);

	// Tokenize the string over "/" and take the last token - that's the file_name
	for (curr_token = strtok_r(file_path_cpy, "/", &save_ptr);
			curr_token != NULL;
			last_token = curr_token, curr_token = strtok_r(NULL, "/", &save_ptr))
		// Do nothing in loop
		;

	// return the last token
	if (last_token == NULL) {
		return "";
	} else {
		char *file_name = malloc(strlen(last_token) + 1);
		memcpy(file_name, last_token, strlen(last_token) + 1);
		return file_name;
	}
}

/* Formats the file system. */
static void do_format(void) {
	printf("Formatting file system...");
	free_map_create();
	if (!dir_create(16, ROOT_DIR_SECTOR))
		PANIC("root directory creation failed");
	free_map_close();
	printf("done.\n");
}

/*
 * Shuts down the file system module, writing any unwritten data to disk.
 */
void filesys_done(void) {
	// Flush file system cache to disk
	flush_cache();
	// close free-bitmap file
	free_map_close();
}

/*
 * Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails.
 */
bool filesys_create(const char *file_path, off_t initial_size, bool isdir) {
	block_sector_t inode_sector = 0;
	struct dir *file_parent_dir = get_parent_container_dir_for_file(file_path);
	char* file_name = extract_filename_from_file_path(file_path);
	bool success = false;

	// Check that file name is not - . or .. which indicates directories and not actual files
	if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0) {
		// if directories then release the files
		dir_close(file_parent_dir);
		free(file_name);
		return false;
	} else {
		success = (file_parent_dir != NULL
				&& free_map_allocate(1, &inode_sector)
				&& inode_create(inode_sector, initial_size, isdir)
				&& dir_add(file_parent_dir, inode_sector, file_name));

		// If file_creation failed then release the allocated inode
		if (!success && inode_sector != 0) {
			free_map_release(inode_sector, 1);
		}

		// Close the directory and free file_name char array
		dir_close(file_parent_dir);
		free(file_name);
		return success;
	}
}

/*
 * Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer otherwise.
 * Fails if no file named NAME exists, or if an internal memory allocation fails.
 */
struct file * filesys_open(const char *file_path) {
	// If file_path is empty or NULL - no sense in going further
	if (file_path == NULL || strlen(file_path) == 0) {
		return NULL;
	}

	// Extract the parent container directory and file name from given file_path
	struct dir* file_parent_dir = get_parent_container_dir_for_file(file_path);
	char* file_name = extract_filename_from_file_path(file_path);
	struct inode *inode = NULL;
	bool result = false;

	if (file_parent_dir == NULL) {
		dir_close(file_parent_dir);
		free(file_name);
		return NULL;
	} else {
		if (strcmp(file_name, "..") == 0) {
			// if file_name - points to upper parent directory in hierarchy
			bool result = get_parent_directory(file_parent_dir, &inode);

			// Close the parent container directory and free file_name char array
			dir_close(file_parent_dir);
			free(file_name);
			if (result == false) {
				// if no such directory exists, i.e. file_path is trying
				// to refer above root "/" path, which is invalid
				return NULL;
			}
		} else if ((is_parent_directory(file_parent_dir) && strlen(file_name) == 0)
				|| strcmp(file_name, ".") == 0) {
			// Meaning file_name refers to the parent container directory itself
			free(file_name);
			return (struct file *) file_parent_dir;
		} else {
			// lokkup for given file_name inside this parent container directory
			dir_lookup(file_parent_dir, &inode, file_name);
			// Close the parent container directory and free file_name char array
			dir_close(file_parent_dir);
			free(file_name);
		}
	}

	// Check if inode is valid
	if (inode != NULL) {
		// if file_name is a directory then open and return it

		if (is_inode_dir(inode)) {
			return (struct file *) dir_open(2, inode, NULL);
		} else {
			return file_open(inode);
		}
	}
	return NULL;
}

/**
 * Change current process's working directory to given directory path
 */
bool filesys_chdir(const char *file_path) {
	struct dir* file_parent_dir = get_parent_container_dir_for_file(file_path);
	char* sub_dir_name = extract_filename_from_file_path(file_path);
	struct inode *inode = NULL;

	if (file_parent_dir == NULL) {
		dir_close(file_parent_dir);
		free(sub_dir_name);
		return false;
	}

	if (strcmp(sub_dir_name, "..") == 0) {
		// if sub_dir_name - points to upper parent directory in hierarchy
		bool result = get_parent_directory(file_parent_dir, &inode);

		// Close the parent container directory and free sub_dir_name char array
		dir_close(file_parent_dir);
		free(sub_dir_name);
		if (result == false) {
			// if no such directory exists, i.e. file_path is trying
			// to refer above root "/" path, which is invalid
			return false;
		}
	} else if ((is_parent_directory(file_parent_dir) && strlen(sub_dir_name) == 0)
			|| strcmp(sub_dir_name, ".") == 0) {
		// Meaning sub_dir_name refers to the parent container directory itself
		thread_current()->cur_dir = file_parent_dir;
		free(sub_dir_name);
		return true;
	} else {
		// lookup for given subdirectory inside this parent container directory
		dir_lookup(file_parent_dir, &inode, sub_dir_name);
		// Close the parent container directory and free sub_dir_name char array
		dir_close(file_parent_dir);
		free(sub_dir_name);
	}

	// Change the current process's  working directory with this new inode's directory
	file_parent_dir = dir_open(2, inode, NULL);
	if (file_parent_dir) {
		dir_close(thread_current()->cur_dir);
		thread_current()->cur_dir = file_parent_dir;
		return true;
	}
	return false;
}

/*
 * Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists, or if an internal memory allocation fails.
 */
bool filesys_remove(const char *file_path) {
	struct dir* file_parent_dir = get_parent_container_dir_for_file(file_path);
	char* file_name = extract_filename_from_file_path(file_path);
	bool success = false;
	if (file_parent_dir == NULL) {
		// Close the parent container directory and free file_name char array
		dir_close(file_parent_dir);
		free(file_name);
		return false;
	} else {
		success = dir_remove(file_parent_dir, file_name);
		// Close the parent container directory and free file_name char array
		dir_close(file_parent_dir);
		free(file_name);
		return success;
	}
}
