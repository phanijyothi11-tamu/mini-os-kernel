/*
 File: ContFramePool.C
 
 Author:
 Date  : 
*/

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool (in simple_frame_pool.H/C) allocates single frames
 and does not guarantee contiguous sequences. ContFramePool extends this to
 allocate sequences of contiguous frames.

 ContFramePool uses a bitmap with 3 states per frame:
   - Free        : Frame is available.
   - Used        : Frame is allocated (not first in sequence).
   - HeadOfSequence (HoS): First frame of a contiguous block.

 This allows efficient allocation and release of contiguous memory.
*/
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/
#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* STATIC MEMBER INITIALIZATION */
/*--------------------------------------------------------------------------*/
ContFramePool* ContFramePool::pool_list_head = nullptr;

/*--------------------------------------------------------------------------*/
/* BITMAP HELPERS (2 bits per frame) */
/*--------------------------------------------------------------------------*/
ContFramePool::FrameState ContFramePool::get_state(unsigned long _frame_no) {
    unsigned long bit_index = _frame_no * 2;
    unsigned long byte_index = bit_index / 8;
    unsigned long offset = bit_index % 8;

    unsigned char state = (bitmap[byte_index] >> offset) & 0b11u;
    return static_cast<FrameState>(state);
}

void ContFramePool::set_state(unsigned long _frame_no, FrameState _state) {
    unsigned long bit_index = _frame_no * 2;
    unsigned long byte_index = bit_index / 8;
    unsigned long offset = bit_index % 8;

    // clear 2 bits at position
    bitmap[byte_index] &= static_cast<unsigned char>(~(0b11u << offset));
    // set new state
    bitmap[byte_index] |= static_cast<unsigned char>((static_cast<unsigned char>(_state) & 0b11u) << offset);
}

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/
ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no)
{
    base_frame_no = _base_frame_no;
    nframes = _n_frames;
    info_frame_no = _info_frame_no;

    // Calculate how many frames needed for bitmap
    unsigned long needed = ContFramePool::needed_info_frames(nframes);

    if (info_frame_no == 0) {
        info_frame_no = base_frame_no;
    }

    assert((info_frame_no >= base_frame_no) &&
           ((info_frame_no + needed) <= (base_frame_no + nframes)));

    // Bitmap location (physical-to-pointer assumption as in starter code)
    bitmap = (unsigned char*)(info_frame_no * FRAME_SIZE);

    // Initialize all frames as Free
    for (unsigned long i = 0; i < nframes; i++) {
        set_state(i, FrameState::Free);
    }

    // Reserve all info frames (mark as Used so they won't be allocated)
    unsigned long rel = info_frame_no - base_frame_no;
    for (unsigned long i = 0; i < needed; i++) {
        set_state(rel + i, FrameState::Used); // <-- use 'Used' per header
    }

    // Update free count
    nFreeFrames = (nframes >= needed) ? (nframes - needed) : 0;

    // Link into global pool list
    this->next_pool = pool_list_head;
    pool_list_head = this;

    Console::puts("ContFramePool initialized.\n");
}

/*--------------------------------------------------------------------------*/
/* ALLOCATE CONTIGUOUS FRAMES */
/*--------------------------------------------------------------------------*/
unsigned long ContFramePool::get_frames(unsigned int _n_frames) {
    if (_n_frames == 0) return 0;
    if ((unsigned long)_n_frames > nFreeFrames) return 0;

    if ((unsigned long)_n_frames > nframes) return 0;
    unsigned long limit = nframes - _n_frames + 1;

    for (unsigned long i = 0; i < limit; i++) {
        if (get_state(i) != FrameState::Free) continue;

        bool ok = true;
        for (unsigned long j = 0; j < _n_frames; j++) {
            if (get_state(i + j) != FrameState::Free) {
                ok = false;
                break;
            }
        }
        if (!ok) continue;

        // Mark first frame as HoS (Head of Sequence)
        set_state(i, FrameState::HoS); // <-- use 'HoS' per header
        // Mark remaining frames as Used
        for (unsigned long j = 1; j < _n_frames; j++) {
            set_state(i + j, FrameState::Used);
        }

        nFreeFrames -= _n_frames;
        return base_frame_no + i;
    }

    return 0; // not found
}

/*--------------------------------------------------------------------------*/
/* MARK FRAMES INACCESSIBLE */
/*--------------------------------------------------------------------------*/
void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames) {
    unsigned long start = _base_frame_no - base_frame_no;
    assert(start < nframes);
    assert((start + _n_frames) <= nframes);

    // Mark first as HoS, rest as Used
    set_state(start, FrameState::HoS);
    for (unsigned long i = 1; i < _n_frames; i++) {
        set_state(start + i, FrameState::Used);
    }

    if (_n_frames <= nFreeFrames) nFreeFrames -= _n_frames;
    else nFreeFrames = 0;
}

/*--------------------------------------------------------------------------*/
/* RELEASE FRAMES (STATIC, search pool) */
/*--------------------------------------------------------------------------*/
void ContFramePool::release_frames(unsigned long _first_frame_no) {
    ContFramePool* p = pool_list_head;
    while (p != nullptr) {
        if ((_first_frame_no >= p->base_frame_no) &&
            (_first_frame_no < (p->base_frame_no + p->nframes))) {

            unsigned long idx = _first_frame_no - p->base_frame_no;
            // Ensure it's a head of sequence
            assert(p->get_state(idx) == FrameState::HoS);

            // Free the head
            p->set_state(idx, FrameState::Free);
            p->nFreeFrames++;

            // Free following Used frames until Free or HoS
            for (unsigned long i = idx + 1; i < p->nframes; i++) {
                FrameState s = p->get_state(i);
                if (s == FrameState::Free || s == FrameState::HoS) break;
                p->set_state(i, FrameState::Free);
                p->nFreeFrames++;
            }
            return;
        }
        p = p->next_pool;
    }

    Console::puts("ContFramePool::release_frames: invalid frame\n");
    assert(false);
}

/*--------------------------------------------------------------------------*/
/* CALCULATE INFO FRAMES NEEDED */
/*--------------------------------------------------------------------------*/
unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames) {
    if (_n_frames == 0) return 0;

    unsigned long total_bits = _n_frames * 2UL;
    unsigned long total_bytes = total_bits / 8UL;
    if (total_bits % 8UL) total_bytes++;

    unsigned long frames = total_bytes / FRAME_SIZE;
    if (total_bytes % FRAME_SIZE) frames++;

    return frames;
}