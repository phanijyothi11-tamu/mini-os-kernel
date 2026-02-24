/*
     File        : file.C

     Author      : Riccardo Bettati
     Modified    : 2021/11/28

     Description : Implementation of simple File class, with support for
                   sequential read/write operations.
*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "console.H"
#include "file.H"

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR/DESTRUCTOR */
/*--------------------------------------------------------------------------*/

File::File(FileSystem *_fs, int _id) {
    Console::puts("Opening file.\n");

    fs = _fs;                         // Store FileSystem pointer
    inode = fs->LookupFile(_id);     // Lookup the inode
    assert(inode != nullptr);        // File must exist

    current_position = 0;
}

File::~File() {
    Console::puts("Closing file.\n");

    // Save updated inode metadata (e.g., file length)
    fs->write_inode_block_to_disk();
}

/*--------------------------------------------------------------------------*/
/* FILE FUNCTIONS */
/*--------------------------------------------------------------------------*/

int File::Read(unsigned int _n, char *_buf) {
    Console::puts("reading from file\n");

    int bytes_read = 0;
    while (bytes_read < (int)_n && !EoF()) {
        int logical_block = current_position / SimpleDisk::BLOCK_SIZE;
        int offset = current_position % SimpleDisk::BLOCK_SIZE;

        int block_num = GetBlockNumber(logical_block, false);
        if (block_num == -1) break;

        unsigned char temp_block[SimpleDisk::BLOCK_SIZE];
        fs->read_block_from_disk(block_num, temp_block);

        _buf[bytes_read] = temp_block[offset];
        current_position++;
        bytes_read++;
    }
    Console::puts("Finished reading file.\n");
    return bytes_read;
}

int File::Write(unsigned int _n, const char *_buf) {
    Console::puts("writing to file\n");

    int bytes_written = 0;
    while (bytes_written < (int)_n && current_position < 128 * SimpleDisk::BLOCK_SIZE) {
        int logical_block = current_position / SimpleDisk::BLOCK_SIZE;
        int offset = current_position % SimpleDisk::BLOCK_SIZE;

        int block_num = GetBlockNumber(logical_block, true);
        if (block_num == -1) break;

        unsigned char temp_block[SimpleDisk::BLOCK_SIZE];
        fs->read_block_from_disk(block_num, temp_block);

        temp_block[offset] = _buf[bytes_written];
        fs->write_block_to_disk(block_num, temp_block);

        current_position++;
        bytes_written++;
    }

    if (current_position > (int)inode->length) {
        inode->length = current_position;
    }
    Console::puts("Finished writing to file.\n");
    return bytes_written;
}

void File::Reset() {
    Console::puts("resetting file\n");
    current_position = 0;
}

bool File::EoF() {
    //Console::puts("checking for EoF\n");
    return current_position >= (int)inode->length;
}

int File::GetBlockNumber(int logical_block_index, bool allocate_if_missing) {
    // Load indirect block
    unsigned char indirect_data[SimpleDisk::BLOCK_SIZE];
    fs->read_block_from_disk(inode->indirect_block, indirect_data);
    int* table = (int*)indirect_data;

    int block_num = table[logical_block_index];

    // If block not allocated and allowed to allocate
    if (block_num == -1 && allocate_if_missing) {
        for (int i = FileSystem::DATA_BLOCK_START; i < fs->get_disk_size(); ++i) {
            if (fs->is_block_free(i)) {
                fs->mark_block_used(i);
                table[logical_block_index] = i;

                // Save updated indirect block
                fs->write_block_to_disk(inode->indirect_block, (unsigned char*)table);
                return i;
            }
        }
        return -1; // Disk full
    }

    return block_num;
}

