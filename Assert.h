
#ifndef ASSERT_H
#define ASSERT_H

/**
 * This function should be defined on the arduino main and it
 * should reset the board
*/
void reset_ino(); 

void assert(bool val, bool rst = true);

#endif //ASSERT_H