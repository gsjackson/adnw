#ifndef MATRIX_H
#define  MATRIX_H 1

#include <avr/io.h>


#define ALL_COLS_MASK ((1<<COLS)-1)  // 0x63 or all lower 6 bits

uint8_t read_col(void);
void unselect_rows(void);
void activate(uint8_t row);


// this must be called once before matrix_scan.
uint8_t read_col(void)
{
    // teensy 2.0: F 01  4567
    uint8_t res = PINF;

    return (
    (res & (1<<0)) << 5 |
    (res & (1<<1)) << 3 |
    (res & (1<<4)) >> 1 |
    (res & (1<<5)) >> 3 |
    (res & (1<<6)) >> 5 |
    (res & (1<<7)) >> 7  );


    return (( res & 0b11) | ((res & 0b11110000)>>2 ));

    // PINF & 0x11
    return (PINF & ~((1<<6) | (1<<7)));
}

void unselect_rows(void) {
    DDRD  = 0x00;
    PORTD = 0x00;
}

void activate(uint8_t row)
{
    unselect_rows();
    // swap upper and lower ports to have left half first in matrix
    (row<4) ? (row+=4) : (row-=4);
    DDRD = (1<<row);
    return;
}
static inline void init_cols(void)
{

    /* Columns are inputs */
    DDRF  &= ((1<<2) | (1<<3)); // 192 or upper 2 bits

    /* Enable pull-up resistors on inputs */
    //PORTF |= ((1<<6)-1);        // 63 or lower 6 bits
    PORTF |= (0b11110011);

}
#endif
