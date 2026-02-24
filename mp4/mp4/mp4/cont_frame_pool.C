/*
 File: ContFramePool.C
 
 Author:
 Date  : 
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

ContFramePool::FrameState ContFramePool::get_state(unsigned long _frame_no) {
    unsigned long byte_index = (_frame_no * 2) / 8;  // Find byte
    unsigned long bit_position = (_frame_no * 2) % 8;  // Find bit position inside byte

    unsigned char state = (bitmap[byte_index] >> bit_position) & 0b11;  // Extract 2 bits

    return static_cast<FrameState>(state);
}

void ContFramePool::set_state(unsigned long _frame_no, FrameState _state) {
    unsigned long byte_index = (_frame_no * 2) / 8;  // Find which byte stores the frame
    unsigned long bit_position = (_frame_no * 2) % 8;  // Find bit position inside the byte

    // Clear the current state (set to 00)
    bitmap[byte_index] &= ~(0b11 << bit_position);

    // Set the new state
    bitmap[byte_index] |= ((unsigned char)_state << bit_position);
}

ContFramePool* ContFramePool::pool = nullptr;

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no)
{
    // TODO: IMPLEMENTATION NEEEDED!
    pool = this;

    base_frame_no = _base_frame_no;
    nframes = _n_frames;
    info_frame_no = _info_frame_no;
    nFreeFrames = _n_frames;

    // Ensure bitmap fits in a single frame
    // assert(nframes <= (FRAME_SIZE * 8) / 2);

    unsigned long total_bytes_needed = (nframes * 2) / 8;

    if ((nframes * 2) % 8 != 0) {
        total_bytes_needed++;
    }

    if (info_frame_no == 0) {
        // Store the bitmap inside the first frame in the pool
        bitmap = (unsigned char *)(base_frame_no * FRAME_SIZE);
        info_frame_no = base_frame_no;
    } else {
        // Store the bitmap at the specified frame
        bitmap = (unsigned char *)(info_frame_no * FRAME_SIZE);
    }

    for (unsigned long i = 0; i < nframes; i++) {
        set_state(i, FrameState::Free);
    }

    // Mark the frame storing the bitmap as HEAD-OF-SEQUENCE (0b01)
    set_state(info_frame_no - base_frame_no, FrameState::HeadOfSequence);
    nFreeFrames--;  // Since one frame is occupied for metadata

    Console::puts("ContframePool::Constructor initialized successfully.\n");
}

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    if (!_n_frames)
        return 0;
    // Search for a contiguous sequence of `_n_frames` FREE frames
    unsigned long start_frame = 0;
    bool found = false;

    for (unsigned long i = 0; i <= (nframes - _n_frames); i++) {
        found = true;
        for (unsigned long j = 0; j < _n_frames; j++) {
            if (get_state(i + j) != FrameState::Free) {
                found = false;
                break;
            }
        }

        if (found) {
            start_frame = i;
            break;
        }
    }

    // If no contiguous block found, return failure (0)
    if (!found) {
        return 0;
    }

    // Mark the first frame as `HEAD-OF-SEQUENCE`
    set_state(start_frame, FrameState::HeadOfSequence);

    // Mark the remaining frames as `ALLOCATED`
    for (unsigned long j = 1; j < _n_frames; j++) {
        set_state(start_frame + j, FrameState::Allocated);
    }

    // Reduce the available free frames count
    nFreeFrames -= _n_frames;

    // Return the starting frame number (relative to base)
    return start_frame + base_frame_no;
}

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    // Convert base frame number to relative index
    unsigned long start_index = _base_frame_no - base_frame_no;

    // Mark the first frame as `HEAD-OF-SEQUENCE`
    set_state(start_index, FrameState::HeadOfSequence);

    // Mark the remaining frames as `ALLOCATED`
    for (unsigned long i = 1; i < _n_frames; i++) {
        set_state(start_index + i, FrameState::Allocated);
    }

    // Reduce the free frame count
    nFreeFrames -= _n_frames;
}

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    // Ensure the pool is initialized
    assert(pool != nullptr);

    // Convert frame number to relative index
    unsigned long frame_index = _first_frame_no - pool->base_frame_no;

    // Ensure the first frame is actually `HEAD-OF-SEQUENCE`
    assert(pool->get_state(frame_index) == FrameState::HeadOfSequence);

    // Mark the first frame as `FREE`
    pool->set_state(frame_index, FrameState::Free);
    pool->nFreeFrames++;

    // Free the remaining frames in the sequence
    for (unsigned long i = frame_index + 1; i < pool->nframes; i++) {
        FrameState state = pool->get_state(i);

        if (state == FrameState::Free || state == FrameState::HeadOfSequence) {
            break;  // Stop at another `HEAD-OF-SEQUENCE` or already free frame
        }

        pool->set_state(i, FrameState::Free);
        pool->nFreeFrames++;
    }
}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    // Calculate the number of bytes required
    unsigned long total_bytes_needed = (_n_frames * 2) / 8;

    // Round up if there is any remainder
    if ((_n_frames * 2) % 8 != 0) {
        total_bytes_needed++;
    }

    // Calculate how many frames are required to store these bytes
    unsigned long info_frames = total_bytes_needed / FRAME_SIZE;
    if (total_bytes_needed % FRAME_SIZE != 0) {
        info_frames++;  // Round up to the next frame
    }

    return info_frames;
}
