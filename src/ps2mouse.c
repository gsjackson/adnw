/*********************************************************************************
 * Title: PS/2 Library
 * Author: Akshay Srinivasan<akshaysrinivasan@nitk.ac.in>
 * Date: 22 June 2009
 * Software: AVR-GCC 4.3.2

 *-------------------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <akshaysrinivasan@gmail.com> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.        Akshay Srinivasan
 *-------------------------------------------------------------------------------------

 DESCRIPTION
    This library provides an abstraction layer for the PS/2 protocol. This library
    provides functions for receiving and sending bytes via PS/2. This library can
    be used to read and write data in PS/2 devices, although interpreting the data
    is left for you to do.

    For more information goto,
    http://computer-engineering.org/ps2protocol/
**********************************************************************************/

#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>

#include "Keyboard.h"
#include "ps2mouse.h"

#define ACK 0
#define DELAY 150

#define ACC_RAMPTIME 400 // acc incrementation time till maximum
#define ACC_MAX      2.5 // maximum accelleration factor reachable

volatile uint8_t     scrollcnt;
volatile uint32_t    mouse_timer; /// toggle mouse mode for a specified time
volatile uint16_t    accel; /// toggle mouse mode for a specified time

void data(uint8_t x)
{
    DDDR |= (1 << DBIT);
    if(x==0)
        DPORT &= ~(1 << DBIT);
    else if(x==1)
        DPORT |= (1 << DBIT);
    return;
}

void clk(uint8_t x)
{
    CDDR |= (1 << CBIT);
    if(x==0)
        CPORT &= ~(1 << CBIT);
    else if(x==1)
        CPORT |= (1 << CBIT);
    return;
}

void tp_reset()
{
    RDDR |= (1 << RBIT);
    RPORT |= (1 << RBIT);
    _delay_us(DELAY);
    RPORT &= ~(1 << RBIT);
    return;

    while(CLK) {
        __asm__("nop");
    }
    while(!CLK) {
        __asm__("nop");
    }
    while(CLK) {
        __asm__("nop");
    }
    while(!CLK) {
        __asm__("nop");
    }
    RPORT &= ~(1 << RBIT);
}


#define CNT 500
void serout(uint8_t bit)
{
    uint8_t cnt;
    cnt=0;
    while(CLK && cnt++ < CNT) {
        __asm__("nop");
    }
    data(bit);
    cnt = 0;
    while(!CLK && cnt++ < CNT) {
        __asm__("nop");
    }
}

uint8_t serin()
{
    uint8_t state;
    uint8_t cnt;
    cnt=0;
    while(CLK && cnt++ < CNT) {
        __asm__("nop");
    }
    state = DATA;
    cnt = 0;
    while(!CLK && cnt++ < CNT) {
        __asm__("nop");
    }
    return state;
}

uint8_t oparity(uint8_t byte)
{
    uint8_t par=1;
    par ^= ((byte & (1 << 0)) >> 0);
    par ^= ((byte & (1 << 1)) >> 1);
    par ^= ((byte & (1 << 2)) >> 2);
    par ^= ((byte & (1 << 3)) >> 3);
    par ^= ((byte & (1 << 4)) >> 4);
    par ^= ((byte & (1 << 5)) >> 5);
    par ^= ((byte & (1 << 6)) >> 6);
    par ^= ((byte & (1 << 7)) >> 7);
    return par;
}



bool send_packet(uint8_t byte)
{
    uint8_t parity;

    errcnt=0;
    do {
        parity = oparity(byte);
        clk(0);
        _delay_us(DELAY);
        data(0); //Start
        clk(1);
        CDDR &= ~(1 << CBIT); // Release clock
        CPORT |= (1 << CBIT); //Set the pull up on Clock

        /////////////
        serout((byte & (1 << 0)) >> 0);
        serout((byte & (1 << 1)) >> 1);
        serout((byte & (1 << 2)) >> 2);
        serout((byte & (1 << 3)) >> 3);
        serout((byte & (1 << 4)) >> 4);
        serout((byte & (1 << 5)) >> 5);
        serout((byte & (1 << 6)) >> 6);
        serout((byte & (1 << 7)) >> 7);
        serout(parity);

        serout(1); //Stop

        DDDR &= ~(1 << DBIT); //Release the Data line
        DPORT |= (1 << DBIT); //Set the pull up on Data

        //if(serin() != ACK )
        //    send_packet(byte); // Try again if ACK has not been received
        errcnt++;
    } while (serin() != ACK && errcnt < 5 );

    if(errcnt >= 5)
        return false;
    return true;
}

uint8_t read_packet(void)
{
    uint8_t byte=0,par;
    serin(); //Start
    byte |= (serin() << 0);
    byte |= (serin() << 1);
    byte |= (serin() << 2);
    byte |= (serin() << 3);
    byte |= (serin() << 4);
    byte |= (serin() << 5);
    byte |= (serin() << 6);
    byte |= (serin() << 7);
    par = serin(); //Parity
    serin(); //Stop

    if(par != oparity(byte))
        send_packet(0xfe); //Resend

    return byte;
}


bool ps2_init_mouse(void)
{
    g_trackpoint = 0;
    g_mouse_enabled = 0;
    scrollcnt = 0;

    uint8_t d[10];

    tp_reset();

    if ( ! send_packet(0xff) )
        return false;
    d[0]=read_packet(); //Ack
    d[1]=read_packet(); //Bat
    d[2]=read_packet(); //dev ID
    //Enable Data reporting
    if ( !send_packet(0xf4) )
        return false;
    read_packet();  // Ack
    ////
    //send_packet(0xe8); //Set Resolution
    //read_packet(); //Ack
    //send_packet(0x01); //8counts/mm
    //read_packet(); //Ack
    ////
    //send_packet(0xf3); //SetSample rate
    //read_packet(); //Ack
    //send_packet(0x64); //200 smaples a second

    //Set remote mode
    if( ! send_packet(0xf0) )
        return false;
    d[3]=read_packet(); //Ack
    g_trackpoint=1;

    printf("\nTP init: %02x %02x %02x %02x", d[0],d[1],d[2],d[3]);

    tp_id();

    return true;
}

void tp_ram_toggle(uint8_t addr, uint8_t val){
    uint8_t tmp;

    tp_send_read_ack(0xe2);
    tp_send_read_ack(0x2c);
    tmp=read_packet();
    if( (tmp & val) != 0x00) {
        printf("\nAlready set");
    }

    tp_send_read_ack(0xe2);
    tp_send_read_ack(0x47);
    tp_send_read_ack(addr);
    tp_send_read_ack(val);
}

uint8_t tp_ram_read(uint8_t addr){
    tp_send_read_ack(0xe2);
    tp_send_read_ack(0x80);
    tp_send_read_ack(addr);
    return( read_packet() );
}

uint8_t tp_ram_write(uint8_t addr, uint8_t val){
    tp_send_read_ack(0xe2);
    tp_send_read_ack(0x81);
    tp_send_read_ack(addr);
    tp_send_read_ack(val);
    //return( read_packet() );
}

// TP config register: See p33 of YKT3Eext.pdf
enum { TP_PTS=0, TP_RES, TP_BTN2, TP_FLIPX, TP_FLIPY, TP_FLIPZ, TP_SWAPXY, TP_FTRANS };

bool tp_send_read_ack(uint8_t val)
{
    if( ! send_packet(val) ) {
        printf("\nError: not send");
        return false;
    }
    if(read_packet() != 0xfa) {
        printf("\nError: not ack");
        return false;
    }
    return true;
}

uint8_t tp_read_config(){
    printf("\nTP Config= ");
    tp_send_read_ack(0xe2);
    tp_send_read_ack(0x2c);
    uint8_t config = read_packet();
    printf("%02x ", config);
    return config;
}

void tp_id(void) {
    uint8_t tmp;


    // read secondary ID
    if( tp_send_read_ack(0xe1) )
        printf("\n2nd  ID: %02x%02x \nExt. ID: ", read_packet(),read_packet());
/*
    // read extended ID
    if( tp_send_read_ack(0xd0) ){
        // better scan for ")" == 29 // 41 ?
        for(uint8_t i=0; i < 31; ++i) {
            tmp=read_packet();
            printf("%c",tmp);
            if( tmp == (uint8_t)')')
                continue;
        }
    }
*/
    /* smaller TP:
     * 2nd  ID:  010e
     * Ext. ID: M 19990623($IBM3780)
     */

    // read config byte at 2C: E2 2C or E2 80 2C
    /* bit  0   1   2    3    4    5    6    7
            Pts res 2clk invX invY invZ ExXY HardTransp
      */
    //tp_read_config();
    //tp_ram_toggle(0x2c, (1<<TP_FLIPX) );
    tp_read_config();
    tp_ram_toggle(0x2c, (1<<TP_PTS) );
    tp_read_config();


    // PressToSelect toggle
    //send_packet(0xe2);send_packet(0x47);send_packet(0x2c);send_packet(0x01);

    /*
    // invertX
    send_packet(0xe2);send_packet(0x81);send_packet(0x2c);send_packet(0x03);
    */

    /* RAM locations:
     * - Read with E2 80 ADDR
     * - Read with E2 81 ADDR VAL
     *
     * 41,42,43 pts btn mask
     * 5C PtS thres
     */

    printf("\nPTS btn masks: %02x %02x %02x ", tp_ram_read(0x41), tp_ram_read(0x42), tp_ram_read(0x43) );
    tp_ram_write(0x42, 0xff);
    printf("\nPTS btn masks: %02x %02x %02x ", tp_ram_read(0x41), tp_ram_read(0x42), tp_ram_read(0x43) );
}

/*
 *
 */
void ps2_read_mouse(int *dx, int *dy, uint8_t *BTNS )
{

    uint8_t ack;
    uint8_t LMB,MMB,RMB;
    *BTNS=0;
    int mouseinf;
    {
        send_packet(0xeb);
        ack=read_packet(); //Ack
        if(ack==0xfa) {
            mouseinf=read_packet();
            LMB=0;
            MMB=0;
            RMB=0;

            if(mouseinf & 0x01)  //0x09
                LMB=1;         // Get leftbutton status
            if(mouseinf & 0x02)
                RMB=1;        // Get rightbutton status
            if(mouseinf & 0x04)
                MMB=1;       // Get middlebutton status


            /*          // emulate 3 buttons
                        if( RMB & LMB ) {
                            MMB=1;
                            RMB=LMB=0;
                        }
            */
            // *BTNS = (LMB<<2) | (MMB<<1) | (RMB << 0);
            // buttons (left to right) are  1 2 3
            // hexcode as expected is       1 4 2
            // swap lower two buttons on "blue cube"
            *BTNS = (LMB<<0) | (MMB<<1) | (RMB << 2);


            *dx= read_packet();
            *dy= read_packet();
            // raw *dx is of 0xXX
            int x = *dx;
            int y = *dy;
            int xtest = *dx;

            if(mouseinf&0x10)
                *dx=-(256-*dx);

            if(mouseinf&0x20)
                *dy=-(256-*dy);

            return;

            if(mouseinf&0x10) {
                xtest=-(256-*dx);
                *dx-=0x100; // Add sign bit to dx

            }
            if(mouseinf&0x20)
                *dy-=0x100; // Add sign bit to dy

            if( x!=0 ) {
                //printf("\n%4x %4x | %4x %4x", x,*dx,y,*dy);
                printf("\ninf=0x%04x x:%2d dx:0x%04x xtest: %d dxrevert: ", mouseinf, x,*dx, xtest);
                if(*dx&0xFF00)
                    printf("-%2d" , 256-(*dx+0x100));
                else
                    printf("+%2d" , *dx);
            }

        }
    }
}


uint8_t getMouseReport(USB_MouseReport_Data_t *MouseReport)
{
    if(!g_mouse_enabled)
        return 0;

    int16_t dx=0, dy=0;
    uint8_t btns=0;
    float factor;

#ifdef PS2MOUSE
    if(g_trackpoint) {
        ps2_read_mouse(&dx, &dy, &btns);
        /*
        if( dx != 0|| dy!=0 || btns != 0) {
            printf("\nPS/2 is %d: %2d %2d -> %d  , ", g_mouse_mode, dx, dy, dx*dx + dy*dy );
            printf(" Btns: %d / %d", btns, g_mouse_keys );
        }
        */
    }
#endif

    if( (g_mouse_keys & 0x0F) || (btns & 0x07) || (dx+dy) > 0 /* Test for spurious movements */ ) {
        if(g_mouse_mode==0) {
            g_mouse_mode=1;
            accel=0;
        }

        mouse_timer=idle_count;
        if(accel<ACC_RAMPTIME)
            accel++;

        // reset mouse mode after inactivity: idle_count is incremented 61 times per second
    } else if(idle_count-mouse_timer > 1/*seconds*/ *61 ) {
        g_mouse_mode=0;
        accel=0;
        scrollcnt=0;
    }

    if(g_mouse_mode || btns) {
        factor= 1 + accel * (ACC_MAX-1) / ACC_RAMPTIME;

#ifdef MOUSE_HAS_SCROLL_WHEELS
        MouseReport->V=0;
        MouseReport->H=0;
        MouseReport->Button=0;

        // keyboard mouse buttons only in mousemode
        if( (btns & 0x05)==0x05 || (g_mouse_keys & 0x08)) {
            int8_t sx=0, sy=0;

            if( dx!=0 ) {
                if(dx&0xFF00)
                    sx= -(256-(dx+0x100));
                else
                    sx=dx;
            }
            if( dy!=0 ) {
                if(dy&0xFF00)
                    sy= -(256-(dy+0x100));
                else
                    sy=dy;
            }

            scrollcnt = scrollcnt+abs(sy)+abs(sx);

            // limit 10 and emiting as is way to fast in windows.
            if(scrollcnt>10) {
                scrollcnt=0;
                MouseReport->X=0;
                MouseReport->Y=0;
                // only move by 1 ?!
                MouseReport->H = -sx*(factor-1)/3;
                MouseReport->V = -sy*(factor-1)/3;
            }
        } else
#endif
        {

            MouseReport->Y = dy * factor;
            MouseReport->X = -dx * factor;

            // do not emit the scroll button
            MouseReport->Button = g_mouse_keys & ~(0x08);
            MouseReport->Button |= btns;    // PS/2 buttons if set

            /*
            if(MouseReport->Button)
                printf("\nButton %d %d ",MouseReport->Button, g_mouse_keys );
            if(MouseReport->Button && (1<<0)){
                if(idle_count - g_mouse_lmb < 30) {
                    g_mouse_double=1;
                    printf("\nLMB DoubleClick");
                } else {
                    g_mouse_double=0;
                }
                g_mouse_lmb=idle_count;
            }
            else {
                g_mouse_double=0;
            }
            */
        }
        g_mouse_keys=0;
        btns=0;

        return sizeof(USB_MouseReport_Data_t);

    }
    return 0;
}
