# CNA-Programming-Assignment-2

Reliable Transport with Selective Repeat Programming


# STEP - 1 

- Created `sr.c` and `sr.h` by copying the base `gbn.c` and `gbn.h` files.
- Replaced cumulative ACK handling with **individual ACK tracking** using a `bool acked[SEQSPACE]` array.
- Updated `A_input()` to:
  * Check for corrupted ACKs
  * Mark specific packets as acknowledged
  * Slide the sender window only when the **front of the buffer** is acknowledged
- Modified `A_timerinterrupt()` to:
  * Resend only those packets that are not yet acknowledged (SR behavior)
- Replaced all C++ style comments (`//`) with C-style comments (`/* */`) for ANSI compatibility.
- Moved all `for (int i...)` loop variables to C90-compatible format.
- Verified correctness through test cases:
  * Sent 5 packets with 0% loss and corruption.
  * All ACKs received correctly and window advanced as expected.
  * No resends occurred.
  * All 5 messages delivered successfully to the application layer.


# STEP - 2 

- Updated `WINDOWSIZE` to SEQSPACE / 2 for Selective Repeat compliance.
- Added `IsSeqNumInWindow()` utility function to handle window wraparound logic.
- Ensured `A_output()` correctly halts transmission when the send window is full.
- Updated buffer and ACK tracking to index by SEQSPACE for wraparound support.
- Resend logic in `A_timerinterrupt()` now checks window boundaries properly.
- Verified correctness with test: sent 10 messages, 3 accepted (window = 3), 7 dropped due to full window.
- All ACKs handled and window slid forward as expected.
- Verified correctness through test cases:
  * Messages correctly blocked when window full
  * Window advanced after ACKs
  * No crashes or logic errors with wraparound
