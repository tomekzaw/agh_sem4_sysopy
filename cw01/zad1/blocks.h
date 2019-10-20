#ifndef BLOCKS_H
#define BLOCKS_H

void create_table(int);
void set_current_directory(char*);
void set_current_file(char*);
void set_current_temp_file(char*);
void search_directory(void);
int create_block(void);
void remove_block(int);
void free_table(void);

#endif