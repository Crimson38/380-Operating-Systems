typedef void (*fsecs_test_funct)(void *);

void init_fsecs(void);
long double fsecs(fsecs_test_funct f, void *argp);
