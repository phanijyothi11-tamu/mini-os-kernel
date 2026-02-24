/*
	File: file_system.H

	Author: R. Bettati
			Department of Computer Science
			Texas A&M University
	Date  : 24/11/22

	Description: Simple File System.


*/

#ifndef _FILE_SYSTEM_H_ // include file only once
#define _FILE_SYSTEM_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "simple_disk.H"

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

class FileSystem;

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

class Inode
{
	friend class FileSystem; // The inode is in an uncomfortable position between
	friend class File;       // File System and File. We give both full access
	// to the Inode.

private:
	long id; // File "name"

	/* You will need additional information in the inode, such as allocation
	   information. */
	unsigned int length;       // File size in bytes
	int indirect_block; 		// Block number containing up to 128 data block pointers
	bool valid;                // Is this inode in use?

	FileSystem* fs; // It may be handy to have a pointer to the File system.
	// For example when you need a new block or when you want
	// to load or save the inode list. (Depends on your
	// implementation.)

/* You may need a few additional functions to help read and store the
   inodes from and to disk. */
public:
    Inode() : id(-1), length(0), indirect_block(-1), valid(false), fs(nullptr) {}

	void SaveToDisk(int index);
	void LoadFromDisk(int index);
};

/*--------------------------------------------------------------------------*/
/* FORWARD DECLARATIONS */
/*--------------------------------------------------------------------------*/

/* (none) */

/*--------------------------------------------------------------------------*/
/* F i l e S y s t e m  */
/*--------------------------------------------------------------------------*/

class FileSystem
{

	friend class Inode;

private:
	/* -- DEFINE YOUR FILE SYSTEM DATA STRUCTURES HERE. */

	SimpleDisk* disk;
	unsigned int size;

	static const int INODE_BLOCK = 0;
	static const int FREELIST_BLOCK = 1;
	// static const int DATA_BLOCK_START = 10; // Everything from here onwards can be used for data

	static constexpr unsigned int MAX_INODES = SimpleDisk::BLOCK_SIZE / sizeof(Inode);
	/* Just as an example; you can store MAX_INODES in a single INODES block */

	Inode* inodes; 
	/* The inode list */

	unsigned char* free_blocks;
	/* The free-block list. You may want to implement the "list" as a bitmap.
	   Feel free to use an unsigned char to represent whether a block is free or not;
	   no need to go to bits if you don't want to.
	   If you reserve one block to store the "free list", you can handle a file system up to
	   256kB. (Large enough for a proof of concept.) */

	   // short GetFreeInode();
	   // int GetFreeBlock();
	   /* It may be helpful to two functions to hand out free inodes in the inode list and free
		  blocks. These functions also come useful to class Inode and File. */

public:
	
	static const int DATA_BLOCK_START = 10; // Everything from here onwards can be used for data
	
	FileSystem();
	/* Just initializes local data structures. Does not connect to disk yet. */

	~FileSystem();
	/* Unmount file system if it has been mounted. */

	bool Mount(SimpleDisk* _disk);
	/* Associates this file system with a disk. Limit to at most one file system per disk.
	   Returns true if operation successful (i.e. there is indeed a file system on the disk.) */

	static bool Format(SimpleDisk* _disk, unsigned int _size);
	/* Wipes any file system from the disk and installs an empty file system of given size. */

	Inode* LookupFile(int _file_id);
	/* Find file with given id in file system. If found, return its inode.
		 Otherwise, return null. */

	bool CreateFile(int _file_id);
	/* Create file with given id in the file system. If file exists already,
	   abort and return false. Otherwise, return true. */

	bool DeleteFile(int _file_id);
	/* Delete file with given id in the file system; free any disk block occupied by the file. */

	void read_block_from_disk(int block_no, unsigned char* buffer);
	/* Reads a block from disk into the given buffer. */

	void write_block_to_disk(int block_no, unsigned char* buffer);
	/* Writes the contents of the given buffer to the specified disk block. */
	
	void write_inode_block_to_disk();
	// flushes inodes[] to INODE_BLOCK

	int get_disk_size() const;
	/* Returns the total number of blocks in the file system. */

    bool is_block_free(int block_no) const;
	/* Checks if a given block number is marked as free. */

    void mark_block_used(int block_no);
	/* Marks the specified block as used in the free block list. */

    void mark_block_free(int block_no);
	/* Marks the specified block as free in the free block list. */

	int GetFreeBlock();
	/* Returns the index of a free block; returns -1 if no free block is available. */

	int GetFreeInode();
	/* Returns the index of a free inode; returns -1 if no free inode is available. */
	
};
#endif
