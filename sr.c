#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

#define RTT  16.0       /* Round trip time (must be set to 16.0) */
#define WINDOWSIZE 6    /* Maximum number of unacked packets allowed in the window */
#define SEQSPACE 7      /* Sequence number space must be >= WINDOWSIZE + 1 */
#define NOTINUSE (-1)   /* Used for unused ack/seq fields */

/* Compute checksum used by both sender and receiver */
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

/********* Sender (A) ************/

static struct pkt buffer[WINDOWSIZE];
static bool acked[SEQSPACE];             /* Tracks ACK status per sequence number */
static int windowfirst, windowlast;
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

    windowlast = (windowlast + 1) % WINDOWSIZE; 
    buffer[windowlast] = sendpkt;
    acked[sendpkt.seqnum] = false;  /* Mark packet as unACKed */
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

    /* Begin: Selective Repeat ACK handling */
    if (!acked[packet.acknum]) {
      if (TRACE > 0)
        printf("----A: ACK %d is not a duplicate\n", packet.acknum);
      acked[packet.acknum] = true;
      new_ACKs++;

      stoptimer(A);
      while (windowcount > 0 && acked[buffer[windowfirst].seqnum]) {
        windowfirst = (windowfirst + 1) % WINDOWSIZE;
        windowcount--;
      }
      if (windowcount > 0)
        starttimer(A, RTT);
    } else {
      if (TRACE > 0)
        printf("----A: duplicate ACK received, do nothing!\n");
    }
    /* End: Selective Repeat ACK handling */

  } else {
    if (TRACE > 0)
      printf("----A: corrupted ACK is received, do nothing!\n");
  }
}

void A_timerinterrupt(void) {
  int i;

  if (TRACE > 0)
    printf("----A: time out,resend packets!\n");

  /* Begin: Selective Repeat resend logic */
  for (i = 0; i < windowcount; i++) {
    int index = (windowfirst + i) % WINDOWSIZE;
    if (!acked[buffer[index].seqnum]) {
      if (TRACE > 0)
        printf("---A: resending packet %d\n", buffer[index].seqnum);
      tolayer3(A, buffer[index]);
      packets_resent++;
    }
    if (i == 0)
      starttimer(A, RTT);
  }
  /* End: Selective Repeat resend logic */
}

void A_init(void) {
  int i;
  A_nextseqnum = 0;
  windowfirst = 0;
  windowlast = -1;
  windowcount = 0;
  for (i = 0; i < SEQSPACE; i++)  /* Initialize all ack flags */
    acked[i] = false;
}

/********* Receiver (B) ************/

static int expectedseqnum;
static int B_nextseqnum;

void B_input(struct pkt packet) {
  struct pkt sendpkt;  
  int i;

  if (!IsCorrupted(packet) && packet.seqnum == expectedseqnum) {
    if (TRACE > 0)
      printf("----B: packet %d is correctly received, send ACK!\n", packet.seqnum);
    packets_received++;

    tolayer5(B, packet.payload);
    sendpkt.acknum = expectedseqnum;
    expectedseqnum = (expectedseqnum + 1) % SEQSPACE;
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
  expectedseqnum = 0;
  B_nextseqnum = 1;
}

void B_output(struct msg message) {}
void B_timerinterrupt(void) {}
