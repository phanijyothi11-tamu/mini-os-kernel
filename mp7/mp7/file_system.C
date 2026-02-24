/*
     File        : file_system.C

     Author      : Riccardo Bettati
     Modified    : 2021/11/28

     Description : Implementation of simple File System class.
                   Has support for numerical file identifiers.
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "console.H"
#include "file_system.H"

/*--------------------------------------------------------------------------*/
/* CLASS Inode */
/*--------------------------------------------------------------------------*/

/* You may need to add a few functions, for example to help read and store 
   inodes from and to disk. */

// Save this inode to disk at the appropriate index in the inode list block
void Inode::SaveToDisk(int index) {
    unsigned char buffer[SimpleDisk::BLOCK_SIZE];
    fs->disk->read(FileSystem::INODE_BLOCK, buffer);
    ((Inode*)buffer)[index] = *this;
    fs->disk->write(FileSystem::INODE_BLOCK, buffer);
}

// Load this inode from disk into memory from index in the inode list block
void Inode::LoadFromDisk(int index) {
    unsigned char buffer[SimpleDisk::BLOCK_SIZE];
    fs->disk->read(FileSystem::INODE_BLOCK, buffer);
    *this = ((Inode*)buffer)[index];
}

/*--------------------------------------------------------------------------*/
/* CLASS FileSystem */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

FileSystem::FileSystem() {
    Console::puts("In file system constructor.\n");
    inodes = nullptr;
    free_blocks = nullptr;
    disk = nullptr;
    size = 0;  // this will be set in Format or Mount
}

FileSystem::~FileSystem() {
    Console::puts("unmounting file system\n");
    /* Make sure that the inode list and the free list are saved. */
    write_inode_block_to_disk();
    disk->write(FREELIST_BLOCK, free_blocks);

    delete[] inodes;
    delete[] free_blocks;
}


/*--------------------------------------------------------------------------*/
/* FILE SYSTEM FUNCTIONS */
/*--------------------------------------------------------------------------*/


bool FileSystem::Mount(SimpleDisk * _disk) {
    Console::puts("Mounting file system from disk...\n");
    /* Here you read the inode list and the free list into memory */

    // Save pointer to the disk
    disk = _disk;
    this->size = disk->NaiveSize() / SimpleDisk::BLOCK_SIZE;

    // Allocate memory for inodes
    inodes = new Inode[MAX_INODES];
    
    // Load inode block (block 0) from disk
    unsigned char inode_block[SimpleDisk::BLOCK_SIZE];
    disk->read(INODE_BLOCK, inode_block);

    for (int i = 0; i < MAX_INODES; i++) {
        inodes[i] = ((Inode*)inode_block)[i];
        inodes[i].fs = this;  // set back pointer to FileSystem
    }

    // Allocate memory for free block list
    free_blocks = new unsigned char[SimpleDisk::BLOCK_SIZE];
    disk->read(FREELIST_BLOCK, free_blocks);

    Console::puts("Mount successful.\n");
    return true;
}

bool FileSystem::Format(SimpleDisk * _disk, unsigned int _size) { // static!
    Console::puts("Formatting disk...\n");
    /* Here you populate the disk with an initialized (probably empty) inode list
       and a free list. Make sure that blocks used for the inodes and for the free list
       are marked as used, otherwise they may get overwritten. */
    
    // Step 1: Calculate how many blocks we need
    unsigned int total_blocks = _size / SimpleDisk::BLOCK_SIZE;

    if (total_blocks < 10) {
        Console::puts("Disk too small to format!\n");
        return false;
    }

    // Step 2: Zero out all blocks on disk
    unsigned char zero_block[SimpleDisk::BLOCK_SIZE] = {0};
    for (unsigned int i = 0; i < total_blocks; i++) {
        _disk->write(i, zero_block);
    }

    // Step 3: Write empty inode list to INODES block (block 0)
    Inode empty_inodes[MAX_INODES];
    for (int i = 0; i < MAX_INODES; i++) {
        empty_inodes[i] = Inode(); // default constructor sets valid = false
    }

    _disk->write(INODE_BLOCK, (unsigned char*)empty_inodes);

    // Step 4: Initialize free block list
    unsigned char free_list[SimpleDisk::BLOCK_SIZE] = {0};

    for (unsigned int i = 0; i < total_blocks; i++) {
        free_list[i] = 0; // 0 = free, 1 = used
    }

    // Mark INODE_BLOCK and FREELIST_BLOCK as used
    free_list[INODE_BLOCK] = 1;
    free_list[FREELIST_BLOCK] = 1;

    _disk->write(FREELIST_BLOCK, free_list);

    Console::puts("Disk formatted successfully.\n");
    return true;
}

Inode * FileSystem::LookupFile(int _file_id) {
    Console::puts("looking up file with id = ");
    Console::puti(_file_id);
    Console::puts("\n");
    /* Here you go through the inode list to find the file. */

    for (int i = 0; i < MAX_INODES; ++i) {
        if (inodes[i].valid && inodes[i].id == _file_id) {
            return &inodes[i];
        }
    }
    return nullptr; // Not found
}

bool FileSystem::CreateFile(int _file_id) {
    Console::puts("creating file with id:");
    Console::puti(_file_id);
    Console::puts("\n");

    // 1. Check if file already exists
    if (LookupFile(_file_id) != nullptr) {
        Console::puts("File already exists!\n");
        return false;
    }
    // 2. Find a free inode
    int inode_index = GetFreeInode();
    if (inode_index == -1) {
        Console::puts("No free inodes!\n");
        return false;
    }
    // 3. Find a free block for the indirect block
    int indirect_block_index = GetFreeBlock();
    if (indirect_block_index == -1) {
        Console::puts("No free blocks for indirect block!\n");
        return false;
    }
    // 4. Mark indirect block as used
    free_blocks[indirect_block_index] = 1;
    disk->write(FREELIST_BLOCK, free_blocks);
    // 5. Initialize the indirect block with 128 pointers set to -1
    unsigned char indirect_block_data[SimpleDisk::BLOCK_SIZE];
    for (int j = 0; j < 128; ++j) {
        ((int*)indirect_block_data)[j] = -1;
    }
    disk->write(indirect_block_index, indirect_block_data);
    // 6. Fill in inode
    Inode& inode = inodes[inode_index];
    inode.id = _file_id;
    inode.length = 0;
    inode.indirect_block = indirect_block_index;
    inode.valid = true;
    inode.fs = this;
    // 7. Save inode to disk
    inode.SaveToDisk(inode_index);

    Console::puts("File created successfully.\n");
    return true;
}


bool FileSystem::DeleteFile(int _file_id) {
    Console::puts("deleting file with id:");
    Console::puti(_file_id);
    Console::puts("\n");
    /* First, check if the file exists. If not, throw an error. 
       Then free all blocks that belong to the file and delete/invalidate 
       (depending on your implementation of the inode list) the inode. */
    
    // 1. Find the inode
    for (int i = 0; i < MAX_INODES; ++i) {
        if (inodes[i].valid && inodes[i].id == _file_id) {
            // 2. Free all blocks used by the file
            // 2a. Read indirect block
            unsigned char indirect_data[SimpleDisk::BLOCK_SIZE];
            disk->read(inodes[i].indirect_block, indirect_data);
            // 2b. Free each allocated data block
            for (int j = 0; j < 128; ++j) {
                int data_block = ((int*)indirect_data)[j];
                if (data_block >= DATA_BLOCK_START && data_block < size) {
                    free_blocks[data_block] = 0;
                }
            }
            // 2c. Free the indirect block itself
            int indirect_block_num = inodes[i].indirect_block;
            if (indirect_block_num >= DATA_BLOCK_START && indirect_block_num < size) {
                free_blocks[indirect_block_num] = 0;
            }
            // 2d. Save updated freelist
            disk->write(FREELIST_BLOCK, free_blocks);

            // 3. Clear the inode
            inodes[i] = Inode(); // reset to default
            inodes[i].fs = this; // restore fs pointer
            inodes[i].SaveToDisk(i);

            Console::puts("File deleted successfully.\n");
            return true;
        }
    }
    Console::puts("File not found!\n");
    return false;
}

void FileSystem::read_block_from_disk(int block_no, unsigned char* buffer) {
    disk->read(block_no, buffer);
}

void FileSystem::write_block_to_disk(int block_no, unsigned char* buffer) {
    disk->write(block_no, buffer);
}

void FileSystem::write_inode_block_to_disk() {
    unsigned char inode_block[SimpleDisk::BLOCK_SIZE];
    for (int i = 0; i < MAX_INODES; ++i) {
        ((Inode*)inode_block)[i] = inodes[i];
    }
    disk->write(INODE_BLOCK, inode_block);
}

int FileSystem::get_disk_size() const {
    return size;
}

bool FileSystem::is_block_free(int block_no) const {
    return free_blocks[block_no] == 0;
}

void FileSystem::mark_block_used(int block_no) {
    free_blocks[block_no] = 1;
    disk->write(FREELIST_BLOCK, free_blocks);
}

void FileSystem::mark_block_free(int block_no) {
    free_blocks[block_no] = 0;
    disk->write(FREELIST_BLOCK, free_blocks);
}

int FileSystem::GetFreeBlock() {
    for (int i = DATA_BLOCK_START; i < (int)size; ++i) {
        if (free_blocks[i] == 0) {
            return i;
        }
    }
    return -1;
}

int FileSystem::GetFreeInode() {
    for (int i = 0; i < (int)MAX_INODES; ++i) {
        if (!inodes[i].valid) {
            return i;
        }
    }
    return -1;
}