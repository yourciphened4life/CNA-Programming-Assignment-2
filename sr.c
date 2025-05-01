#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

#define RTT  16.0
#define SEQSPACE 7
#define WINDOWSIZE 6
#define NOTINUSE (-1)

int ComputeChecksum(struct pkt packet) {
  int checksum = packet.seqnum + packet.acknum;
  int i;
  for (i = 0; i < 20; i++)
    checksum += (int)(packet.payload[i]);
  return checksum;
}

bool IsCorrupted(struct pkt packet) {
  return packet.checksum != ComputeChecksum(packet);
}

bool IsSeqNumInWindow(int base, int seqnum) {
  return ((seqnum - base + SEQSPACE) % SEQSPACE) < WINDOWSIZE;
}

static struct pkt buffer[SEQSPACE];
static bool acked[SEQSPACE];
static int windowfirst;
static int windowcount;
static int A_nextseqnum;

void A_output(struct msg message) {
  struct pkt sendpkt;
  int i;

  if (windowcount < WINDOWSIZE) {
    if (TRACE > 1)
      printf("----A: New message arrives, send window is not full, send new messge to layer3!\n");

    sendpkt.seqnum = A_nextseqnum;
    sendpkt.acknum = NOTINUSE;
    for (i = 0; i < 20; i++)
      sendpkt.payload[i] = message.data[i];
    sendpkt.checksum = ComputeChecksum(sendpkt);

    buffer[A_nextseqnum] = sendpkt;
    acked[A_nextseqnum] = false;
    windowcount++;

    if (TRACE > 0)
      printf("Sending packet %d to layer 3\n", sendpkt.seqnum);
    tolayer3(A, sendpkt);

    if (windowcount == 1)
      starttimer(A, RTT);

    A_nextseqnum = (A_nextseqnum + 1) % SEQSPACE;
  } else {
    if (TRACE > 0)
      printf("----A: New message arrives, send window is full\n");
    window_full++;
  }
}

void A_input(struct pkt packet) {
  if (!IsCorrupted(packet)) {
    if (TRACE > 0)
      printf("----A: uncorrupted ACK %d is received\n", packet.acknum);
    total_ACKs_received++;

    if (IsSeqNumInWindow(windowfirst, packet.acknum)) {
      if (!acked[packet.acknum]) {
        if (TRACE > 0)
          printf("----A: ACK %d is not a duplicate\n", packet.acknum);
        acked[packet.acknum] = true;
        new_ACKs++;

        stoptimer(A);
        while (windowcount > 0 && acked[windowfirst]) {
          windowfirst = (windowfirst + 1) % SEQSPACE;
          windowcount--;
        }
        if (windowcount > 0)
          starttimer(A, RTT);
      } else {
        if (TRACE > 0)
          printf("----A: duplicate ACK received, do nothing!\n");
      }
    } else {
      if (TRACE > 0)
        printf("----A: received ACK %d, but not in window (ignored)\n", packet.acknum);
    }
  } else {
    if (TRACE > 0)
      printf("----A: corrupted ACK is received, do nothing!\n");
  }
}

void A_timerinterrupt(void) {
  int i;
  if (TRACE > 0)
    printf("----A: time out,resend packets!\n");

  for (i = 0; i < SEQSPACE; i++) {
    if (IsSeqNumInWindow(windowfirst, i) && !acked[i]) {
      if (TRACE > 0)
        printf("---A: resending packet %d\n", buffer[i].seqnum);
      tolayer3(A, buffer[i]);
      packets_resent++;
    }
  }

  if (windowcount > 0)
    starttimer(A, RTT);
}

void A_init(void) {
  int i;
  A_nextseqnum = 0;
  windowfirst = 0;
  windowcount = 0;
  for (i = 0; i < SEQSPACE; i++)
    acked[i] = false;
}

static int expectedseqnum;
static int B_nextseqnum;
static struct pkt B_buffer[SEQSPACE];
static bool B_received[SEQSPACE];

void B_input(struct pkt packet) {
  struct pkt sendpkt;
  int i, index = packet.seqnum;

  if (!IsCorrupted(packet) && IsSeqNumInWindow(expectedseqnum, index)) {
    if (!B_received[index]) {
      B_received[index] = true;
      B_buffer[index] = packet;
      packets_received++;

      if (TRACE > 0)
        printf("----B: packet %d is correctly received, send ACK!\n", index);

      while (B_received[expectedseqnum]) {
        tolayer5(B, B_buffer[expectedseqnum].payload);
        B_received[expectedseqnum] = false;
        expectedseqnum = (expectedseqnum + 1) % SEQSPACE;
      }
    } else {
      if (TRACE > 0)
        printf("----B: duplicate packet received, send ACK!\n");
    }

    sendpkt.acknum = index;
  } else {
    if (TRACE > 0)
      printf("----B: packet corrupted or not expected sequence number, resend ACK!\n");

    sendpkt.acknum = (expectedseqnum == 0) ? SEQSPACE - 1 : expectedseqnum - 1;
  }

  sendpkt.seqnum = B_nextseqnum;
  B_nextseqnum = (B_nextseqnum + 1) % 2;
  for (i = 0; i < 20; i++)
    sendpkt.payload[i] = '0';
  sendpkt.checksum = ComputeChecksum(sendpkt);
  tolayer3(B, sendpkt);
}

void B_init(void) {
  int i;
  expectedseqnum = 0;
  B_nextseqnum = 1;
  for (i = 0; i < SEQSPACE; i++)
    B_received[i] = false;
}

void B_output(struct msg message) {}
void B_timerinterrupt(void) {}
