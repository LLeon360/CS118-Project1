# Description of Work (10 points)

## Design Choices
We started off with implementing the three way handshake. Since this is done just once, we manually crafted the packets that both the client and sender would send to each other. 
When that worked, since the logic for both client and sender would be pretty much the same, we defined a new function to handle the rest of the behavior called `normal_loop`. 
This contained an infinite loop that on each iteration would 1. consume as much input as the flow window would allow and send that 2. send any ACKs as needed and 3. receive up to one packet from the network and handle it accordingly. 
That last step would either immediately output it and flush as much data from the out-of-order buffer as possible, store it in the out-of-order buffer, or drop it. 
We would also read that packet's ACK number (if it had one) and handle that accordingly, removing packets from our sender window or counting dupicate ACKs for retransmission. 

## Problems We Encountered
- Fun fact, the `==` operator takes precedence over `&`. We had to add some parentheses to fix some logic because of this.
- We missed some `ntohs()` calls in various places.
- We didn't realize that dedicated ACKs didn't increase the SEQ number for a time. 
- We initially made a buffer for ACK numbers to send out, before realizing that this wasn't necessary since ACKs were cumulative. 
- We used to do a bunch of fancy packet validation before realizing that 1. at most one bit would be flipped at a time so parity checking was all that was needed and 2. our validation was wrong. 
- It turns out not respecting the flow window size makes you fail the non-small test cases. 
- One diabolical error I got was when the Makefile stopped working for no apparent reason. I still don't know why it broke. 

## Solutions To Our Problems
> "Git gud!" 
> 
> \- Hornet

Reading the spec carefully, reading our own code carefully, adding debug print statements, reading those debug print statements, that covered most of our problems. 
That last problem was fixed by a good old `./helper clean`. 
