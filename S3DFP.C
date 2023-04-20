#include <dos.h>
#include <stdio.h>
#include <stdbool.h>
#include <conio.h>
#include <string.h>

#define BUFFER_SIZE 1024

// Function prototypes
void (_interrupt _far *old_interrupt_handler)();
void _interrupt _far new_interrupt_handler();
unsigned char read_vga_sequencer_register(unsigned char index);
void write_vga_sequencer_register(unsigned char index, unsigned char value);
unsigned int get_horizontal_resolution_crt();
unsigned int get_vertical_resolution_crt();
/* void set_horizontal_resolution(unsigned int horizontal_resolution); */
void set_vertical_resolution_dfp(unsigned int vertical_resolution);

// Variables
char log_buffer[BUFFER_SIZE] = {0};
int buffer_position = 0;

#pragma argsused
void _interrupt _far new_interrupt_handler() {
    static unsigned char last_video_mode = 0xFF;
    union REGS regs;

    regs.h.ah = 0x0F;
    int86(0x10, (union REGS *)&regs, (union REGS *)&regs);

    if (last_video_mode != regs.h.al) {
        last_video_mode = regs.h.al;
        buffer_position += sprintf(log_buffer + buffer_position, "Video mode changed to: 0x%02X\n", last_video_mode);
    }

    _chain_intr(old_interrupt_handler);
}

unsigned char read_vga_sequencer_register(unsigned char index) {
    outp(0x3D4, index);        // Write the index to the CRT Index Register (3D4h)
    return inp(0x3D5);         // Read the value from the CRT Data Register (3D5h)
}

void write_vga_sequencer_register(unsigned char index, unsigned char value) {
    outp(0x3C4, index);        // Write the index to the Sequencer Index Register (3C4h)
    outp(0x3C5, value);        // Write the value to the Sequencer Data Register (3C5h)
}

unsigned int get_horizontal_resolution_crt() {
    // Read CR00 (Horizontal Total)
    unsigned char horizontal_total = read_vga_sequencer_register(0x00);

    // Read CR01 (Horizontal Display Enable End)
    unsigned char horizontal_display_enable_end = read_vga_sequencer_register(0x01);

    // Calculate horizontal resolution
    unsigned int horizontal_resolution = (horizontal_display_enable_end + 1) * 8;

    return horizontal_resolution;
}

unsigned int get_vertical_resolution_crt() {
    // Read CR06 (Vertical Total, lower 8 bits)
    unsigned char vertical_total = read_vga_sequencer_register(0x06);

    // Read CR07 (Vertical Total, bits 8-9)
    unsigned char cr07 = read_vga_sequencer_register(0x07);
    unsigned char vertical_total_8 = (cr07 >> 1) & 0x01;
    unsigned char vertical_total_9 = (cr07 >> 6) & 0x01;

    // Read CR12 (Vertical Display Enable End, lower 8 bits)
    unsigned char vertical_display_enable_end = read_vga_sequencer_register(0x12);

    // Read CR07 (Vertical Display Enable End, bits 8-9)
    unsigned char vertical_display_enable_end_8 = (cr07 >> 7) & 0x01;
    unsigned char vertical_display_enable_end_9 = (cr07 >> 2) & 0x01;

    // Calculate the vertical total and vertical display enable end values
    unsigned int vertical_total_combined = (vertical_total_9 << 9) | (vertical_total_8 << 8) | vertical_total;
    unsigned int vertical_display_enable_end_combined = (vertical_display_enable_end_9 << 9) | (vertical_display_enable_end_8 << 8) | vertical_display_enable_end;

    // Calculate the active vertical resolution
    unsigned int vertical_resolution = vertical_display_enable_end_combined - vertical_total_combined + 1;

    return vertical_resolution;
}

/* void set_horizontal_resolution(unsigned int horizontal_resolution) {
    unsigned int horizontal_panel_size = horizontal_resolution / 8 - 1;

    // Set SR61 (lower 8 bits)
    unsigned char sr61_value = horizontal_panel_size & 0xFF;
    write_vga_sequencer_register(0x61, sr61_value);

    // Set SR66 (bit 8)
    unsigned char sr66_value = (horizontal_panel_size >> 8) & 0x01;
    unsigned char sr66_current_value = read_vga_sequencer_register(0x66);
    write_vga_sequencer_register(0x66, (sr66_value << 1) | (sr66_current_value & 0xFD));

    // Set SR67 (bits 10-9)
    unsigned char sr67_value = (horizontal_panel_size >> 9) & 0x03;
    unsigned char sr67_current_value = read_vga_sequencer_register(0x67);
    write_vga_sequencer_register(0x67, (sr67_value << 2) | (sr67_current_value & 0xF3));
} */



void set_vertical_resolution_dfp(unsigned int vertical_resolution) {
    // Calculate values for Sequencer registers
    unsigned int panel_size = vertical_resolution - 1;
    unsigned char sr69_value = panel_size & 0xFF;
    unsigned char sr6e_value = (panel_size >> 8) & 0x07;
    unsigned char sr6f_value = (panel_size >> 11) & 0x01;

    // Write values to the Sequencer registers
    write_vga_sequencer_register(0x69, sr69_value);
    write_vga_sequencer_register(0x6E, (read_vga_sequencer_register(0x6E) & 0xF8) | sr6e_value);
    write_vga_sequencer_register(0x6F, (read_vga_sequencer_register(0x6F) & 0xFE) | sr6f_value);
}

int main() {
    FILE *log_file;
    int key;

    old_interrupt_handler = _dos_getvect(0x1C);
    _dos_setvect(0x1C, new_interrupt_handler);

    printf("TSR active. Press any key to unload TSR.\n");
	
	unsigned int vertical_resolution = get_vertical_resolution_crt();
	set_vertical_resolution_dfp_resolution(vertical_resolution);

    while (!kbhit()) {
        /* TSR active, wait for a keypress */
    }

    key = getch(); /* Clear the key buffer */
    _dos_setvect(0x1C, old_interrupt_handler);

    log_file = fopen("mode_chg.log", "w");
    if (!log_file) {
        printf("Error: Unable to create log file.\n");
        return 1;
    }

    fwrite(log_buffer, 1, buffer_position, log_file);
    fclose(log_file);

    printf("TSR unloaded.\n");

    return 0;
}
